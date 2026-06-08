# utility/

The only **board-specific** code: the LVGL ↔ M5GFX bridge.

- `lvgl_port_m5stack.*` — draw buffer, flush callback, touch input, tick, and
  the `lvgl_port_lock()` / `lvgl_port_unlock()` you must wrap UI work in (LVGL
  renders on its own task). Used on real hardware.
- `sdl_lvgl_main.cpp` — `main()` for the 466×466 round emulator
  (`emulator_StopWatch`), built on LVGL's own SDL driver (no M5GFX). Provides
  no-op `lvgl_port_lock/unlock`. Guarded by `FRIJ_USE_LV_SDL`.
- `snapshot_main.cpp` — headless `main()` for the `snapshot` env: renders the UI
  offscreen and writes `/tmp/frij_snapshot.bmp` (no window). Guarded by
  `FRIJ_SNAPSHOT`. Run: `pio run -e snapshot && .pio/build/snapshot/program`,
  then `sips -s format png /tmp/frij_snapshot.bmp --out out.png`.

`build_src_filter` in `platformio.ini` excludes the device-only files from the
emulator (paths are src-relative, e.g. `-<main.cpp>`).

Retargeting to another board means changing this folder; `apps/`, `ui/`,
`launcher/`, and `store/` don't change.
