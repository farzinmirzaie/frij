# Frij — M5Stack StopWatch multi-app UI

A fridge-mounted touch UI (todos, reminders, grocery list, photos) for the
**M5Stack StopWatch Dev Kit**. Built on LVGL v9 + M5GFX. Develop on the PC
emulator first; flash the real device later.

> **Owner is new to C and M5Stack.** Favor simple, readable code with comments
> that teach. Explain *why*, not just *what*. No premature abstraction.

## Target hardware

- MCU: ESP32-S3R8 (16MB flash, 8MB PSRAM)
- Display: **round** 1.75" AMOLED, **466×466**, driver CO5300
- Input: capacitive touch (CST820B), 2 programmable buttons + power, IMU (BMI270)
- Emulator stand-in: M5Dial round frame (240×240) — design round, expect more px on device.

## Quick start (PC emulator — no hardware needed)

```bash
brew install sdl2 pkg-config           # once
~/.platformio/penv/bin/pio run -e emulator_Dial    # build + run
```

`pio` is not on PATH; it lives at `~/.platformio/penv/bin/pio` (installed by the
VS Code PlatformIO extension). In VS Code you can also click the env in the
PlatformIO sidebar.

## Structure

| Path | Purpose |
| --- | --- |
| `src/main.cpp` | Boot: init display + LVGL, call `user_app()` |
| `src/user_app.cpp` | Entry: register apps + start launcher (thin wiring) |
| `src/app.h` | Neutral app contract (`frij_app_t`) — all a mini-app needs |
| `src/launcher/` | Home screen, navigation, back button, app registry |
| `src/apps/` | Mini-apps (one folder each) + `apps.cpp` where they register |
| `src/utility/` | LVGL↔M5GFX glue: `lvgl_port_m5stack.cpp` (device), `sdl_main.cpp` (emulator) |
| `include/lv_conf*.h` | LVGL v9 config (`lv_conf.h` just includes `lv_conf_v9.h`) |
| `support/sdl2_build_extra.py` | SDL2 build helper for the emulator |
| `platformio.ini` | 2 envs: `emulator_Dial`, `board_StopWatch` (device WIP) |
| `docs/` | Living project docs — see below |

## Key patterns

- UI work must be wrapped in `lvgl_port_lock()` / `lvgl_port_unlock()`.
- Use LVGL v9 API (`lv_screen_active()`, `lv_color_hex()`, etc.).
- Round screen: keep key content centered.
- **Adding an app:** build `src/apps/<name>/` (include only `app.h`, expose a
  `const frij_app_t* <name>_app(void)`), then add one line to
  `src/apps/apps.cpp`. Apps never include launcher code. See `docs/ARCHITECTURE.md`.

## Docs (read + UPDATE these every session)

- [docs/CHANGELOG.md](docs/CHANGELOG.md) — **append an entry for every change you make.**
- [docs/ROADMAP.md](docs/ROADMAP.md) — the app vision + what's next.
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — how boot, render loop, and the port layer work.

**Rule for any agent:** before finishing a task, log what changed in
`docs/CHANGELOG.md` and update `docs/ROADMAP.md` if scope moved. This keeps
context alive across sessions.

## Working rhythm (commit / review loop)

The owner reviews each round of changes to learn. So:

1. **At the end of a round, leave changes UNCOMMITTED** for the owner to review.
2. **At the start of the next round, commit the previous (now-reviewed) round
   FIRST** — one clean Conventional Commit — *before* writing any new code.
3. Then do the new round's work and again leave it unstaged.

Never bundle two rounds into one commit. One reviewed round = one commit.

## Gotchas

- No StopWatch board enum in M5GFX yet → `board_StopWatch` won't build until set. Emulator is the daily driver.
- `-arch arm64` in `platformio.ini` is Apple-Silicon only; remove on Intel/Linux.
- This is a fork of `m5stack/lv_m5_emulator`; trimmed to the StopWatch + emulator only.
