#!/usr/bin/env python3
"""Two-way sync between a shared Google Keep checklist and the Frij store.

Keep owns the list *structure* (which items exist + their text — the watch can
only toggle, not add/remove). The done-state syncs **both ways**: a 3-way merge
against a saved base (`<key>_base`) decides per item which side changed since the
last sync; if both changed differently, *checked wins*. Watch toggles are written
back to Keep; the merged list (`<key>`) is what the device reads.

Keep has no official consumer API, so this uses the unofficial `gkeepapi`
(reverse-engineered) — see bridge/README.md for setup + the master-token steps.
It's meant to run on the GitHub Actions cron (~10 min) — sync is not realtime.

Usage:
    python3 keep_to_frij.py            # 2-way sync (merge + write back)
    python3 keep_to_frij.py --dry-run  # show the merged result, write nothing
"""
import datetime
import json
import os
import re
import sys
import urllib.error
import urllib.request

MAX_ITEMS = 16  # matches the device's todo cap (src/apps/todo/todo.cpp MAX_ITEMS)
TEXT_MAX = 63   # matches the device's TEXT_LEN - 1 (the list ellipsizes; glance wraps full)

# The device font (SF Pro subset) has no emoji glyphs, so they render as tofu
# boxes. Strip emoji / pictographs / dingbats / variation selectors.
_EMOJI = re.compile(
    "["
    "\U0001F000-\U0001FAFF"  # emoji & pictographs (incl. flags, skin tones)
    "\U00002600-\U000027BF"  # misc symbols + dingbats
    "\U00002B00-\U00002BFF"  # misc symbols & arrows
    "\U0000FE00-\U0000FE0F"  # variation selectors
    "\U0000200D"             # zero-width joiner
    "]+"
)


def clean_text(text):
    """Drop emoji the device can't render, collapse the leftover spaces."""
    return re.sub(r"\s{2,}", " ", _EMOJI.sub("", text or "")).strip()


def load_dotenv():
    """Load the repo's .env (Supabase creds, reused from the device) and an
    optional bridge/.env (the GKEEP_* values), without adding a dependency.
    Real environment variables (e.g. GitHub Actions secrets) always win."""
    here = os.path.dirname(os.path.abspath(__file__))
    for path in (os.path.join(here, "..", ".env"), os.path.join(here, ".env")):
        try:
            with open(path) as f:
                for line in f:
                    line = line.strip()
                    if not line or line.startswith("#") or "=" not in line:
                        continue
                    k, v = line.split("=", 1)
                    os.environ.setdefault(k.strip(), v.strip())
        except FileNotFoundError:
            pass


def env(name, default=None, required=False):
    value = os.environ.get(name, default)
    if required and not value:
        sys.exit(f"error: missing required env var {name}")
    return value


def fetch_keep_note(email, master_token, title):
    """Return (keep, note) for the Keep list titled `title` (note = a gkeepapi List)."""
    import gkeepapi  # lazy: keeps to_todo_json() importable without the dep

    keep = gkeepapi.Keep()
    # `authenticate` on newer gkeepapi, `resume` on 0.14.x (Python 3.9) — both take
    # (email, master_token). Prefer authenticate to avoid the rename deprecation warning.
    auth = getattr(keep, "authenticate", None) or keep.resume
    auth(email, master_token)

    def is_target(note):
        return (
            isinstance(note, gkeepapi.node.List)
            and note.title == title
            and not note.trashed
            and not note.archived
        )

    note = next((n for n in keep.find(func=is_target)), None)
    if note is None:
        sys.exit(f'error: no Keep list titled "{title}" visible to {email}')
    return keep, note


def item_key(text):
    """The match key for a Keep item: the same cleaned/capped text the device
    stores (so device toggles map back to the right Keep item)."""
    return clean_text(text)[:TEXT_MAX]


def to_todo_json(items):
    """Map [(text, checked)] -> the device's todo JSON, trimmed + capped."""
    out = []
    for text, checked in items:
        text = clean_text(text)
        if not text:
            continue
        out.append({"t": text[:TEXT_MAX], "d": bool(checked)})
    if len(out) > MAX_ITEMS:
        print(f"note: {len(out)} items, capping to {MAX_ITEMS} (device limit)", file=sys.stderr)
        out = out[:MAX_ITEMS]
    return out


def upsert_supabase(url, anon_key, table, store_key, value):
    """Upsert {key, value, updated_at} into the Supabase `store` table over REST."""
    endpoint = f"{url.rstrip('/')}/rest/v1/{table}?on_conflict=key"
    body = json.dumps({
        "key": store_key,
        "value": value,
        "updated_at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
    }).encode()
    req = urllib.request.Request(endpoint, data=body, method="POST", headers={
        "apikey": anon_key,
        "Authorization": f"Bearer {anon_key}",
        "Content-Type": "application/json",
        "Prefer": "resolution=merge-duplicates,return=minimal",
    })
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            return resp.status
    except urllib.error.HTTPError as e:
        sys.exit(f"error: Supabase upsert failed: {e.code} {e.read().decode(errors='replace')}")


def read_supabase_value(url, anon_key, table, key):
    """GET a store row's `value` (a list), or [] if absent."""
    endpoint = f"{url.rstrip('/')}/rest/v1/{table}?key=eq.{key}&select=value"
    req = urllib.request.Request(endpoint, headers={
        "apikey": anon_key,
        "Authorization": f"Bearer {anon_key}",
    })
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            rows = json.loads(resp.read().decode())
    except urllib.error.HTTPError as e:
        sys.exit(f"error: Supabase read failed: {e.code} {e.read().decode(errors='replace')}")
    if rows and isinstance(rows[0].get("value"), list):
        return rows[0]["value"]
    return []


def run_sync(write=True):
    """Two-way sync of the done-state between Keep and the device.

    Keep owns the *structure* (which items exist, their text — the watch can only
    toggle, not add/remove). For each item's done-state we 3-way merge against a
    saved base (last sync): the side that changed since the base wins; if both
    changed differently, *checked wins*. Changes the watch made are written back
    to Keep; the merged list + a fresh base are written to Supabase.

    Returns (merged_list, table, store_key, keep_changed).
    """
    keep, note = fetch_keep_note(
        env("GKEEP_EMAIL", required=True),
        env("GKEEP_MASTER_TOKEN", required=True),
        env("GKEEP_LIST_TITLE", required=True),
    )
    url = env("SUPABASE_URL", required=True)
    anon = env("SUPABASE_ANON_KEY", required=True)
    table = env("SUPABASE_TABLE") or "store"
    key = env("FRIJ_STORE_KEY") or "todo"
    base_key = key + "_base"

    device = {it["t"]: bool(it["d"]) for it in read_supabase_value(url, anon, table, key) if "t" in it}
    base = {it["t"]: bool(it["d"]) for it in read_supabase_value(url, anon, table, base_key) if "t" in it}

    merged = []
    keep_changed = False
    seen = set()
    for item in note.items:  # Keep order; Keep owns which items exist
        text = item_key(item.text)
        if not text or text in seen:
            continue
        seen.add(text)
        keep_done = bool(item.checked)
        base_done = base.get(text, keep_done)      # first sight -> no change vs Keep
        dev_done = device.get(text, base_done)     # device may have toggled it
        dev_moved = dev_done != base_done
        keep_moved = keep_done != base_done
        if dev_moved and keep_moved:
            m = dev_done or keep_done               # tie -> checked wins
        elif dev_moved:
            m = dev_done
        else:
            m = keep_done                           # keep_moved or no change
        if m != keep_done:
            item.checked = m                        # write the watch's toggle back to Keep
            keep_changed = True
        merged.append({"t": text, "d": m})

    if len(merged) > MAX_ITEMS:
        print(f"note: {len(merged)} items, capping to {MAX_ITEMS} (device limit)", file=sys.stderr)
        merged = merged[:MAX_ITEMS]

    if write:
        if keep_changed:
            keep.sync()  # push the toggles to Keep
        upsert_supabase(url, anon, table, key, merged)        # device-facing list
        upsert_supabase(url, anon, table, base_key, merged)   # new merge base
    return merged, table, key, keep_changed


def main():
    load_dotenv()
    write = "--dry-run" not in sys.argv

    merged, table, key, keep_changed = run_sync(write=write)
    print(json.dumps(merged, ensure_ascii=False))
    if not write:
        print(f"dry-run: would sync {len(merged)} items; Keep write-back: {keep_changed}",
              file=sys.stderr)
    else:
        print(f"synced {len(merged)} item(s) <-> {table}:{key}; Keep updated: {keep_changed}",
              file=sys.stderr)


if __name__ == "__main__":
    main()
