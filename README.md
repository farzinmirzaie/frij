# Frij

A small launcher + mini-apps (todos, reminders, lists, photos…) for a **round
touch display**. Built with LVGL v9 + M5GFX. Develop on a PC emulator now; flash
a real board later. Target-agnostic — the same apps can run on different
hardware (see [docs/HARDWARE.md](docs/HARDWARE.md)).

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

- `src/apps/` — the mini-apps; `src/apps/apps.cpp` registers them.
- `AGENTS.md` — full project context for humans and AI agents.
- `docs/` — [roadmap](docs/ROADMAP.md), [architecture](docs/ARCHITECTURE.md),
  [hardware](docs/HARDWARE.md), [skills](docs/SKILLS.md), [changelog](docs/CHANGELOG.md).

New here? Read `AGENTS.md`, then `docs/ARCHITECTURE.md`.
