# Frij

A small launcher with mini-apps for round LVGL touch displays. The launcher draws
a tile per app; tap to open, with a back button to return. Apps are self-contained
and don't know about each other or the launcher — adding one is a new folder plus
a single line.

Built with [LVGL](https://github.com/lvgl/lvgl) v9 and
[M5GFX](https://github.com/m5stack/M5GFX). Forked from
[m5stack/lv_m5_emulator](https://github.com/m5stack/lv_m5_emulator).

## Build & run

Runs on your computer via SDL2 — no hardware required (Apple Silicon macOS):

```sh
pio run -e emulator_Dial
```

In the emulator window: **1–6** zoom, **L** / **R** rotate.

First-time SDL2 / PlatformIO setup: see the upstream
[lv_m5_emulator README](https://github.com/m5stack/lv_m5_emulator#readme).

## Adding an app

1. Create `src/apps/<name>/` exposing `const frij_app_t* <name>_app(void)`
   (include only `app.h`).
2. Register it with one line in `src/apps/apps.cpp`.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the full model.

## Layout

- `src/apps/` — the mini-apps; `src/apps/apps.cpp` registers them.
- `src/launcher/` — home screen and navigation.
- `src/utility/` — the only board-specific code (LVGL ↔ M5GFX).
- `docs/` — [architecture](docs/ARCHITECTURE.md), [hardware](docs/HARDWARE.md),
  [roadmap](docs/ROADMAP.md).

## License

MIT — see [LICENSE](LICENSE).
