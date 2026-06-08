# Changelog

Newest first. One short entry per change.

## 2026-06-09

- Page dots are now **always visible** (kept above the pages with
  `lv_obj_move_foreground`; dropped the idle auto-fade). Verified on home.
- The app **header is now persistent**, owned by the launcher above the content
  carousel — it no longer swipes with the screens. The action icon updates per
  screen via the app's new `action_symbol(index)` / `on_action(index)` contract;
  `build_screen` is content-only. Carousel gained a `set_change_cb` hook.

## 2026-06-09

- Added a shared **`frij_header`** component (back + centered title + optional
  action button), round-safe; Todo's screen now uses it (back + "+" action).
- **Back** on the home layer now jumps to the default tile (the clock); from an
  app/settings it returns home as before (added `frij_carousel_goto`).
- The Home/clock tile is **glance-only** now — swiping up on it does nothing
  (apps with no `build_screen` can't be opened).
- Added [docs/TESTING.md](TESTING.md): how to build + visually verify via the
  headless snapshot tool.

## 2026-06-08

- Sized the SF Pro Rounded fonts up for the 466px screen (body 18, title 26,
  display 34, clock 64; symbols on Montserrat 20) — legible at device resolution.
- Home clock face: added a dimmer concentric **minute ring** inside the seconds ring.
- Snapshot tool can now capture a specific screen (`FRIJ_SNAP=todo|counter|settings`);
  verified Todo/Settings/Counter render correctly at 466 (no overflow, slider unclipped).

## 2026-06-08

- Fixed a black screen in the LVGL-SDL emulator: it had no tick source, so
  nothing rendered. Set `lv_tick_set_cb(SDL_GetTicks)` in `sdl_lvgl_main.cpp`.
- Switched LVGL to the **system allocator** (`LV_STDLIB_CLIB`); the built-in
  64KB pool was far too small for 466×466 (render layers, clip masks, snapshots
  failed/hung). Important for the device too (it has 8MB PSRAM).
- Added a headless **snapshot tool** (`snapshot` env, `src/utility/snapshot_main.cpp`)
  that renders the UI offscreen to a BMP — lets the UI be checked visually where
  `screencapture` can't reach the display.
- Round-clipped the UI: the launcher clips `s_root` to a circle, fills the
  corners with a light-gray "outside" color (`FRIJ_OUTSIDE`), and **ignores
  touches outside the circle**. Removed the emulator bezel overlay (clip handles it).
- Cleanup: **dropped `emulator_Dial`** (and `sdl_main.cpp`). `emulator_StopWatch`
  is now the default and only emulator. Updated all docs.

## 2026-06-08

- Added a **466×466 round emulator** (`emulator_StopWatch`) on LVGL's own SDL
  driver — previews the UI at the real device resolution + round bezel, app code
  unchanged (`src/utility/sdl_lvgl_main.cpp`; `LV_USE_SDL` gated by
  `FRIJ_USE_LV_SDL`). Fixed `build_src_filter` paths to be src-relative.

## 2026-06-08

- Carousel now shows an **auto-fading page-dot indicator** at the bottom: fades
  in on swipe, idles out after ~1.4s, active dot uses the layer's accent, hidden
  for single-page layers. Per-instance timer cleaned up on viewport delete.

## 2026-06-08

- Added **haptics** as a board service (`src/system/haptics`): UI components
  fire a tap/success pulse on interactions (`frij_haptic_attach`); Counter
  buttons too. Settings → General → **Vibration** toggles it (persisted, applied
  at boot). No-op on the emulator; motor pulse on device is a TODO.
- Redesigned the **Home clock face**: a thin seconds ring (scales to 80% of the
  screen) with large SF Pro Rounded numerals + a refined date (no leading zero,
  AM/PM in 12h) centered inside. Added a 46px `frij_sf_clock` font.

## 2026-06-08

- **SF Pro Rounded** is now the UI font (converted via lv_font_conv, in
  `src/ui/fonts/`; symbols stay on Montserrat via `FRIJ_FONT_SYMBOL`). See the
  fonts README for regen + the license caveat.
- Fixed the **clipped slider** at the component level (knob inset with margins).
- Added **subtle gradients** at the ui level: page background (`frij_apply_bg`)
  and surface rows. Added `frij_screen_min()` and made the Home ring scale with
  the screen → layouts stay correct on the larger device.
- **Settings** grew to 5 screens: Display (brightness), Sound (volume), General
  (24h), Network (Wi-Fi placeholder), About. Volume persists.

## 2026-06-08

- Fixed carousel/nav lag: **cloud I/O moved off the UI thread**. `frij_store`
  now pushes on a background thread and adds `pull_async`; the cache is written
  atomically. Todo/Counter pull async on open, so swipes/toggles never block on
  the network (the real cause of the stutter). Added `-pthread`.
- Polish pass: **brightness now works** via a neutral `src/system/brightness`
  interface (board impl) — slider applies live + saved value applied at boot.
- Polish pass: **Home watch face** got a seconds ring, 12h AM/PM, and no leading zero.
- Polish pass: tactile **press-pop** on Counter buttons; Todo long text ellipsizes.

## 2026-06-08

- Per-app **color scheme**: `app.color` is now the app's accent (from the ui
  palette); the page background is uniform Surface-1. Todo amber, Counter blue,
  Home/Settings purple. Added `FRIJ_YELLOW` + `frij_slider`/`frij_toggle`/`frij_page`.
- Added a **Home** watch-face app (time + date, ticks each second, reads the
  24-hour setting) and registered it first → it's the default landing glance.
- Fleshed out **Settings**: brightness slider + 24-hour toggle (both persisted
  via the store) + about. Counter restyled to the theme.

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
