# Architecture

How the code boots and draws, so you know where to plug things in.

## Layers

```
your UI code  ──>  user_app()            (src/user_app.cpp)
                      │
LVGL v9 (widgets, layout, events)        (lib dep: lvgl)
                      │
port glue   ──>  lvgl_port_m5stack.*      (LVGL <-> display bridge)
                      │
M5GFX (draws pixels)                      (lib dep: M5GFX)
                      │
            ┌─────────┴──────────┐
   real panel (device)     SDL2 window (emulator)
```

## Boot flow

1. **Emulator only:** `sdl_main.cpp` provides `main()` → opens the SDL2 window
   and calls `setup()` then `loop()` repeatedly. On real hardware, Arduino
   provides `main()` and calls the same `setup()`/`loop()`.
2. `setup()` (`src/main.cpp`): `gfx.init()` → `lvgl_port_init(gfx)` → `user_app()`.
3. `loop()` just idles; LVGL renders on its own task/timer inside the port.

So you almost never touch `main.cpp` — build the UI in `user_app()`.

## The port layer (`src/utility/lvgl_port_m5stack.cpp`)

Bridges LVGL and M5GFX: sets up the draw buffer, flush callback, touch input,
and a tick. It also exposes the **lock** you must use:

- `lvgl_port_lock()` / `lvgl_port_unlock()` — take this around any LVGL calls
  from `user_app()` or callbacks, because LVGL renders on a separate task and is
  not thread-safe.

This file is excluded from the emulator build (`build_src_filter` in
`platformio.ini`); the emulator uses `sdl_main.cpp` instead.

## Emulator vs device

| | Emulator (`emulator_Dial`) | Device (`board_StopWatch`) |
| --- | --- | --- |
| Platform | `native` + SDL2 | `espressif32` + Arduino |
| Screen | M5Dial round 240×240 frame | round AMOLED 466×466 (CO5300) |
| Entry | `sdl_main.cpp` `main()` | Arduino runtime |
| Status | works now | WIP — board enum TBD |

Because resolutions differ, **don't hardcode pixel positions** — center / align
relative to the screen so the same code scales to the real panel.
