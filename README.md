# Frij

A fridge-mounted touch UI for the **M5Stack StopWatch Dev Kit** (ESP32-S3, round
466×466 AMOLED) — todos, reminders, grocery list, photos. Built with LVGL v9 +
M5GFX. Develop on a PC emulator now; flash the real device later.

Forked and trimmed from [m5stack/lv_m5_emulator](https://github.com/m5stack/lv_m5_emulator).

## Run the emulator (macOS)

```bash
brew install sdl2 pkg-config                     # once
~/.platformio/penv/bin/pio run -e emulator_Dial  # build + open the window
```

(Or use the PlatformIO sidebar in VS Code.) On Intel Mac / Linux, remove the
`-arch arm64` line in `platformio.ini`.

Window keys: **1–6** zoom, **L / R** rotate.

## Where things live

- `src/user_app.cpp` — **start here**, this is your UI code.
- `AGENTS.md` — full project context for humans and AI agents.
- `docs/` — [roadmap](docs/ROADMAP.md), [architecture](docs/ARCHITECTURE.md),
  [changelog](docs/CHANGELOG.md).

New here? Read `AGENTS.md`, then `docs/ARCHITECTURE.md`.
