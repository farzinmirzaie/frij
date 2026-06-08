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
pio run -e emulator_Dial        # quick 240px round emulator (M5GFX SDL)
pio run -e emulator_StopWatch    # 466x466 round, device resolution (LVGL SDL)
```

Navigate: **drag** to swipe (left/right between apps, up to open an app),
**Backspace** to go back. Window extras: **1–6** zoom, **L** / **R** rotate.

First-time SDL2 / PlatformIO setup: see the upstream
[lv_m5_emulator README](https://github.com/m5stack/lv_m5_emulator#readme).

## Adding an app

1. Create `src/apps/<name>/` exposing `const frij_app_t* <name>_app(void)`
   with `build_glance` + `build_screen` (include only `app.h`).
2. Register it with one line in `src/apps/apps.cpp`.

See [src/apps/README.md](src/apps/README.md) for the contract and the full model.

## Layout

Each `src/` package has its own README:

- [`src/apps/`](src/apps/README.md) — the mini-apps (+ settings).
- [`src/launcher/`](src/launcher/README.md) — navigation shell.
- [`src/ui/`](src/ui/README.md) — shared widgets (carousel; future components).
- [`src/store/`](src/store/README.md) — key→JSON storage (local + Supabase).
- [`src/utility/`](src/utility/README.md) — board-specific LVGL ↔ M5GFX bridge.

Project docs: [architecture](docs/ARCHITECTURE.md), [storage/cloud](docs/STORAGE.md),
[hardware](docs/HARDWARE.md), [roadmap](docs/ROADMAP.md).

## License

MIT — see [LICENSE](LICENSE).
