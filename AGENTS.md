# Frij — a multi-app launcher UI

A launcher with mini-apps (todos, reminders, lists, photos…) for a **round
touch display**. Built on LVGL v9 + M5GFX.

**Target-agnostic by design.** Apps are pure LVGL and don't know the board;
only `src/utility/` is board-specific, so the same apps compile for different
hardware. Board details: [docs/HARDWARE.md](docs/HARDWARE.md).

> Favor simple, readable code with comments that explain *why*. No premature
> abstraction.
>
> **Do not use `sa-*` (StashAway) skills in this repo.**

## Quick start

```bash
pio run -e emulator_Dial   # build + run the SDL2 emulator
```

If `pio` is not on PATH, it ships with the VS Code PlatformIO extension at
`~/.platformio/penv/bin/pio`.

## Structure

| Path | Purpose |
| --- | --- |
| `src/main.cpp` | Boot: init display + LVGL, call `user_app()` |
| `src/user_app.cpp` | Entry: register apps + start launcher (thin wiring) |
| `src/app.h` | Neutral app contract (`frij_app_t`) — all a mini-app needs |
| `src/launcher/` | Home screen, navigation, back button, app registry |
| `src/apps/` | Mini-apps (one folder each) + `apps.cpp` where they register |
| `src/utility/` | LVGL↔M5GFX glue (the only board-specific code): `lvgl_port_m5stack.cpp` (device), `sdl_main.cpp` (emulator) |
| `include/lv_conf.h` | LVGL v9 config (LVGL's `lv_conf.h` template, trimmed) |
| `support/sdl2_build_extra.py` | SDL2 build helper for the emulator |
| `platformio.ini` | 2 envs: `emulator_Dial`, `device` (real board, WIP) |
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
- [docs/HARDWARE.md](docs/HARDWARE.md) — board targets + how to add a new one.
- [docs/SKILLS.md](docs/SKILLS.md) — Claude skills/tools worth using on this stack.

**Rule for any agent:** before finishing a task, log what changed in
`docs/CHANGELOG.md` and update `docs/ROADMAP.md` if scope moved. This keeps
context alive across sessions.

## Commit / review loop

Changes are reviewed before they are committed:

1. At the end of a round, leave changes **uncommitted** for review.
2. At the start of the next round, commit the previous round **first** — one
   Conventional Commit — before writing new code.
3. One reviewed round = one commit. Never bundle two rounds together.

## Gotchas

- The `device` env is a placeholder — no verified board panel yet. Emulator is the daily driver.
- Dev machine is an **Apple Silicon Mac** only (emulator builds with `-arch arm64`).
- This is a trimmed fork of `m5stack/lv_m5_emulator`; M5GFX is the rendering lib.
