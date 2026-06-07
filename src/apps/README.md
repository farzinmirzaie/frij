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
  build_screen(parent, index);   // build screen `index`
}
```

## Add an app

1. Create `apps/<name>/<name>.cpp` (+ `.h`) that includes only `app.h` and
   exposes `const frij_app_t* <name>_app(void)`.
2. Register it in `apps.cpp` with `frij_registry_add(<name>_app())`.

That's it — the launcher reads the registry; no app names are hardcoded there.

## Settings is an app too

`apps/settings/` is an ordinary `frij_app_t`, but `apps.cpp` registers it via
`frij_registry_set_settings(...)`, so the launcher shows it in the **settings
slot** (swipe down) instead of the home carousel. Its `build_glance` is unused.

## Examples

- `todo/` — a checklist; list is JSON synced to the cloud (3 screens).
- `counter/` — a number with ±; value persists (1 screen).
- `settings/` — device settings (2 screens).
