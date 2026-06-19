# apps/

Mini-apps. Each app is **isolated**: it includes only `app.h` and knows nothing
about the launcher or other apps. It builds its UI with LVGL (shared widgets
live in [`../ui/`](../ui/README.md)) and persists data via the shared store
(see [`../store/`](../store/README.md)).

## The contract (`src/app.h`)

```c
frij_app_t {
  name;                          // shown on the home glance
  color;                         // page background 0xRRGGBB
  build_glance(parent);          // full-screen card in the home carousel
  screen_count;                  // app's own screens (>= 1)
  build_screen(parent, index);   // build screen `index` (content only)

  action_symbol(index);          // optional: header action icon, or NULL
  on_action(index);              // optional: header action tapped
}
```

The launcher draws the shared header (back + app name + the per-screen action);
`build_screen` builds only the content below it. Leave `action_symbol`/`on_action`
NULL for no action.

## Page lifecycle (cleanup)

Attach teardown (timers, heap ctx) to the page via `LV_EVENT_DELETE` — the
launcher's carousel fires it both when a page is really deleted *and* before a
rebuild reuses the page (it also drops your old handlers so they can't stack or
double-free). Never assume a page object dies between visits.

## Add an app

1. Create `apps/<name>/<name>.cpp` (+ `.h`) that includes only `app.h` and
   exposes `const frij_app_t* <name>_app(void)`.
2. Register it in `apps.cpp` with `frij_registry_add(<name>_app())`.

That's it — the launcher reads the registry; no app names are hardcoded there.

## Settings is an app too

`apps/settings/` is an ordinary `frij_app_t`, but `apps.cpp` registers it via
`frij_registry_set_settings(...)`, so the launcher shows it in the **settings
slot** (swipe down) instead of the home carousel. Its `build_glance` is unused.

## Building blocks you can reuse

Don't hand-roll what already exists — compose these (an app includes `app.h`
plus whatever shared headers it needs):

- **Layout/UI** ([`ui/components.h`](../ui/README.md)): `frij_page` +
  `frij_page_settle` (scroll/center), `frij_surface_row`, `frij_action_row`,
  `frij_slider_row`, `frij_toggle`, `frij_check`, `frij_section_label`,
  `frij_progress_ring`, `frij_empty_state`, `frij_label`.
- **Modals/feedback**: `frij_prompt_screen` (full-screen confirm/notice — pass
  `FRIJ_DANGER` for destructive primaries), `frij_result_screen` (flow
  conclusion), `frij_action_sheet` (multi-choice), `frij_toast` /
  `frij_toast_status` (transient, optionally with a ✓/✗ glyph).
- **Motion** ([`ui/anim.h`](../ui/README.md)): `frij_stagger_in` for list
  entrances; `frij_anim_enter` for one widget.
- **Time** ([`core/datetime.h`](../core/README.md)): `frij_format_time` /
  `frij_format_relative` ("5m ago") / `frij_clock_is_24h` (respect the 24-hour
  setting — don't call `strftime` directly for clock strings).
- **Persistence** ([`store/store.h`](../store/README.md)): `frij_store_load/save`
  + typed `frij_store_load_int/bool` (+ `_save_*`), `frij_store_pull_async`.
- **Board services** ([`system/`](../system/README.md)): `frij_set_brightness`,
  `frij_haptic`, `frij_battery_*`, `frij_wifi_*`.

The launcher owns the header (back + title + per-screen action) and the Back
gesture; `build_screen` only fills the content below it.

## Color scheme

Each app picks an accent from `ui/theme.h` as its `color` and passes it to
components (checks, rings, sliders…). The page background stays Surface-1, so
every app shares the look but has its own scheme.

## Examples

- `home/` — watch face (time + date), registered first so it's the landing tile. Purple.
- `todo/` — Keep-synced checklist (single screen: tap a row to toggle, with an
  "Updated Xm ago" footer; header ↻ pulls). Glance shows a random open item +
  count. Amber.
- `events/` — countdowns to upcoming events across one or more calendars
  (read-only; the `bridge/calendar_to_frij.py` cron mirrors each `GCALENDAR_*`
  iCal feed into `store:events`). All calendars are treated alike — a holidays
  feed is just one given a gray color. 3 screens: list (Today/This week/Later
  sections, a day-count badge in each calendar's color, "Updated Xm ago" footer) /
  big-number countdown to the next event / Calendars (toggle each calendar on/off
  — persists to `store:events_off`, applies to list + glance + countdown). Glance
  + countdown show the calendar name in its color. App accent pink; per-calendar
  colors appear only on the list badges. All store/JSON logic lives in the data
  seam `packages/data/events`, so the app is pure UI. Pink.
- `stopwatch/` — MM:SS.cs stopwatch with Start/Stop, laps, and reset; timing
  lives in module state so it keeps running off-screen. Green.
- `scoreboard/` — two-player score keeper (game-night companion). Full-bleed
  left/right split of two transparent touch halves (Farzin/Farah) with a center
  divider + "VS", no chrome: tap a half +1 / hold -1; header reset (confirm);
  scores persist + sync via the store (`sb_a`/`sb_b`). Uses `frij_page_full_bleed`.
  Glance shows the line + who leads. Blue.
- `assistant/` — Frij AI: push-to-talk to a real cloud backend (the Supabase
  `ask` edge function + Gemini — see [../../docs/AI.md](../../docs/AI.md); the
  device records the mic, the emulator sends a sample question, falling back to
  canned answers only when the cloud is unconfigured). Overlay driven by the
  input layer (`frij_assistant_ptt`): listening (voice bars + pulse rings) /
  thinking (dots) / answer; screen 0 lists the last 5 questions + answers. Violet.
- `couples/` — a gentle "fight tracker" for two. Screen 0 asks "Did we fight
  today?" — No does nothing, Yes logs the day (then flips to a logged state with
  Undo). Screen 1 is stats: a peace-streak hero, a 7-day dot strip, and this
  week / this month / total counts; the header clears history (confirm). Glance
  shows days since the last fight. One entry per logged day (a local-midnight day
  index) in a JSON array under `couples_fights`; persists + syncs. Pink.
- `settings/` — 3 screens: General (Display: brightness/sleep/raise-to-wake;
  Sound: volume/touch-sounds; Preferences: 24-hour/vibration/animations/auto-sync), Network
  (Wi-Fi list + connect/disconnect/forget), About (battery, last sync, sync now,
  reset, erase all data). Hardware-backed toggles (raise-to-wake → BMI270,
  touch-sounds → ES8311) are stored now, wired on device later. Purple.
  Registered in the settings slot (swipe down), not the home carousel.
