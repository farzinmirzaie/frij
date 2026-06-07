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
  └─ frij_launcher_start()  ── reads registry, draws home tiles

registry  (src/launcher/registry.*)   small list of frij_app_t*
launcher  (src/launcher/launcher.*)   home grid + open/back navigation
contract  (src/app.h)                 frij_app_t { name, color, open(parent) }
apps      (src/apps/<name>/)          each exposes const frij_app_t* <name>_app()
glue      (src/apps/apps.cpp)         the ONE file that knows every app
```

**Who knows whom (dependency direction):**

- A mini-app includes only `app.h`. It never includes the launcher. It just
  fills the `parent` container it is given in `open()`.
- The launcher includes only `registry.h` + `app.h`. It iterates the registry;
  it has no list of app names baked in.
- `apps.cpp` is the single glue point that includes each app and the registry,
  and wires them together. Adding an app = new folder + one line here.

**Navigation is the launcher's job.** Tapping a home tile makes the launcher
create a full-screen page with a launcher-owned "Back" button, then calls the
app's `open(content)` with the area below it. Back deletes the page and the
home screen underneath returns. The app never sees the back button.

**State:** the demo `counter` keeps state in file-static vars (one instance on
screen at a time, reset on open). Real apps with persistence will store their
own state — that's the app's concern, not the launcher's.

## Emulator vs device

| | Emulator (`emulator_Dial`) | Device (`board_StopWatch`) |
| --- | --- | --- |
| Platform | `native` + SDL2 | `espressif32` + Arduino |
| Screen | M5Dial round 240×240 frame | round AMOLED 466×466 (CO5300) |
| Entry | `sdl_main.cpp` `main()` | Arduino runtime |
| Status | works now | WIP — board enum TBD |

Because resolutions differ, **don't hardcode pixel positions** — center / align
relative to the screen so the same code scales to the real panel.
