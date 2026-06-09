# bridge/ — Google Keep → Frij sync

Mirrors a **shared Google Keep checklist** into Frij's todo list.

Keep has no official consumer API, so this uses the unofficial
[`gkeepapi`](https://github.com/kiwiz/gkeepapi). It runs **off-device** (GitHub
Actions cron) and writes into the same Supabase row the Frij device already
pulls — so the device needs **no change** for read-only.

```
Google Keep  ──(gkeepapi, read)──>  keep_to_frij.py  ──(REST upsert)──>  Supabase store:todo  ──>  Frij device
```

Direction today: **read-only** (Keep → device). Writing back (device/edits →
Keep) is a later phase — see "Phase 2" below.

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

## On-demand trigger (the Todo refresh button)

The device's Todo refresh button can kick a Keep→cloud sync itself instead of
waiting for the cron. Run the bridge as a tiny HTTP server on an **always-on
host reachable from the device's Wi-Fi** (a Pi, a NAS, a small VM):

```bash
# on the host (creds in its .env, same keys as above)
python3 keep_to_frij.py --serve 8765
# optional: require a token — set KEEP_SYNC_TOKEN in the env, then callers add ?token=...
```

`GET /sync` runs the sync synchronously and returns `{"ok":true,"items":N}`.
Point the device at it:

- **Emulator:** `KEEP_SYNC_URL=http://<host>:8765/sync` in the repo `.env`.
- **Device:** build flag `-D FRIJ_KEEP_SYNC_URL='"http://<host>:8765/sync"'`.

The device's refresh then does: `GET /sync` (Keep→cloud) → pull the row → redraw.
Run the server under a process manager (systemd / `pm2` / a `launchd` plist) so
it survives reboots.

## Test (no credentials)

```bash
python3 test_mapping.py     # verifies the Keep-items → device-JSON mapping
```

## Notes / limits

- Items map to the device shape `{"t": <text>, "d": <done>}`; text is trimmed to
  39 chars and the list is capped at 16 items (the device's limits) — a cap is
  logged, never silent.
- Unofficial API: it can break if Google changes things, and the token may need
  re-minting. That risk is isolated here, off the device.
- **Phase 2 (write-back):** once the todo app has on-device add/edit, the bridge
  can push changes back with `list.add(...)` + `keep.sync()`. Two-way needs a
  merge rule (last-write-wins per item, or treat Keep as source of truth).
