# Frij AI — the assistant pipeline

```
hold Key B ──► system/ai ──HTTPS──► Supabase Edge Function "ask" ──► Gemini (free tier)
 (emulator:                          │  GEMINI_API_KEY lives here       │ tools
  Space + a sample                   │  service-role store access       ▼
  question stands in                 └──◄── {"q": ..., "a": ...} ◄── get_events/get_todos/
  for the mic)                                                       add_todo/add_point
```

- **Function**: [`src/packages/supabase/functions/ask/index.ts`](../src/packages/supabase/functions/ask/index.ts),
  deployed to the same project as the store. Accepts `{"q": "<text>"}` now and
  `{"audio": "<base64>", "mime": ...}` for the device's mic later (Gemini takes
  audio natively — no separate STT step).
- **Tools** the model can call: read events/todos, queue a new todo in
  `store:todo_inbox` (the Keep bridge owns list structure, so adds go through
  an inbox; bridge pickup is a follow-up), bump the scoreboard.
- **Emulator side**: `src/packages/system/ai.*` — worker-thread text POST (libcurl,
  reusing the store's `.env` config). A random sample question stands in for
  the mic. No cloud → canned answers.
- **Device side**: `src/packages/system/ai.*` (the `#else` branch) — hold Key B (blue)
  to record from the ES8311 mic (M5.Mic, 16 kHz mono, ≤12s) on a FreeRTOS
  task; release wraps it as a WAV, base64-encodes it (PSRAM), and POSTs it to
  the same `ask` function over `WiFiClientSecure` (Gemini transcribes +
  answers — no separate STT). Cloud config is baked in at build time (below).
  Compile-only so far — verify on flash.

## Device build config

The on-device AI needs the Supabase project URL + anon key. The `device` env
pulls them from the shell at build time (same values as the repo `.env`):

```bash
set -a; source .env; set +a        # exports SUPABASE_URL + SUPABASE_ANON_KEY
pio run -e device                   # bakes them into FRIJ_SUPABASE_* macros
```

Unset → builds without cloud and the assistant falls back to the mock. The
GEMINI_API_KEY still lives only in the edge function — never on the device.

## Setup (one time)

1. Create a free key at https://aistudio.google.com (Gemini free tier).
2. `supabase secrets set GEMINI_API_KEY=<key>` (or Dashboard ▸ Edge Functions ▸
   ask ▸ Secrets). Optional: `GEMINI_MODEL` (default `gemini-2.5-flash`).
3. Done — the emulator's push-to-talk now gets real answers.

## Test from a terminal

```bash
set -a; source .env; set +a
curl -s -X POST "$SUPABASE_URL/functions/v1/ask" \
  -H "apikey: $SUPABASE_ANON_KEY" -H "Authorization: Bearer $SUPABASE_ANON_KEY" \
  -H "Content-Type: application/json" -d '{"q":"what is on the calendar this week?"}'
```

Free-tier limits (Gemini Flash: ~15 req/min, hundreds/day) are far above
fridge usage; the function returns a clean error the UI shows if anything
fails.
