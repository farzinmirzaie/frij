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

1. **Emulator:** `sdl_lvgl_main.cpp` provides `main()` → creates a 466×466 LVGL
   SDL window + mouse input, then calls `user_app()` and runs `lv_timer_handler`.
2. **Device:** `main.cpp` `setup()` does `gfx.init()` → `lvgl_port_init(gfx)` →
   `user_app()`; `loop()` idles while LVGL renders on its own task.

Either way the UI is built in `user_app()` — you almost never touch the entry files.

## The port layer (`src/utility/lvgl_port_m5stack.cpp`)

Bridges LVGL and M5GFX: sets up the draw buffer, flush callback, touch input,
and a tick. It also exposes the **lock** you must use:

- `lvgl_port_lock()` / `lvgl_port_unlock()` — take this around any LVGL calls
  from `user_app()` or callbacks, because LVGL renders on a separate task and is
  not thread-safe.

This file is the device path; the emulator excludes it (`build_src_filter`) and
uses `sdl_lvgl_main.cpp` instead.

## Launcher + apps

`user_app()` (`src/user_app.cpp`) inits the store, registers apps, and starts
the launcher. From there:

- The **launcher** is the navigation shell (layers, gestures, registry) —
  see [../src/launcher/README.md](../src/launcher/README.md).
- **Apps** are isolated: each includes only `app.h`, never the launcher —
  see [../src/apps/README.md](../src/apps/README.md).
- Shared widgets live in [../src/ui/README.md](../src/ui/README.md); persistence
  in [../src/store/README.md](../src/store/README.md).

Dependency direction: apps → `app.h` (+ `ui/`, `store/`); the launcher reads the
registry and hardcodes no app names; `apps.cpp` is the single glue point.

## Emulator vs device

| | Emulator (`emulator_StopWatch`) | Device (`device`) |
| --- | --- | --- |
| Platform | `native` + LVGL SDL | `espressif32` + Arduino |
| Screen | 466×466, clipped to a circle | round AMOLED (see HARDWARE.md) |
| Entry | `sdl_lvgl_main.cpp` `main()` | Arduino runtime via `main.cpp` |
| Status | works now | WIP — placeholder env |

Because resolutions differ, **don't hardcode pixel positions** — center / align
relative to the screen so the same code scales to any panel. Board details:
[HARDWARE.md](HARDWARE.md).
