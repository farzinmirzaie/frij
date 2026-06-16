# Storage & cloud sync

Apps share one small key→value store so their data can also live in the cloud
and be read by a future web app (a Google-Keep-style view of the same data).

The store API and backends are documented in
[../src/store/README.md](../src/store/README.md). This doc covers the **cloud
setup** (Supabase) and the longer-term plan.

## Chosen cloud backend: Supabase

Free Postgres + REST + a JS client/realtime — so the ESP32 writes rows over
REST and a web app reads the same rows.

### Setup

1. Create a Supabase project. Copy `.env.example` to `.env` and fill in
   `SUPABASE_URL` + `SUPABASE_ANON_KEY` (and `SUPABASE_TABLE`, default `store`).
   `.env` is gitignored.
2. Create the table (SQL editor, or via the Supabase MCP):
   ```sql
   create table store (
     key        text primary key,
     value      jsonb,
     updated_at timestamptz default now()
   );
   alter table store enable row level security;
   -- permissive to start (hobby); tighten later
   create policy "anon all" on store for all to anon
     using (true) with check (true);
   ```
3. Optional tooling: add the official **Supabase MCP** to Claude Code:
   `claude mcp add --transport http supabase https://mcp.supabase.com/mcp`
   then `/mcp` to authenticate. Lets the agent run SQL / inspect schema.

The anon key + RLS is what the device and web app use; the `service_role` key is
admin-only and must never ship on the device.

## Google Keep sync (checklists)

Checklists can be mirrored from **shared Google Keep notes**. Keep has no
official consumer API, so a small **off-device bridge**
([`../src/packages/bridge/`](../src/packages/bridge/README.md)) reads the notes
with the unofficial `gkeepapi` and **upserts the matching `store:<key>` rows**
the device pulls. Each note is declared as a `GKEEP_NOTE_<ID>=<storeKey>,<title>`
secret, so one account can feed several apps (todo → `store:todo`, …):

```
Google Keep ──(read)──> bridge (GitHub Actions cron) ──(REST upsert)──> store:<key> ──> device
```

The device just reads/writes each `<key>` row; the bridge does the Keep side, so
no firmware change is needed. You provide a Supabase project (this table), a
Google master token, and one `GKEEP_NOTE_*` per note (as GitHub Actions secrets).
Setup + token steps: [`../src/packages/bridge/README.md`](../src/packages/bridge/README.md).

**Done-state is two-way** (check/uncheck syncs Keep⇄device via a 3-way merge with
a per-note `store:<key>_base` base row; checked-wins on conflict). **Add/remove
stay in Keep** — the watch can only toggle for now (voice-add later). Not
realtime: it rides the ~10-min cron.

## Phasing

1. **Local store:** API + emulator file backend. ✅
2. **Emulator ↔ Supabase:** libcurl backend, `.env` config, file cache. ✅
   (Requires the `store` table — see Setup.)
3. **Device + cloud:** Supabase over `WiFiClientSecure`, LittleFS file cache, on
   a serialized background worker. ✅ (Offline replay queue still TODO — below.)
4. **Web app:** reads/writes the same rows via the Supabase JS client.

## Things to get right later (device phase)

- **TLS:** `WiFiClientSecure` (~40–50 KB/conn; fine with PSRAM). Root CA or `setInsecure()`.
- **Secrets:** only a *scoped* key on device (Supabase anon key + row-level
  security). Never an admin key — firmware can be dumped.
- **Offline:** queue writes locally, flush on reconnect.
- **Conflicts:** start with last-write-wins per blob; refine if needed.
