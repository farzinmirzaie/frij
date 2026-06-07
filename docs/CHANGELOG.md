# Changelog

Append-only log so future sessions know what happened. Newest first.
One short entry per change: **what** changed and **why**.

## 2026-06-07 — Decouple target + skills notes (round 3)

- Made the project **target-agnostic**: removed fridge / specific-board branding
  from the name and docs. Apps were already pure LVGL; only `src/utility/` is
  board-specific. Why: the same apps should compile for other boards later.
- `platformio.ini`: renamed the device env `board_StopWatch` → `device`; reworded
  comments to describe the first board by spec, not brand.
- Moved concrete board specs into [docs/HARDWARE.md](HARDWARE.md) (reference, not identity).
- Added a rule to `AGENTS.md`: **never use `sa-*` skills in this repo**.
- Added [docs/SKILLS.md](SKILLS.md): community Claude skills/tools worth using on this stack.
- Verified: `pio run -e emulator_Dial` SUCCESS.

## 2026-06-07 — Launcher + first apps (round 2)

- Renamed the product to **Frij** everywhere.
- Added the commit/review working rhythm to `AGENTS.md`: leave each round
  unstaged for review, commit it at the start of the next round.
- New architecture: launcher + isolated mini-apps.
  - `src/app.h`: neutral app contract (`frij_app_t`).
  - `src/launcher/`: `registry` (app list) + `launcher` (home tiles, open/back nav).
  - `src/apps/`: `apps.cpp` glue + `todo/` (checkbox list) + `counter/` (+/- value).
  - `user_app.cpp`: now just `frij_register_apps()` + `frij_launcher_start()`.
  - Apps include only `app.h` — they never know about the launcher.
- `platformio.ini`: added `-I src` so apps resolve `app.h` / cross-folder headers.
- Counter uses `lv_font_montserrat_26` (28 isn't enabled in lv_conf).
- Verified: `pio run -e emulator_Dial` SUCCESS.

## 2026-06-07 — Initial cleanup & project setup

- Forked `m5stack/lv_m5_emulator`; trimmed to a minimal, readable base.
- `platformio.ini`: cut ~10 board envs down to two — `emulator_Dial` (PC/SDL2)
  and a real-device placeholder (WIP). Inlined the old `emulator_common`.
  Why: only one board mattered; everything else was noise.
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
