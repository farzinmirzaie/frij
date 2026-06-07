# Changelog

Append-only log so future sessions know what happened. Newest first.
One short entry per change: **what** changed and **why**.

## 2026-06-07 — Initial cleanup & project setup

- Forked `m5stack/lv_m5_emulator`; trimmed to a minimal, readable base.
- `platformio.ini`: cut ~10 board envs down to two — `emulator_Dial` (PC/SDL2)
  and `board_StopWatch` (device placeholder, WIP). Inlined the old
  `emulator_common`. Why: the only target is the StopWatch; everything else was noise.
- Removed: EEZ Studio code/assets, Tab5 cleanup script, `lv_conf_v8.h`,
  PlatformIO scaffold READMEs (`lib/`, `test/`, `include/`), README gifs, support pngs.
- `include/lv_conf.h`: now just includes `lv_conf_v9.h` (we're v9-only).
- `src/user_app.cpp`: replaced the LVGL benchmark demo + EEZ branches with a
  minimal commented "Frij" hello screen — owner builds from here.
- Added agent context: `AGENTS.md` (+ `CLAUDE.md` symlink), `docs/CHANGELOG.md`,
  `docs/ROADMAP.md`, `docs/ARCHITECTURE.md`. Rewrote `README.md` for this project.
- Dropped redundant `-l SDL2` flag (pkg-config already links it) — removes a
  duplicate-library linker warning.
- Verified: `pio run -e emulator_Dial` builds clean (SUCCESS, ~23s).
