#!/usr/bin/env python3
"""Sync a shared Google Keep checklist into the Frij store.

Direction: **read-only** — Google Keep -> Supabase. It reads the Keep list named
`GKEEP_LIST_TITLE` and upserts it as Frij's `todo` row, in the exact JSON the
device already understands ([{"t": <text>, "d": <done>}, ...]). The Frij device
needs no change: its todo app pulls the same row from Supabase.

Keep has no official consumer API, so this uses the unofficial `gkeepapi`
(reverse-engineered) — see bridge/README.md for setup + the master-token steps.

Usage:
    python3 keep_to_frij.py            # read Keep, upsert Supabase
    python3 keep_to_frij.py --dry-run  # read Keep, print JSON, no write
"""
import datetime
import json
import os
import re
import sys
import urllib.error
import urllib.request

MAX_ITEMS = 16  # matches the device's todo cap (src/apps/todo/todo.cpp MAX_ITEMS)
TEXT_MAX = 39   # matches the device's TEXT_LEN - 1

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


def fetch_keep_items(email, master_token, title):
    """Return [(text, checked), ...] from the Keep list titled `title`."""
    import gkeepapi  # lazy: keeps to_todo_json() importable without the dep

    keep = gkeepapi.Keep()
    keep.resume(email, master_token)  # resume = auth with a master token (all versions)

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

    # Unchecked first (active to-dos), then checked — mirrors Keep's own layout.
    return [(it.text, False) for it in note.unchecked] + \
           [(it.text, True) for it in note.checked]


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


def main():
    dry_run = "--dry-run" in sys.argv
    load_dotenv()

    items = fetch_keep_items(
        env("GKEEP_EMAIL", required=True),
        env("GKEEP_MASTER_TOKEN", required=True),
        env("GKEEP_LIST_TITLE", required=True),
    )
    value = to_todo_json(items)
    print(json.dumps(value, ensure_ascii=False))

    if dry_run:
        print("dry-run: not writing to Supabase", file=sys.stderr)
        return

    status = upsert_supabase(
        env("SUPABASE_URL", required=True),
        env("SUPABASE_ANON_KEY", required=True),
        env("SUPABASE_TABLE", "store"),
        env("FRIJ_STORE_KEY", "todo"),
        value,
    )
    print(f"upserted {len(value)} item(s) -> store:{env('FRIJ_STORE_KEY', 'todo')} (HTTP {status})",
          file=sys.stderr)


if __name__ == "__main__":
    main()
