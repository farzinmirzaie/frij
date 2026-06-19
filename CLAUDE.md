# Frij — a multi-app launcher UI

A launcher with mini-apps (todos, reminders, lists, photos…) for a **round
touch display**. Built on LVGL v9 + M5GFX.

**Target-agnostic by design.** Apps are pure LVGL and don't know the board;
only `src/packages/platform/` is board-specific, so the same apps compile for different
hardware. Board details: [docs/HARDWARE.md](docs/HARDWARE.md).

> Favor simple, readable code with comments that explain *why*. No premature
> abstraction.
>
> **Do not use `sa-*` (StashAway) skills in this repo.**

## Quick start

```bash
pio run   # build + run the 466x466 round SDL emulator (emulator_StopWatch, default)
```

If `pio` is not on PATH, it ships with the VS Code PlatformIO extension at
`~/.platformio/penv/bin/pio`.

## Structure

| Path | Purpose |
| --- | --- |
| `src/main.cpp` | Boot: init display + LVGL, call `user_app()` |
| `src/user_app.cpp` | Entry: register apps + start launcher (thin wiring) |
| `src/app.h` | App contract (`frij_app_t`: glance + screens) — all a mini-app needs |
| `src/apps/` | Mini-apps + settings (**pure UI**); `apps.cpp` registers them — see its README |
| `src/packages/ui/` | Shared app-agnostic widgets + motion (`components`, `overlays`, `carousel`, `anim`, `theme`, fonts) |
| `src/packages/data/` | The seam: per-app data/view providers (e.g. `events`) — read store/system, hand apps plain structs |
| `src/packages/core/` | App-agnostic **non-UI** helpers (e.g. `datetime`) — pure, zero deps |
| `src/packages/store/` | Shared key→JSON store (file + Supabase, async) — see its README |
| `src/packages/system/` | Board services (battery, wifi, brightness, haptics, audio, sleep, motion, display, ai) |
| `src/packages/launcher/` | Nav (4-way finger-follow), registry, Back input — see its README |
| `src/packages/platform/` | The only board-specific code (LVGL↔M5GFX port, SDL/snapshot mains) |
| `src/packages/bridge/` | Off-device Python sync (Google Keep ⇄ `store:todo`, Calendar → `store:events`) + GitHub crons |
| `src/packages/supabase/` | Off-device edge functions (the AI `ask`). NOT compiled into firmware. CLI: `--workdir src/packages/supabase` |

**Layering rule:** `apps/` may include `app.h`, `packages/ui`, `packages/core`,
`packages/data` only — **never** `store`, `system`, `platform`, or the
off-device packages. The `data/` layer is the seam that talks to services and
returns plain structs. (Pilot: the Events app + `packages/data/events.*`.)

Each package folder has its own `README.md` with the details.
| `include/lv_conf.h` | LVGL v9 config (LVGL's `lv_conf.h` template, trimmed) |
| `support/sdl2_build_extra.py` | SDL2 build helper for the emulator |
| `platformio.ini` | envs: `emulator_StopWatch` (466 round, LVGL-SDL, default), `device_new` (carousel launcher, flashed + verified), `device` (old launcher, compile-only), `snapshot` |
| `docs/` | Living project docs — see below |

## Key patterns

- UI work must be wrapped in `lvgl_port_lock()` / `lvgl_port_unlock()`.
- Use LVGL v9 API (`lv_screen_active()`, `lv_color_hex()`, etc.).
- Round screen: keep key content centered.
- **Adding an app:** see [src/apps/README.md](src/apps/README.md). Apps include
  only `app.h`, never launcher code. Shared widgets go in `packages/ui/`.
- **Any new UI pattern must be a reusable `packages/ui/` component** (keypad, result
  screen, icons, logo…), never app-local widget code — other apps will need it.

## Docs (read + UPDATE these every session)

- [docs/CHANGELOG.md](docs/CHANGELOG.md) — **append an entry for every change you make.**
- [docs/ROADMAP.md](docs/ROADMAP.md) — the app vision + what's next.
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — how boot, render loop, and the port layer work.
- [docs/TESTING.md](docs/TESTING.md) — **how to build + visually verify changes (snapshot tool).**
- [docs/STORAGE.md](docs/STORAGE.md) — cloud setup (Supabase table, env, keys).
- [docs/AI.md](docs/AI.md) — the assistant pipeline (edge function, Gemini, setup).
- Per-package detail lives in each `src/packages/*/README.md` + `src/apps/README.md`.
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

## End-of-round report (required)

Every round, end the reply with a **"What changed & how to test"** section: for
each change, a one-line summary + concrete **manual steps the user can do on the
running emulator** to see it, including the expected result. Be specific, e.g.:

> **Sleep timeout** — Settings → General → set Sleep to 1 min, don't touch for
> 1 min → screen goes black; tap → it comes back.

Cover device-only changes too, but label them "(on device)" since they can't be
seen in the emulator. Keep it short and checkable — not a changelog dump.

## Gotchas

- The `device_new` env (carousel launcher) **has been flashed and runs on the
  M5Stack StopWatch** — panel, touch, both buttons, Wi-Fi, and the AI voice path
  are verified on hardware. Emulator is still the daily driver; the plain
  `device` env (old launcher) stays compile-only. First device build ~15 min.
- Dev machine is an **Apple Silicon Mac** only (emulator builds with `-arch arm64`).
- This is a trimmed fork of `m5stack/lv_m5_emulator`; M5GFX is the rendering lib.
