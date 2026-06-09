# bridge/ — Google Keep ↔ Frij sync

Keeps a **shared Google Keep checklist** and Frij's todo list in sync.

Keep has no official consumer API, so this uses the unofficial
[`gkeepapi`](https://github.com/kiwiz/gkeepapi). It runs **off-device** (GitHub
Actions cron) against the same Supabase row the device reads/writes.

```
Google Keep  ⇄ (gkeepapi)  keep_to_frij.py  ⇄ (REST)  Supabase store:todo  ⇄  Frij device
```

**Two-way, done-state only.** Keep owns the structure — which items exist and
their text — because the watch can only *toggle* (no on-device add/remove yet;
voice-add is planned). Checking/unchecking syncs **both directions**:

- A 3-way merge against a saved base row (`store:todo_base`) decides, per item,
  which side moved since the last sync. If both moved differently, **checked
  wins**. Watch toggles are written back to Keep; the merged list lands in
  `store:todo` for the device.
- Items are matched by their cleaned/capped text (same transform the device
  stores), so toggles map back to the right Keep item.

Not realtime — it syncs on the ~10-min cron (each direction lands within a cron
cycle + the device's next pull). Add/remove still happen in the Keep app.

## What you need to provide

1. **Supabase** — already configured in the repo-root `.env` (the device uses
   it); the bridge **reuses those values automatically**. Just make sure the
   `store` table exists (SQL in [`../docs/STORAGE.md`](../docs/STORAGE.md)).
2. The **exact title** of your shared Keep list note (e.g. `Todos`).
3. A Google **master token** for the account that can see the list (below).
4. Put the Keep values where the bridge runs:
   - **Local run**: append to the repo-root `.env` (Supabase is read from there):
     ```
     GKEEP_EMAIL=you@gmail.com
     GKEEP_MASTER_TOKEN=...        # from get_master_token.py
     GKEEP_LIST_TITLE=Todos
     ```
   - **GitHub Actions** (default): repo → Settings → Secrets and variables →
     Actions → add `GKEEP_EMAIL`, `GKEEP_MASTER_TOKEN`, `GKEEP_LIST_TITLE`,
     `SUPABASE_URL`, `SUPABASE_ANON_KEY`, `SUPABASE_TABLE` (CI has no `.env`).

## Get the master token (one time)

```bash
python3 -m pip install --user -r requirements.txt   # `pip` may not be on PATH
python3 get_master_token.py
```

Tip: a virtualenv avoids the `--user` dance:
`python3 -m venv .venv && . .venv/bin/activate && pip install -r requirements.txt`.

It prints the steps: sign in to the account in an incognito window, visit
`https://accounts.google.com/EmbeddedSetup`, copy the `oauth_token` cookie
(`oauth2_4/...`), paste it in. The script prints `GKEEP_MASTER_TOKEN` and
`GKEEP_DEVICE_ID` — store both as secrets. **The master token is as sensitive as
the account password; never commit it.**

## Run it

```bash
python3 keep_to_frij.py --dry-run   # read Keep, print the JSON, write nothing
python3 keep_to_frij.py             # read Keep, upsert Supabase store:todo
```

The GitHub Actions workflow ([`../.github/workflows/keep-sync.yml`](../.github/workflows/keep-sync.yml))
runs it every ~10 min and on demand (Actions tab → Run workflow).

## Test (no credentials)

```bash
python3 test_mapping.py     # verifies the Keep-items → device-JSON mapping
```

## Notes / limits

- Items map to the device shape `{"t": <text>, "d": <done>}`; text is trimmed to
  39 chars and the list is capped at 16 items (the device's limits) — a cap is
  logged, never silent. Emoji are stripped (the device font has none).
- Unofficial API: it can break if Google changes things, and the token may need
  re-minting. That risk is isolated here, off the device.
- **Done-state is two-way; add/remove is not.** The watch can't add or delete
  items yet, so those only happen in Keep. When on-device add lands (voice), the
  bridge would `list.add(...)` + `keep.sync()` to push new items back too.
