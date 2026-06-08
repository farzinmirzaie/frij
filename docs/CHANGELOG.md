# Changelog

Newest first. One short entry per change.

## 2026-06-08

- Added a design system to `src/ui/`: `theme.h` (colors, type, spacing, radius,
  motion tokens from the Frij design system) and `components.*` (themed row,
  circular check with pop, progress ring, empty state, entrance animation).
- Polished **Todo** with them: dark surface, rounded rows, purple checks, a
  progress ring on the glance/stats, staggered fade-in rows, animated toggle,
  and press feedback. Type maps to Montserrat until a rounded font is added.

## 2026-06-08

- Filled in real M5Stack StopWatch (C152) hardware details in `docs/HARDWARE.md`
  from the official docs: CO5300 QSPI panel (reset via M5IOE1), CST820B touch,
  Key A=G2 / Key B=G1, BMI270, RX8130CE, ES8311, M5PM1; `esp32s3box` board +
  M5Unified. Confirmed M5GFX/M5Unified support the board (no longer "blocked"),
  and updated the P5 bring-up plan.

## 2026-06-08

- Restructured `src/` into self-documenting packages: each folder now has its
  own `README.md` (apps, launcher, ui, store, utility) and a top `src/README.md`.
- Extracted a shared **`src/ui/`** package; moved the carousel there.
- Made **Settings an app** under `apps/settings/`; it registers in the registry's
  settings slot (`frij_registry_set_settings`) and the launcher reuses the app
  builder for it. Removed `launcher/settings.*`.
- Removed `docs/LAUNCHER.md` (now `src/launcher/README.md`); trimmed the
  duplicated launcher/store detail out of `docs/ARCHITECTURE.md` / `docs/STORAGE.md`.

## 2026-06-08

- Reworked navigation into a **4-direction finger-follow** model. The launcher
  now owns one input handler and routes by axis; the carousel became input-free
  (`drag`/`end`). Vertical swipes slide whole layers (home in the middle, app
  below, settings above) and follow the finger like horizontal paging.
- **Settings is now a multi-screen carousel** (`settings.*`, 2 screens) — same
  loop behavior as apps. Layers with a single screen don't loop.

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
