# data/ — the app ⇄ services seam

Per-app **data/view providers**. Each one reads the shared store (and, where
needed, board services), does all the parsing / domain math / formatting, and
hands the app **plain, display-ready structs**. This is what keeps apps pure UI.

```
store:<key> (JSON)  ──>  packages/data/<app>  ──>  view structs  ──>  apps/<app> (LVGL)
```

## Why it exists

The [layering rule](../../../CLAUDE.md) lets an app include only `app.h`,
`packages/ui`, `packages/core`, and `packages/data` — **never** `store`,
`system`, `platform`, or the off-device packages. So anything an app needs from
those lower layers comes through a `data/` provider:

- the provider knows the store key + JSON shape; the app never does
- it owns the date/number/string formatting (via `core/`), so the app just lays
  out strings
- it can read board services (`system/`) and hand the app a plain value

Swapping a data source (file ⇄ cloud, real ⇄ mock) touches only this layer.

## Providers

- **`events`** — `store:events` (written by `bridge/calendar_to_frij.py`) into
  upcoming-event view structs: day math, unit-scaled badges ("3d"/"2w"), 24-hour
  time formatting, per-calendar colors, and the on-device show/hide set
  (`store:events_off`). See [`events.h`](events.h) for the API
  (`frij_events_load` / `_next` / `_calendars` / `_set_calendar` / `_synced_ago`
  / `_sync`). Consumed by [`apps/events`](../../apps/events).

## Adding a provider

1. `data/<app>.h` — pure C structs + functions, **no LVGL**.
2. `data/<app>.cpp` — include `store/`, `core/`, ArduinoJson as needed; return
   the structs. Keep it UI-agnostic and (for the UI path) non-blocking — kick
   cloud refreshes with `frij_store_pull_async` and read the cache.

(Pilot: Events. Other apps still read the store directly and will migrate here.)
