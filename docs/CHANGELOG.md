# Changelog

Newest first. One short entry per change.

## 2026-06-07

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
- Added `AGENTS.md` (+ `CLAUDE.md` symlink) and `docs/` for project context.
