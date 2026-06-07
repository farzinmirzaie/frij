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

## Launcher + apps (the Frij model)

Frij is a **launcher** (home screen) plus a set of **mini-apps**. The goal is
that apps are isolated: an app knows nothing about the launcher or other apps,
so you can add one without touching anything else.

```
user_app()                                  (src/user_app.cpp)
  ├─ frij_register_apps()   ── apps.cpp adds each app to the registry
  ├─ frij_launcher_start()  ── reads registry, shows the glance carousel
  └─ frij_input_init()      ── wires the Back button / key

registry  (src/launcher/registry.*)   list of frij_app_t*
carousel  (src/launcher/carousel.*)   generic looping pager
launcher  (src/launcher/launcher.*)   layer state machine + gestures + back
input     (src/launcher/input.*)      Back: key (emulator) / GPIO (device)
contract  (src/app.h)                 frij_app_t { name, build_glance,
                                                    screen_count, build_screen }
apps      (src/apps/<name>/)          each exposes const frij_app_t* <name>_app()
glue      (src/apps/apps.cpp)         the ONE file that knows every app
```

**Who knows whom (dependency direction):**

- A mini-app includes only `app.h`. It never includes the launcher. It fills the
  `parent` containers it is handed in `build_glance` / `build_screen`.
- The launcher includes only `registry.h` + `carousel.h` + `app.h`. It iterates
  the registry; no app names are baked in.
- `apps.cpp` is the single glue point that wires apps into the registry.
  Adding an app = new folder + one line here.

Navigation, gestures, and Back are the launcher's job — see
[LAUNCHER.md](LAUNCHER.md) for the layer/gesture model.

**State:** the `counter` app keeps state in file-static vars (one instance on
screen at a time). Apps that need persistence store their own state — that's
the app's concern, not the launcher's.

## Emulator vs device

| | Emulator (`emulator_Dial`) | Device (`device`) |
| --- | --- | --- |
| Platform | `native` + SDL2 | `espressif32` + Arduino |
| Screen | round 240×240 frame | round AMOLED (see HARDWARE.md) |
| Entry | `sdl_main.cpp` `main()` | Arduino runtime |
| Status | works now | WIP — placeholder env |

Because resolutions differ, **don't hardcode pixel positions** — center / align
relative to the screen so the same code scales to any panel. Board details:
[HARDWARE.md](HARDWARE.md).
