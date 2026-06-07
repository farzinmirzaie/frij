# Changelog

Newest first. One short entry per change.

## 2026-06-08

- `frij_store` now talks to **Supabase** on the emulator (libcurl): `save`
  upserts + caches, `pull` fetches into the cache, `load` reads the cache.
  Config from `.env`. Added ArduinoJson + `-lcurl`.
- Fleshed out **Todo** into a cloud-backed checklist (JSON array via
  ArduinoJson); toggling an item syncs. Counter pulls on open too.
- Requires a `store` table in Supabase (see docs/STORAGE.md).

## 2026-06-07

- Added `.env.example` (Supabase config) and documented Supabase setup + the
  official Supabase MCP in `docs/STORAGE.md` / `docs/SKILLS.md`. `.env` gitignored.

## 2026-06-07

- Added a shared **`frij_store`** key→JSON utility (`src/store/`): emulator
  file backend now, Supabase over HTTPS on device later. Counter persists its
  value through it. Design in `docs/STORAGE.md`; chosen cloud backend: Supabase.

## 2026-06-07

- Carousel now **follows the finger** during a drag and snaps on release
  (`carousel.cpp` owns press/drag input; vertical swipes routed via a callback).
- Each app has its own background **color** (`frij_app_t.color`); colors slide
  in during a swipe.
- Todo now has **3 screens** (list / add / stats) to exercise the app carousel.

## 2026-06-07

- Reworked the launcher into a looping glance **carousel** (Launcher phase A):
  swipe left/right between apps, swipe up to open an app's own screen carousel,
  swipe down for a Settings stub, and Back via Backspace (emulator) / button GPIO
  (device, TODO). New files: `launcher/carousel.*`, `launcher/input.*`; launcher
  is now a layer state machine.
- Evolved the app contract: `frij_app_t` is now `{ name, build_glance,
  screen_count, build_screen }`. Updated Todo and Counter accordingly.
- Added `docs/LAUNCHER.md` (layer/gesture design).
- Trimmed `platformio.ini`: collapsed the header comment, dropped the redundant
  `LVGL_USE_V8=0` define, and tightened comments.
- Made `CLAUDE.md` the single project-context file (dropped the `AGENTS.md` symlink).
- Trimmed `support/sdl2_build_extra.py` (drop unused import + dead code, add comment)
  and the obsolete copy-instructions header in `include/lv_conf.h`.
- Expanded README: how it works, controls, and how to add an app.
- Merged `lv_conf_v9.h` into `include/lv_conf.h` (single LVGL config file).
- Apple-Silicon-only: removed Intel/Linux/Windows build notes and the unused
  `-m32` handling in `support/sdl2_build_extra.py`.
- Tightened README and docs; removed conversational/session-specific phrasing.
- Made the project target-agnostic: dropped board-specific branding; apps are
  pure LVGL, only `src/utility/` is board-specific. Board specs moved to
  `docs/HARDWARE.md`. Device env renamed to `device`.
- Added `docs/SKILLS.md` (relevant Claude tooling) and a rule to forbid `sa-*`
  skills in this repo.
- Added the launcher + isolated mini-app architecture: neutral contract
  (`src/app.h`), launcher with tile grid and open/back navigation, an app
  registry, and example apps (`todo`, `counter`) wired in `src/apps/apps.cpp`.
- Trimmed the upstream emulator fork to a minimal base: two PlatformIO envs
  (`emulator_Dial`, `device`), LVGL v9 only, removed EEZ Studio, Tab5, unused
  boards, scaffold READMEs, and image assets.
- Added `CLAUDE.md` and `docs/` for project context.
