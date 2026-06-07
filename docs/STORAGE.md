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

## Phasing

1. **Local store:** API + emulator file backend. ✅
2. **Emulator ↔ Supabase:** libcurl backend, `.env` config, file cache. ✅
   (Requires the `store` table — see Setup.)
3. **Device + cloud:** Supabase over `WiFiClientSecure`, NVS/LittleFS cache,
   async + offline queue.
4. **Web app:** reads/writes the same rows via the Supabase JS client.

## Things to get right later (device phase)

- **TLS:** `WiFiClientSecure` (~40–50 KB/conn; fine with PSRAM). Root CA or `setInsecure()`.
- **Secrets:** only a *scoped* key on device (Supabase anon key + row-level
  security). Never an admin key — firmware can be dumped.
- **Offline:** queue writes locally, flush on reconnect.
- **Conflicts:** start with last-write-wins per blob; refine if needed.
