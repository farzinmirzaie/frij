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

## Add an app

1. Create `apps/<name>/<name>.cpp` (+ `.h`) that includes only `app.h` and
   exposes `const frij_app_t* <name>_app(void)`.
2. Register it in `apps.cpp` with `frij_registry_add(<name>_app())`.

That's it — the launcher reads the registry; no app names are hardcoded there.

## Settings is an app too

`apps/settings/` is an ordinary `frij_app_t`, but `apps.cpp` registers it via
`frij_registry_set_settings(...)`, so the launcher shows it in the **settings
slot** (swipe down) instead of the home carousel. Its `build_glance` is unused.

## Color scheme

Each app picks an accent from `ui/theme.h` as its `color` and passes it to
components (checks, rings, sliders…). The page background stays Surface-1, so
every app shares the look but has its own scheme.

## Examples

- `home/` — watch face (time + date), registered first so it's the landing tile. Purple.
- `todo/` — cloud-synced checklist, 3 screens. Amber.
- `counter/` — a number with ±, persists. Blue.
- `settings/` — 3 screens: General (Display: brightness/sleep/raise-to-wake;
  Sound: volume/touch-sounds; Preferences: 24-hour/vibration/auto-sync), Network
  (Wi-Fi list + connect/disconnect/forget), About (battery, last sync, sync now,
  reset, erase all data). Hardware-backed toggles (raise-to-wake → BMI270,
  touch-sounds → ES8311) are stored now, wired on device later. Purple.
  Registered in the settings slot (swipe down), not the home carousel.
