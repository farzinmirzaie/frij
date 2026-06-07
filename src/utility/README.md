# utility/

The only **board-specific** code: the LVGL ↔ M5GFX bridge.

- `lvgl_port_m5stack.*` — draw buffer, flush callback, touch input, tick, and
  the `lvgl_port_lock()` / `lvgl_port_unlock()` you must wrap UI work in (LVGL
  renders on its own task). Used on real hardware.
- `sdl_main.cpp` — provides `main()` for the SDL2 emulator build. Excluded from
  the device build via `build_src_filter` in `platformio.ini`.

Retargeting to another board means changing this folder; `apps/`, `ui/`,
`launcher/`, and `store/` don't change.
