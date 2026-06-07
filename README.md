# Frij

A small launcher with mini-apps (todos, reminders, lists, photos…) for round
LVGL touch displays.

Built with [LVGL](https://github.com/lvgl/lvgl) v9 and
[M5GFX](https://github.com/m5stack/M5GFX). Forked from
[m5stack/lv_m5_emulator](https://github.com/m5stack/lv_m5_emulator).

## Build

Run the UI on your computer via SDL2 (no hardware required):

```sh
pio run -e emulator_Dial
```

For SDL2 and PlatformIO setup, see the upstream
[lv_m5_emulator README](https://github.com/m5stack/lv_m5_emulator#readme).

## Layout

- `src/apps/` — the mini-apps; `src/apps/apps.cpp` registers them.
- `src/launcher/` — home screen and navigation.
- `docs/` — [architecture](docs/ARCHITECTURE.md), [hardware](docs/HARDWARE.md),
  [roadmap](docs/ROADMAP.md).

## License

MIT — see [LICENSE](LICENSE).
