#!/usr/bin/env python3
"""Two-way sync between shared Google Keep checklists and the Frij store.

Each note is declared as a `GKEEP_NOTE_*` env var, value `storeKey,noteTitle`
(split on the first comma, so titles may contain commas):

    GKEEP_NOTE_TODO=todo,Todos
    GKEEP_NOTE_GROCERIES=groceries,Groceries

The bridge syncs each Keep note titled `noteTitle` to `store:<storeKey>`, so
different device apps can hook into different keys (todo→`todo`, …).

Keep owns each list's *structure* (which items exist + their text — the watch can
only toggle, not add/remove). The done-state syncs **both ways**: a 3-way merge
against a saved base (`<key>_base`) decides per item which side changed since the
last sync; if both changed differently, *checked wins*. Watch toggles are written
back to Keep; the merged list (`<key>`) is what the device reads.

Keep has no official consumer API, so this uses the unofficial `gkeepapi`
(reverse-engineered) — see bridge/README.md for setup + the master-token steps.
It's meant to run on the GitHub Actions cron (~10 min) — sync is not realtime.
A missing/failed note warns and is skipped; the others still sync.

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
    """Load the repo-root .env (Supabase creds, reused from the device) and an
    optional bridge-local .env (GKEEP_*/GCALENDAR_* values), without adding a
    dependency. Real environment variables (e.g. GitHub Actions secrets) win.
    This file lives at src/packages/bridge/, so the repo root is three up."""
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.abspath(os.path.join(here, "..", "..", ".."))
    for path in (os.path.join(root, ".env"), os.path.join(here, ".env")):
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


def parse_notes():
    """Read every GKEEP_NOTE_* env var into [{key, title}], sorted by env-key name
    (stable across runs). Value is `storeKey,noteTitle` (split on the first comma,
    so titles may contain commas). Exits if none are configured."""
    notes = []
    for env_key in sorted(os.environ):
        if not env_key.startswith("GKEEP_NOTE_"):
            continue
        parts = os.environ[env_key].split(",", 1)
        key = parts[0].strip()
        title = parts[1].strip() if len(parts) > 1 else ""
        if not key or not title:
            print(f"warning: {env_key} must be 'storeKey,noteTitle', skipping", file=sys.stderr)
            continue
        notes.append({"key": key, "title": title})
    if not notes:
        sys.exit("error: no GKEEP_NOTE_* notes configured (see bridge/README.md)")
    return notes


def keep_login(email, master_token):
    """Authenticate once and return the gkeepapi.Keep client."""
    import gkeepapi  # lazy: keeps to_todo_json() importable without the dep

    keep = gkeepapi.Keep()
    # `authenticate` on newer gkeepapi, `resume` on 0.14.x (Python 3.9) — both take
    # (email, master_token). Prefer authenticate to avoid the rename deprecation warning.
    auth = getattr(keep, "authenticate", None) or keep.resume
    auth(email, master_token)
    return keep


def find_note(keep, title):
    """The visible Keep list titled `title` (a gkeepapi List), or None."""
    import gkeepapi

    def is_target(note):
        return (
            isinstance(note, gkeepapi.node.List)
            and note.title == title
            and not note.trashed
            and not note.archived
        )

    return next((n for n in keep.find(func=is_target)), None)


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


def sync_note(note, key, url, anon, table, write=True):
    """Two-way sync of one Keep note's done-state with store:<key>.

    Keep owns the *structure* (which items exist, their text — the watch can only
    toggle, not add/remove). For each item's done-state we 3-way merge against a
    saved base (last sync): the side that changed since the base wins; if both
    changed differently, *checked wins*. Toggles the watch made are written onto
    `note` (the caller batches one keep.sync()); the merged list + a fresh base
    are written to Supabase. Returns (merged_list, keep_changed).
    """
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
        upsert_supabase(url, anon, table, key, merged)        # device-facing list
        upsert_supabase(url, anon, table, base_key, merged)   # new merge base
    return merged, keep_changed


def main():
    load_dotenv()
    write = "--dry-run" not in sys.argv

    notes = parse_notes()
    url = env("SUPABASE_URL", required=True)
    anon = env("SUPABASE_ANON_KEY", required=True)
    table = env("SUPABASE_TABLE") or "store"
    keep = keep_login(env("GKEEP_EMAIL", required=True), env("GKEEP_MASTER_TOKEN", required=True))

    any_keep_changed = False
    for n in notes:
        note = find_note(keep, n["title"])
        if note is None:  # one missing note must not block the rest
            print(f'warning: no Keep list titled "{n["title"]}" (key {n["key"]}); skipping',
                  file=sys.stderr)
            continue
        merged, keep_changed = sync_note(note, n["key"], url, anon, table, write=write)
        any_keep_changed = any_keep_changed or keep_changed
        print(json.dumps({"key": n["key"], "items": merged}, ensure_ascii=False))
        verb = "would sync" if not write else "synced"
        print(f"{verb} {len(merged)} item(s) <-> {table}:{n['key']}; Keep change: {keep_changed}",
              file=sys.stderr)

    if write and any_keep_changed:
        keep.sync()  # push every note's write-backs in one round


if __name__ == "__main__":
    main()
