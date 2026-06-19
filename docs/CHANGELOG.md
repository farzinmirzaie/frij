# Changelog

Newest first. One short entry per change.

## 2026-06-19 — Audio idle-off, Wi-Fi scan keeps connection, drag thresholds

- **White noise fixed** — the ES8311 amp was left powered after a tone (idle hiss);
  now `frij_audio_idle_tick()` (called each loop) powers it down once nothing's
  played for ~2s. Click bursts never hit the gap, so they stay crisp.
- **Wi-Fi scanning keeps the connection visible** — a rescan now shows the active
  network row + a "Scanning…" footer instead of a blank scanning screen.
- **Drag-to-open less jumpy** — vertical nav is asymmetric: pull-up (open app)
  engages at 16px, pull-down (Settings) needs 28px, so an up-drag that starts with
  a tiny downward settle can't flash Settings.

## 2026-06-19 — UX: Wi-Fi sort, drag fix, consistent haptics, reel feedback

- **Wi-Fi list sorted** — the connected network first, then strongest → weakest
  signal.
- **Drag-to-open-app fixed** — an up-drag from the watch face no longer occasionally
  jumps to Settings. The vertical nav now commits only on travel *in the locked
  direction* (was absolute |dy|), so a brief direction mis-lock reverts instead of
  firing the wrong layer.
- **Consistent haptics** — every tap now uses the medium buzz (SELECT), matching the
  slider; removed the double-buzz on toggles and score cards (they were
  `frij_haptic_attach`'d *and* fired a second haptic). SUCCESS still marks completions.
- **Reel spin feedback** — the "who goes first?" reel ticks (buzz + click) once per
  name, fast → slow with the ease-out, ending on the winner's success buzz.

## 2026-06-19 — Bigger glow + core affinity (UI off the network core)

- **Accent glow images enlarged** 320 → 400 px — a wider halo behind each glance.
- **Render pinned to core 1; network/background to core 0.** The LVGL render task
  was unpinned (could land on the Wi-Fi core); now it owns core 1 (APP) while the
  store sync worker, AI capture/upload, and wifi scan/connect run on core 0 (PRO,
  with the Wi-Fi stack). A sync / connect / AI burst no longer hitches the UI.
  (Dual-core *render* was tried and reverted — flush-bound, no gain.)

## 2026-06-19 — New carousel launcher (device, behind FRIJ_NEW_LAUNCHER)

- An alternate launcher for the device, selected at build time by
  `-D FRIJ_NEW_LAUNCHER` (the `device_new` env): vertical Home / Settings / App
  layers with a horizontal glance row. The original `frij_launcher_start` is
  unchanged and still the default (`device`). Vertical nav direction-locks per
  gesture with a fast-flick (fling) commit + nav haptics; Key B = Back (tap = back
  a layer, hold = watch face); swipe hint + header action-spin carry over.

## 2026-06-19 — Watch face: clock rings back, accent glow as an image

- **Clock rings restored.** The home face shows the outer gliding seconds ring +
  the dimmer inner minutes ring again (dropped earlier for slide perf).
- **Accent glow is back as a baked opaque image.** The old translucent gradient
  re-blended every slide frame (~1 fps on the GPU-less panel); an opaque RGB565
  glow-on-black is a plain blit (identical look on the pure-black AMOLED) that
  costs ~nothing. One image per app accent (`glow_img.c`); `frij_glow_for()` picks
  by colour.

## 2026-06-19 — Performance round (device)

- **Panel SPI write clock → 80 MHz.** The pixel-push (flush) is the slide
  bottleneck and its time scales ~1/clock; autodetect left it conservative — the
  biggest single slide-fps win. (Async-DMA double-buffering was tried and reverted:
  `endWrite` force-waits on this panel, so there's no overlap to gain.)
- **Draw buffer 40 → 64 lines** — fewer flush round-trips per full redraw, still
  under the internal-RAM ceiling the TLS handshake shares.
- **`LV_OBJ_STYLE_CACHE` on** — fewer style-cascade lookups per redraw.
- **`CORE_DEBUG_LEVEL` 4 → 1** (device_new) — drops per-event `log_d()` printf over
  USB CDC (was stalling the UI during a sync).
- **Events list capped at 16 rows** (+ "+N more" footer) — 50 heavy rows could
  exhaust LVGL's heap and crash on first open; the full set still feeds the
  countdown/glance.

## 2026-06-18 — Scoreboard: moved up, swipe fix, "who goes first?" slot reel

- **Moved up.** Scoreboard now sits right after the Events glance (3rd app),
  ahead of Stopwatch.
- **VS swipe fixed.** The full-bleed score cards swallowed horizontal drags, so
  paging between scoreboard screens never fired. The cards now bubble the gesture
  (`EVENT_BUBBLE`) and carry a small edge inset — a swipe works from anywhere;
  tap still scores +1, long-press -1.
- **New "Who goes first?" screen.** A slot reel of the two players' names: tap to
  spin and it scroll-spins 10+ turns, easing to a stop on a random winner. No box
  — a 5-slot window with a 3-stop top/bottom gradient (header-style) buries the
  neighbour names so the centre pick stands alone. Reduce-motion lands instantly.
  (A pill spinner and a coin flip were trialled first; the reel won.)

## 2026-06-18 — Header Back fix + Frij AI on the new launcher

- **Header Back button now works on the new launcher.** `on_header_back` called
  the old, inert `frij_back`. It now routes to the iso launcher. `frij_iso_back`
  was split into a no-lock core (`iso_back_impl`) + a locking wrapper: the header
  tap fires inside the LVGL render task (lock already held) so it calls the core
  directly — re-locking would dead-lock the non-recursive LVGL mutex. Key B
  (device) still uses the locking wrapper.
- **Frij AI wired into the new launcher.** Key A (G2) is now push-to-talk on the
  carousel launcher too (it was only on the old one). The assistant pops a
  full-screen overlay, so it works from any layer.

## 2026-06-16 — Network time (SNTP) on device

- The device clock booted unset (near epoch), so the watch face + events showed
  the wrong time even on Wi-Fi. New `system/timesync` starts SNTP and sets the
  POSIX timezone once Wi-Fi is up; the clock self-corrects within a second or
  two. TZ defaults to Malaysia (UTC+8, no DST), overridable with `-DFRIJ_TZ`.
  Emulator is a no-op (the host clock is already correct).

## 2026-06-16 — Wake: raise-to-wake fix + G1/G2 button wake

- **Raise-to-wake now runs on device.** The loop returned before
  `frij_motion_update()` in the iso path, so the IMU was never polled — moved the
  poll above that branch. The `raisewake` setting is now cached (re-read ≤1×/s)
  instead of read from the filesystem every ~5 ms loop.
- **G1/G2 wake the panel.** A Key A/B press while asleep wakes the screen and is
  swallowed (it doesn't also fire Back/PTT); once both buttons are up the next
  press acts normally. (Touch already woke via LVGL's inactivity timer.)

## 2026-06-16 — Reduce-motion: overlays snap when Animations off

- The "Animations" setting now also covers overlay open/close. A dialog's
  close fade and a toast's fade in/out snap instantly when Animations is off
  (the entrance + rise were already gated). Done by zeroing the fade duration,
  so the cleanup/dismiss callbacks still fire.

## 2026-06-16 — Device store cache on LittleFS (Events/Todo sync)

- The device store backend now caches each key in a **LittleFS** file (`/<key>`)
  instead of NVS. NVS's ~20 KB partition caps a value at ~4 KB, so the ~3 KB
  events blob failed to write (`NOT_ENOUGH_SPACE`) and Events stayed empty;
  LittleFS uses the 3.5 MB data partition with no per-blob ceiling.
- Supabase GET/upsert run on one serialized worker task (TLS off the UI thread,
  never two sessions at once); the cache is guarded by a mutex since the worker
  and UI both touch the filesystem.

## 2026-06-16 — Buttons + audio wiring, debug overlay

On-hardware bring-up (M5Stack StopWatch):

- **Physical buttons.** Key A (G2, yellow) = Frij AI push-to-talk; Key B (G1,
  blue) = Back (tap) / home (hold). Both echo on-screen tap feedback — haptic on
  press, plus the click tone on Back (Key A hands the I2S bus to the mic, so no
  tone there).
- **Speaker idle hiss fixed.** The ES8311 amp is powered down at boot and after
  recording; tones re-begin on demand (`main.cpp`, `audio.cpp`, `ai.cpp`).
- **Touch feedback** fires on `LV_EVENT_CLICKED` (tap-up), not `PRESSED`, so
  scrolling a list no longer buzzes on every row.
- **Panel DMA** turned on — M5GFX leaves the StopWatch QSPI bus with DMA off, so
  full-screen redraws were a CPU byte-bang (`main.cpp`).
- **Debug performance overlay** — a Settings ▸ About ▸ Debug toggle shows LVGL's
  FPS/CPU monitor (centered for the round screen).

## 2026-06-16 — Battery + Storage: accurate device readings

- **Battery charge flag debounced.** The PMIC flag flickers near a full charge; a
  single false read blinked the About bolt. Charging now shows instantly but only
  clears after ~3 consecutive "not charging" reads (a real unplug). Poll dropped
  to 2 s (plug detection felt sluggish at 5 s); `frij_battery_poll()` lets a
  screen sample immediately on open. About shows just the level %.
- **Storage About reports the real app partition** (used + free of the running
  slot). Was `getFlashChipSize()`, which ignored the partition table and
  overstated free space by ~14 MB.

## 2026-06-16 — Wi-Fi: stay connected across scans, auto-connect, off-thread

On-hardware fixes (M5Stack StopWatch):

- **Scan no longer drops the connection.** `frij_wifi_scan` used to call
  `WiFi.disconnect()` before every scan; re-opening the Network screen (which
  rescans) tore down the live link. The ESP32 scans fine while connected, so the
  disconnect is gone — the connection now survives navigating around.
- **Auto-connect + auto-reconnect.** Enabling Wi-Fi (incl. at boot) joins the
  saved network fire-and-forget and sets `setAutoReconnect(true)`, so a dropped
  link comes back on its own. The emulator mock auto-connects to a known network.
- **Async scan/connect.** Both run on a short-lived worker task off the LVGL
  thread (`scan_start`/`scan_poll`, `connect_start`/`connect_poll`); the Network
  screen shows a "Scanning…/Connecting…" state and a poll timer reflects the
  result, so a join no longer freezes the UI for seconds.

## 2026-06-14 — keep bridge: multiple notes (GKEEP_NOTE_*)

- `keep_to_frij.py` now syncs **any number of Keep notes** instead of one: each
  `GKEEP_NOTE_<ID>=<storeKey>,<noteTitle>` maps a note to `store:<storeKey>`
  (+ `store:<storeKey>_base`). Mirrors the calendar `GCALENDAR_*` design. Auth
  happens once, notes loop, Keep write-backs batch into one `keep.sync()`; a
  missing note warns and is skipped. Dropped `GKEEP_LIST_TITLE`/`FRIJ_STORE_KEY`.
- `keep-sync.yml` discovers every `GKEEP_NOTE_*` secret via `toJSON(secrets)`
  (a "Collect notes" step), so adding a note needs only a new secret.
- Device unchanged: the todo app reads `store:todo` and `user_app` pulls `"todo"`,
  so `GKEEP_NOTE_TODO=todo,<title>` keeps it working as-is.
- Tests (`test_mapping.py` parse_notes) + docs (`.env.example`, bridge README,
  STORAGE) updated.

## 2026-06-14 — events: doc audit (multi-calendar) + data/ README

- Refreshed stale docs to match the multi-calendar app: `apps/README.md` events
  entry (3 screens, per-calendar colors, Calendars toggle, no holiday special
  case), the ROADMAP Events bullet, and the list empty-state string ("family" →
  generic). Added the missing `packages/data/README.md` (the app⇄services seam).

## 2026-06-14 — events: 10-point cleanup (perf / structure / naming)

Behavior-preserving refactor of the Events app + data layer:
1. Data layer captures the clock **once per build** (`s_now_tm`/`s_today_noon`)
   instead of calling `time()`/`localtime_r`/`mktime` for every event — cheaper
   day math across up to 50 events.
2. `frij_events_next()` is now a thin wrapper over `frij_events_load(out, 1)`
   (dropped a duplicated loop).
3. Widened the `events_off` JSON buffers 256 → 512 (8 max-length calendar names
   could overflow 256).
4. Extracted the breathing-pulse animation into reusable `frij_pulse()` in
   `ui/anim`; the list badge calls it instead of 14 inline lines.
5. Glance uses `frij_events_next(&v)` (clear intent; drops the `v[1]` array).
6. Screen indices are an `enum` (`SCREEN_LIST`/`COUNTDOWN`/`CALENDARS`/`COUNT`);
   `screen_count` derives from it.
7. Shared `add_centered_title()` + `add_cal_tag()` helpers (glance + countdown
   no longer duplicate the title/last-row layout).
8. `section_of(days)` helper + named `THIS_WEEK_DAYS` (was an inline ternary).
9. Named the badge-ink color (`BADGE_INK`) and row heights (`EVENT_ROW_H`,
   `EVENT_ROW_H_LOC`).
10. Tidied comments to match.

## 2026-06-14 — events: calendar name on glance + countdown

- The glance and countdown screens show the event's **calendar name in its
  color** as the **last row** (plain colored label, no dot). View struct gains
  `cal`.

## 2026-06-14 — events: raise cap to 50; GCALENDAR_COMPANY [MY]-only cleanup

- Event cap raised 10 → 50 (`FRIJ_EVENTS_MAX` + bridge `MAX_EVENTS`); the
  365-day window already bounds it to ~a year. The app's view buffer is now
  `static` (50 views would overflow the stack).
- Bridge special-cases `GCALENDAR_COMPANY` (the StashAway BambooHR feed): keeps
  only `[MY]` events and strips the `Company Holiday - [MY] ` prefix
  (`clean_company_title`). Other regions are dropped. Display name comes from
  the env value (e.g. `Company Holidays`). Tests added.

## 2026-06-14 — events: calendar color dots, feed UA fix, dotenv path fix

- Calendars screen: each row now shows a **leading dot in the calendar's color**
  before the name (switch stays the app accent). New reusable
  `frij_toggle_row_dot` in `packages/ui/components`.
- Bridge `fetch_ics` sends a real `User-Agent` — feeds that 403 the default
  `Python-urllib` agent (e.g. BambooHR company-holidays) now return 200.
- Fixed `load_dotenv` after the `src/packages/` move: it now finds the repo-root
  `.env` (three levels up) again, so local bridge runs load Supabase creds.

## 2026-06-14 — events: app-accent everywhere but the list; drop holiday case

- Per-calendar color now used **only on the event-list badges**. The glance,
  countdown, and the Calendars toggle switches use the app accent (pink).
- Removed the holiday special-case end to end: the device no longer treats a
  holiday calendar differently (gray is just its color), and the countdown/next-
  event screen no longer excludes it. Bridge drops the `holiday` token and the
  `h` flag (`url,name,color` only; a trailing legacy token is ignored).
  `frij_events_next_family` → `frij_events_next`. Tests/docs updated.
- CI: `calendar-sync.yml` now discovers every `GCALENDAR_*` secret via
  `toJSON(secrets)` (a "Collect calendars" step forwards them to `$GITHUB_ENV`),
  so adding a calendar needs only a new secret — no workflow edit.

## 2026-06-14 — events: multiple calendars (colors + on/off toggles)

- Bridge (`calendar_to_frij.py`) now reads **any number of `GCALENDAR_*`** env
  vars (`url,name,color[,holiday]`) instead of `FRIJ_ICS_URL`/
  `FRIJ_HOLIDAYS_ICS_URL`. Each event is tagged with its calendar; the payload
  gains a `cal` list (name/color/holiday) and per-event `c`. One broken feed is
  non-fatal. Tests updated (color/calendar parsing + new shape).
- Data layer (`packages/data/events`) parses `cal[]` + per-event color, exposes
  the calendar list, and persists a per-calendar hide set in `store:events_off`
  (applies to list, glance, and countdown).
- Events app: each badge/countdown/glance uses its **calendar's color** (gray
  for holidays, readable text by luminance); new **Calendars** screen (screen 2)
  toggles each calendar on/off. Added the `events_calendars` snapshot key.
- Docs: `.env.example`, `bridge/README.md`, `calendar-sync.yml` rewritten for
  `GCALENDAR_*`. Clarified `GKEEP_DEVICE_ID` is printed for reference but unused
  by the Keep sync (removed from setup as a required secret).

## 2026-06-14 — restructure: src/packages layout + events decoupled (pilot)

- Monorepo-style layout: shared code moved under `src/packages/` (ui, core,
  store, system, launcher, platform[was utility], data, bridge, supabase).
  Apps stay in `src/apps/`. Includes resolve via `-I src/packages` so most
  `#include` lines are unchanged; `utility/`→`platform/` and build_src_filter
  updated. Off-device bridge/supabase now live under packages (CI paths +
  supabase CLI `--workdir` noted).
- New layering rule: apps are PURE UI — may include only `ui`, `core`, `data`.
- Events pilot: new `packages/data/events.{h,cpp}` owns the store read, the
  {at,ev} parse, day math, badge units and time formatting, returning
  display-ready view structs. `apps/events/events.cpp` is now pure UI (no
  store/time/JSON). Behavior unchanged (verified by snapshots).
- Fix: Todo "Updated Xm ago" footer no longer always says "Just now" — the
  sync time is stamped at the real sync points (boot/auto-sync/manual ↻), not
  on every screen open.

## 2026-06-12 — events badge units, todo footer + single screen, Wi-Fi action

- Events badges scale their unit: hours for today's timed events ("3h"), then
  "3d" / "2w" / "5m" / "1y" instead of capping at "99+".
- Todo list gains an "Updated Xm ago" footer (matches Events); the progress
  ring and add-by-voice screens are gone — Todo is now a single screen
  (snapshot keys todo_progress/todo_add removed).
- The Network header's rescan (↻) hides while Wi-Fi is off (new
  frij_launcher_refresh_action re-queries the action live on toggle).

## 2026-06-12 — 10-improvement round (AI polish, events, haptics)

- Frij AI: tap a Recent row to re-read that answer; long answers scroll; the
  glance hint reflects whether the cloud is set up; backing out mid-capture on
  device aborts the mic (new frij_ai_listen_cancel) instead of firing a wasted
  call.
- Events: the badge of a today event gently pulses; the countdown screen shows
  the event location.
- Settings ▸ About: a "Frij AI · Ready / Not set up" row.
- Home: the battery readout pulses (deeper + faster) when low and unplugged,
  not just while charging.
- Toggle rows give a select haptic like the sliders; "Yesterday" added to the
  relative-time helper (Last sync).

## 2026-06-12 — Frij AI: real device mic capture (end to end)

- `system/ai` device backend: hold Key B (blue) records the ES8311 mic
  (M5.Mic, 16 kHz mono, ≤12s) on a FreeRTOS task; release wraps WAV +
  base64 + POSTs to the `ask` edge function over WiFiClientSecure. Gemini
  transcribes and answers in one call (audio path the function already had).
- New `frij_ai_listen_start/ask`: the assistant starts capture on press and
  sends audio on release; emulator no-ops them and keeps the text/sample mock.
- Device Key A = Back (tap)/home (hold) + Key B = push-to-talk, wired in
  main.cpp's loop via M5.BtnA/BtnB.
- Supabase URL + anon key bake into the device build from the shell env
  (platformio device build_flags); GEMINI_API_KEY stays server-side. The
  device path is compile-only — verify on flash. See docs/AI.md.

## 2026-06-12 — docs/comment audit: drop stale "mock"/"TODO" claims

- Cross-checked the tree after the AI work: no dead code found. Fixed stale
  comments/docs that still called the assistant a mock or the device buttons a
  TODO — assistant.h, ai.h, src/apps + ui + launcher READMEs, launcher/input.h.
- ROADMAP: Launcher C marked done (glance refresh), P2/Todo wording corrected,
  "Next step" repointed at P5 device bring-up. frij_apply_bg README line
  (gradient -> pure black) corrected.

## 2026-06-12 — consistent round ✓ on prompts + non-clipping loading dots

- Single-action prompts/result screens now use the round ✓ icon button (same
  as the two-action confirm's primary) instead of a text pill — every
  confirm/notice/result across the apps dismisses with the same control.
- `frij_loading_dots` pulses opacity in place instead of bouncing, so the dots
  can't clip against the round core they sit in (the Thinking screen).

## 2026-06-12 — Frij AI: shared error prompt + dots thinking loader

- Errors now use the same full-screen prompt as Reset/Erase (warning ring,
  message, single OK) instead of a bespoke screen with a retry button.
- Thinking drops the spinner for the listening visual's rippling rings with a
  three-dot loading bounce at the center (new reusable `frij_loading_dots`).
- Snapshot key `ai_thinking`.

## 2026-06-12 — Frij AI: friendly error UI

- The "ask" function now returns a short human message (e.g. "Frij AI is busy
  right now. Try again in a moment.") instead of leaking the raw provider JSON.
- The assistant renders errors as a dedicated state — amber warning ring, the
  message, and two round actions (dismiss + retry) — not as a wall of text in
  the answer slot. Errors aren't saved to Recent. Snapshot key `ai_error`.

## 2026-06-12 — ask function: wider scope + free-tier retries (v4 deployed)

- System prompt no longer implies the tools are its whole job — general
  kitchen/household questions get answered instead of refused.
- Transient Gemini free-tier 503/429s retry up to twice with backoff.

## 2026-06-12 — Frij AI backend: Gemini via a Supabase Edge Function

- New edge function `ask` (deployed): Gemini free tier + a tool loop over the
  store — reads events/todos, queues voice-added todos in `store:todo_inbox`,
  bumps the scoreboard. Returns {"q","a"}; audio input is already accepted for
  the device's future mic path. The Gemini key never leaves the function.
- New board service `system/ai`: worker-thread ask (libcurl, reuses the
  store's Supabase config), polled by the UI. Device backend TODO.
- The assistant now asks the REAL backend when configured (the emulator sends
  a sample question standing in for the mic) and renders the real answer;
  errors show honestly; no cloud -> canned answers as before. History stores
  dynamic strings now.
- Setup + curl test: docs/AI.md. Needs one secret: GEMINI_API_KEY.

## 2026-06-12 — Frij AI: Recent-only screen + listening edge glow

- The preset "Ask something" screen is gone — the app's single screen is the
  Recent history (last 5 Q&As). Push-to-talk is the only way to ask.
- The listening overlay gains a very subtle accent halo breathing at the
  screen's rim (new reusable `frij_edge_glow`).

## 2026-06-12 — Frij AI: hold gate, blue-button copy, history screen

- Push-to-talk is **hold-gated** (350ms): a stray tap of Key B / Space no
  longer summons the assistant.
- Copy says "hold the **blue** button" — per the official M5 docs Key A (G2,
  Back) is yellow and Key B (G1, AI) is blue; HARDWARE.md now records the
  colors and the power button's real behavior (press on, double-press off).
- New second screen in Frij AI: the **last 5 questions + answers** (RAM only),
  newest first; empty state until something is asked. Snapshot key
  `assistant_history`.

## 2026-06-12 — Frij AI: push-to-talk UI shell (mock pipeline)

- **New app: Frij AI** (violet) — the assistant's full interaction without the
  cloud yet. Hold Key B (Space on the emulator) anywhere -> full-screen
  listening overlay (rippling pulse rings + equalizer voice bars); release ->
  spinning "Thinking" with the heard question; ~1.5s later the answer screen
  (canned Q&A for now) with a dismiss button. Back cancels at any stage.
- Glance shows the brand + how to invoke; the app screen offers preset
  questions that run the same pipeline (works without a mic).
- New reusable ui pieces: `frij_pulse_ring` (rippling live indicator),
  `frij_sound_bars` (equalizer bars), `frij_modal_register` (app overlays get
  Back-to-close), shared `frij_anim_exec_rotation`.
- Snapshot keys: `assistant`, `assistant_glance`, `ai_listen`, `ai_answer`;
  the snapshot tool now runs timers before rendering so timed states settle.

## 2026-06-12 — Back steps to the app's main screen first

- Inside an app (or Settings), Back now returns to screen 0 if the user has
  swiped sideways; a second press closes the layer. Hold-Back still jumps
  straight to the watch face (it skips the screen-0 step).

## 2026-06-12 — hold-Back jumps to the watch face

- New `frij_home()`: closes any open dialog/sheet/layer and lands on glance 0.
- Back input is now short/hold aware: short press (on release) = back one
  layer; hold 600ms = home. Emulator keys unchanged (Backspace/Esc).
- HARDWARE.md documents all three buttons (Key A short/hold, Key B free,
  power button = PMIC, works out of the box).

## 2026-06-12 — fix: home battery showed the date after leaving an app

- Use-after-free: carousel pages are REUSED on rebuilds (`lv_obj_clean`, not a
  delete), but apps attach their teardown to the page via `LV_EVENT_DELETE` —
  so a rebuild (the new refresh-on-return, or reduce-motion page jumps) leaked
  the old build's timer + ctx. The home clock's orphaned 1s timer kept writing
  through dangling label pointers into recycled memory — the new battery label
  inherited "Fri 12 Jun". Same latent leak in Stopwatch and Scoreboard.
- Fix (systemic, in the carousel): `build_into` now delivers `LV_EVENT_DELETE`
  to the old build and removes its handlers before cleaning, so every app's
  page-attached cleanup runs on rebuild and can't double-free on real deletes.

## 2026-06-12 — 10 more (randomness, sound, sleep, hygiene)

- **PRNG seeded at boot**: LVGL's lv_rand boots with a fixed seed, so the
  "random" todo glance pick repeated the same sequence every boot — now seeded
  from the clock.
- **Erase all data also forgets the saved Wi-Fi** (credentials live in NVS,
  not the store — "all data" now means it).
- **Empty states ease in** instead of popping into place.
- **Volume slider previews on release** — a click at the level you just set.
  (on device)
- **Status toasts chirp**: rising two-tone on success, low buzz on failure,
  gated by the touch-sounds switch. (on device)
- **About gains an Uptime row** ("3h 24m" since boot) and keeps the build date.
- **Page dots flash when a layer opens** — you can see an app has more screens
  without guess-swiping; they still idle out.
- **Soft wake**: brightness ramps up from the dim level over 250ms instead of
  slamming to full.
- Perf/hygiene: Events computes each row's day-count once (was 3-4 times per
  row); stale "subtle dark gradient" comment fixed.

## 2026-06-12 — 10-improvement round (UX / micro-interactions / hygiene)

- **Glances refresh on return home**: closing an app rebuilds the visible
  glance (new `frij_carousel_refresh`) — the random todo pick, events, etc.
  no longer live frozen from boot.
- **Nav haptics**: a light tick when a page swipe commits and when an app/
  settings layer opens or closes (cancelled partial drags stay silent).
- **Pre-sleep dim warning**: the last 10s before idle-sleep drop brightness to
  15% — a touch restores it and resets the countdown.
- **Sleep fades to black**: the shade eases in over 300ms (panel powers off
  when the fade lands); waking stays instant. Reduce-motion: snap.
- **Stopwatch keeps the screen awake** while running (new
  `frij_sleep_inhibit`), released on stop.
- **Low-battery toast**: one-shot "Battery low" at ≤15% unplugged; re-arms
  after charging or recovering above 30%. (on device)
- **Charging pulse**: the home battery readout breathes gently while on
  power. (on device)
- **Esc = Back** in the emulator, alongside Backspace.
- **Snapshot tool is scriptable**: exit 1 on a failed capture, exit 2 on an
  unknown `FRIJ_SNAP` key (no more silently rendering the launcher).
- **About shows the build date**: "v0.1, Jun 12 2026" — which firmware is on
  the fridge at a glance.

## 2026-06-11 — Events: sections, holidays feed, countdown screen, freshness

- **Sections**: the list groups into Today / This week / Later with the shared
  section labels.
- **End-time grace**: a today event disappears from list/glance/countdown once
  its end time has passed (events without an end stay all day).
- **Holidays feed (optional)**: second iCal feed via `FRIJ_HOLIDAYS_ICS_URL`
  (e.g. Google's public Malaysia holidays calendar) merges into the same list
  with `"h":true`. Family events wear the pink accent on their day badge;
  holidays stay neutral gray. Missing/broken feed never blocks the family sync.
- **Countdown screen**: swipe to Events' second screen for a big-number
  countdown ("23 / days / Trip to Iran") to the next family event (holidays
  excluded).
- **Freshness**: the bridge stamps the payload (`{"at": epoch, "ev": [...]}`)
  and the list shows a tiny "Updated Xm ago" footer. The old bare-array shape
  still loads.

## 2026-06-11 — Todo glance: random open item + empty-state pattern

- The Todo glance now shows a **random unchecked item** on each rebuild
  ("On the list" + item + "N left") instead of always the first — other todos
  get face time without opening the app.
- Empty Todo glance follows the Events glance pattern: "No todos" title +
  "Add via Google Keep" subtitle.

## 2026-06-11 — reusable two-line empty states

- `frij_empty_state` redesigned: text-only (icon dropped) — `title` in primary
  white over a fainter `subtitle` hint (new `FRIJ_TEXT_3` token) saying how the
  screen gets filled. Used everywhere something can be empty: Todo ("Nothing
  yet / Add items in Google Keep"), Events ("No upcoming events / Add events in
  the family Google Calendar"), Wi-Fi off ("Wi-Fi is off / Turn on to see
  nearby networks") and the no-networks scan result.

## 2026-06-11 — pure-black backgrounds + always-on-top round bezel

- **Pure-black backgrounds**: `FRIJ_SURFACE_1` is now `0x000000` and
  `frij_apply_bg` is a solid fill (the old `0x0D0D10` + gradient wash read as
  gray) — launcher pages, the seams between them, and every app screen are
  true black, so AMOLED pixels switch fully off on the device.
- **Round bezel above overlays**: new reusable `frij_round_mask` — a thick
  FLOATING ring whose hole is the round panel. The live emulator puts it on
  `lv_layer_sys()` so full-screen overlays (prompts, numpad, results) can no
  longer cover the square window's corners; the snapshot harness adds the same
  ring last on the screen. Device builds never create it.

## 2026-06-11 — Events fixes: timezone, time ranges, all-day, location

- **Timezone fix**: the family calendar's own timezone is UTC, so Google's
  feed exports bare UTC instants — 12:00 showed as 04:00. The bridge now
  renders clock times in `FRIJ_TZ` (falls back to the calendar's
  X-WR-TIMEZONE); set in the workflow + .env(.example).
- **Time ranges**: same-day ends sync as "te" and render as "12:00 - 13:00"
  (list + glance, 24h-setting aware). All-day events say "all day"; multi-day
  all-day events show the span ("Sat 04 Jul - 17 Jul", via inclusive "de").
- **Location**: event locations sync as "l" and show as a muted third row
  line (row grows 72→88) and under the glance.
- **Empty state**: text-only (Wi-Fi-off style, no icon) + "Add events in the
  family Google Calendar" hint; glance hint matches.

## 2026-06-11 — Events app + Google Calendar bridge

- **New mini-app: Events** (pink) — countdowns to the family calendar's
  upcoming events. List rows carry a day-count badge ("3d", accent "Now"/"1d"
  when close), title + "Fri 12 Jun, 18:00" line; glance shows the nearest
  event ("Coming up · Gym class · Tomorrow, 18:00"). Read-only, ↻ pulls the
  store; clock times honor the 24-hour setting. Stale (past) cached events are
  hidden client-side.
- **New bridge: `bridge/calendar_to_frij.py`** — fetches the calendar's secret
  iCal URL (no OAuth), expands recurring events (`recurring-ical-events`),
  writes the next 10 to `store:events`. Offline-tested (`test_calendar.py`);
  hourly GitHub Actions cron (`calendar-sync.yml`, needs the `FRIJ_ICS_URL`
  secret).
- Boot + auto-sync now pull `events` too (deduped into `pull_synced_keys()`).
- Snapshot keys: `events`, `events_glance`.

## 2026-06-11 — drag-tied nav FX + perf/hygiene pass (10)

- **Navigation zoom/fade FX**: pages (left/right) and layers (up/down) now zoom
  (1.0 → ~0.86) and fade (→ ~35%) as they leave, and grow/fade in as they enter —
  tied 1:1 to the finger during drags, completed by the snap animation on
  release. Applies to swipes, flings, dot-taps and Back. Reduce-motion gated.
- **Page dots track the drag too**: the active pill hands its width, accent
  color and opacity over to the target dot proportionally as you drag (was only
  morphing after the page settled); reverts settle the dots back.
- Perf: the sleep manager re-reads the stored minutes every ~2s instead of twice
  a second; the **24h-clock pref is cached in memory** (was a file read every
  clock tick) with a setter from Settings.
- Perf (device): Wi-Fi row actions (connect/disconnect/forget) **no longer
  trigger a radio rescan** (1–2s block on hardware) — the cached scan list is
  updated in place; rescans happen only via ↻ or toggling the radio on.
- Perf: the emulator main loop sleeps for `lv_timer_handler()`'s next-run hint
  instead of spinning at 200Hz.
- Hygiene: overlays (modal stack, action sheet, numpad, prompt/result, toast)
  split out of components.cpp into **ui/overlays.cpp** (pure move; 1417 → 764+653
  lines); `frij_header_zone()` is the single source of truth for the header
  height (launcher + snapshot harness); the duplicated arc-anim exec callbacks
  became `frij_anim_exec_arc`; the header action only spins when visible.
- Verified `input.cpp` Backspace handling is edge-triggered (no repeat bug) and
  the store already does async pushes + atomic cache writes (no changes needed).
- Docs: CLAUDE.md device gotcha updated (compiles, never flashed); ROADMAP P5
  reflects the real bring-up status.

## 2026-06-11 — header redesign (slim + accent + scroll fade)

- **Slimmer header**: bar 78% → 62% wide, sits higher (11% vs 15%), and the
  header zone shrank 24% → 19% — more room for content on every app screen.
- **Accent identity**: the title + back/action icons now take the app's color
  (Todo amber, Settings purple, …); the **top tint gradient is gone** — the
  header background stays the dark base.
- **Scroll fade**: new reusable `frij_header_fade` — a base-color→transparent
  strip under the header, so rows scrolling up dissolve into the dark zone
  instead of clipping at a hard line. `frij_top_tint` removed (dead).
- `frij_header` signature gains the accent: `(parent, title, accent, action_cb)`.
- Follow-ups: header title rendered a notch smaller (~23px, scaled — no 22px SF
  cut available without the source OTFs); About hero enlarged (clover 60px,
  wordmark in the display font) and the version line is just "v0.1" (dropped the
  "on-device UI" filler); **in-place rebuilds keep the scroll position** — new
  `frij_page_settle_at` restores the saved offset (Sync now / rescan / todo
  refresh no longer jump to the top).
- **`frij_prompt_screen`** — full-screen confirm/notice in the result-screen
  style (big colored ring + symbol, title, message, 1–2 pill actions; primary =
  app accent or `FRIJ_DANGER`). Replaces the modal `frij_confirm` (removed) for
  Reset settings (↻), Erase all data (🗑) and Scoreboard reset. Back/Cancel
  dismiss without firing.
- About hero final form: the halo is the hero ROW's own padded background (a
  radial gradient centered on the clover in px) — the two earlier structures
  (overflow child, oversized box) both clipped to a square. Wordmark at ~80% of
  the clover height, tight SP_S gap, padding compensated so the hero centers.
- Prompt actions are **round icon buttons**: ✕ (neutral) and ✓ (accent/danger)
  via frij_circle_button, replacing the text pills on two-action prompts.
- **Blur fixed at the source**: regenerated all SF cuts from the macOS system
  variable font (fontTools instancing, see fonts README) and added two new ones —
  `frij_sf_header` (22px, the app header title; replaces the 0.875× scale hack)
  and `frij_sf_logo` (56px "Frij" wordmark; replaces the ~1.7× upscale). The big
  prompt/result ring glyphs now use native `montserrat_40` (`FRIJ_FONT_SYMBOL_L`)
  instead of a 2× scale. Remaining softness in the emulator is macOS Retina
  upscaling of the 466px SDL window — the device renders 1:1.
- Polish batch: prompt/numpad overlays **fade in** (no snap to black); About hero
  margins halved; **Sync now + Last sync merged** into one row (tap to sync, the
  time sits where the chevron was); **battery "Text" bug fixed** (home bound to
  the battery subjects before they were initialized — init moved ahead of app
  registration); **auto-sync is now periodic** (pulls synced keys every 5 min
  while on; was boot-only); success toasts drop the green border (failures keep
  red); **no more Milk/Eggs seed todos** — empty list is the default; the header
  action icon **fades out** too (and in-flight fades cancel cleanly).

## 2026-06-11 — improvement pass 3 (10: nav feel + fixes)

- **Sleep shield fixes a device bug**: the touch controller stays live while the
  panel sleeps, so the waking tap used to click whatever was under the finger.
  The sleep manager now owns a full-screen clickable shade (both targets); the
  emulator's display stub simplified away.
- **Fling to navigate**: a fast short flick up/down commits the layer transition
  — no more dragging 30% of the screen for a quick open/close.
- **Animated jumps**: tapping a page dot (or Back-to-clock on home) slides
  direction-aware like a swipe instead of teleporting.
- **Action icon spins** once when tapped (all current actions are refresh-style).
- **Stopwatch screen shows Ready / Running / Paused** under the time.
- **Add-by-voice button breathes** (gentle infinite pulse) to invite the tap.
- **Slider release buzz** — committing a value gives a SELECT haptic.
- **Stagger delay caps at ~8 rows** (long lists stopped reading as lag).
- **Launcher hygiene**: three duplicate transition-done handlers merged into one
  `done_back_home`.
- **Emulator window is titled "Frij"** (was "LVGL Simulator").

## 2026-06-11 — improvement pass 2 (10: details + hygiene)

- **Stopwatch timers pause when stopped** — the 33ms refresh only runs while the
  watch runs (idle page costs ~nothing now).
- **Newest lap row pops in** after tapping Lap.
- **Reset / Erase refresh About in place** (Last sync / rows no longer stale).
- **Swipe-up hint retires after ~5 boots** (`hint_seen`) — learned gestures don't
  need a permanent bobbing chevron.
- **Read-only value rows are inert** — no press-darken/haptic on rows that do
  nothing (Battery/Storage/Last sync).
- **Numpad ✓ ignores empty input** (tap haptic instead of submitting "").
- **Toggle knob slide uses the house FRIJ_ANIM_MS** like all other motion.
- **`FRIJ_VERSION`** defined once (app.h), About uses it.
- Hygiene: Wi-Fi selection kinds are a named enum (was magic 0/1/2); the
  snapshot tool now warns on an unknown `FRIJ_SNAP` key and lists valid ones.

## 2026-06-11 — improvement pass (10: UX, micro-anim, hygiene)

- **Sliders persist on release only**: dragging Brightness/Volume/Sleep applied
  AND saved (cloud-push included) on every tick — now they apply live and save
  once on release (key carried in the slider's user_data).
- **Wi-Fi is wired at boot**: `frij_wifi_init()` was never called and the master
  switch wasn't persisted. Boot now restores the saved `wifi_on` state; on device
  the auto-reconnect is **fire-and-forget** (no 8s boot stall).
- **New `frij_toggle_row`** ui component (whole-row tap flips the switch) —
  replaces three hand-rolled copies (Settings toggles + the Wi-Fi master row).
- **Todo refresh is non-blocking**: the header refresh did a blocking cloud pull
  on the UI thread (froze gestures); now async pull + "Syncing..." + a one-shot
  timer rebuild.
- **Header entrance**: the back/title/action bar fades + rises in on layer open.
- **Header action icon fades in** when it appears on a screen change.
- **Result-screen ring pops in** (overshoot scale).
- **Numpad dots pop** as digits are entered.
- **Carousel page dot morphs** into the active pill (width animates, no snap).
- **Status toasts get a hairline green/red border** for a faster read.
- All motion respects the reduce-motion ("Animations") setting.

## 2026-06-11 — settings polish: logo, storage, Wi-Fi flow (inspiration pass)

- **`frij_logo`** — reusable three-circle clover (pink/purple/violet, new
  `FRIJ_PINK` token) + "Frij" wordmark; About's hero now shows it. Each orb has a
  vertical light→deep gradient and the clover sits on a soft radial glow.
- **"Networks" section heading** on the Wi-Fi screen (same grouped-list style as
  Display/Sound).
- **Erase-all copy is honest**: "Clears this device. Synced data re-downloads."
  (cloud rows — and Keep-owned todos — come back by design).
- **Storage readout**: new `system/storage` service ("12.3 MB free" — flash chip
  vs firmware size on device, believable mock on the emulator) + a Storage row
  in About.
- **Wi-Fi rows restyled** (per the inspiration shots): accent ✓ + accent name on
  the connected network; right side shows a small "Saved" hint, a **lock** for
  secured networks (`frij_lock_icon` — drawn, the symbol font has no padlock) and
  the signal glyph. Header gains a **rescan** action on the Network screen.
- **Numpad v2**: bottom row is now ⌫ / 0 / ✓ — backspace with **hold-to-repeat**;
  cancel moved to the **Back** action (keypad registers with the modal stack);
  two-line title ("Enter password for\n<ssid>"); dot display capped at 14.
- **`frij_result_screen`** — reusable flow-conclusion overlay: big green-check
  (or red-cross) ring + title + subtitle + Done pill, Back-dismissable. The Wi-Fi
  password flow now ends with "Connected · <ssid> · Done" instead of a toast.
- CLAUDE.md: new rule — every new UI pattern lands in `src/ui/` as a reusable
  component.

## 2026-06-11 — custom round numeric keypad

- Dropped LVGL's `lv_keyboard` (light-themed + rectangular → ran off the round
  screen). Built our own **`frij_numpad_prompt`**: a full-screen, on-brand keypad
  — title + masked dots display + a 3×4 ring of round keys (1–9, ✕ cancel, 0, ✓
  confirm) made from `frij_circle_button`, centered so every key stays inside the
  circle. ✓ returns the digits; ✕ cancels. Wi-Fi password now uses it.

## 2026-06-10 — Wi-Fi fully wired + on-screen keyboard

- **Wi-Fi password entry** (originally LVGL `lv_keyboard`; replaced 06-11 by the
  custom `frij_numpad_prompt` above).
- **Wi-Fi is real**: tapping a new secured network opens the keyboard for the
  password → `frij_wifi_connect(ssid, pw)`; saved networks reconnect without
  asking; open networks join directly. Connect result drives the toast (✓/✗).
- **Device backend** (`wifi.cpp`): Arduino `WiFi` scan/connect + credentials in
  NVS via `Preferences` (one saved "home" network — NVS keys cap at 15 chars, so
  not keyed by SSID). Auto-reconnects the saved network when Wi-Fi is switched
  on. Emulator keeps its in-memory mock. (scan/connect block briefly — fine for
  user-initiated actions; could be made async later.)
- Process: the agent now ends each round with a **"What changed & how to test"**
  section (manual emulator steps) — see CLAUDE.md.

## 2026-06-10 — hardware wiring (device features made real)

- **Device bring-up via M5Unified**: `src/main.cpp` now calls `M5.begin()` (panel
  reset via M5IOE1, touch, BMI270 IMU, PMIC) and hands `M5.Display` to the LVGL
  port — replaces the bare `gfx.init()` and fixes the panel-reset blocker. `device`
  env gains the M5Unified dep + 16MB partition. *(Not built in CI here — no ESP
  toolchain in this env; verify on flash. The device target does compile through
  our sources.)*
- **Sleep is real + cross-platform**: new `system/sleep` idle manager (watches
  LVGL inactivity, reads the "Sleep" minutes) turns the panel off and wakes on the
  next touch. New `system/display` does the on/off (black overlay on emulator,
  `M5.Display.sleep()`/`wakeup()` + brightness restore on device). Works on the
  emulator now.
- **Brightness** now drives `M5.Display.setBrightness()` on device (was already
  wired; repointed off the removed `gfx` global).
- **Raise-to-wake**: new `system/motion` polls the BMI270 (`M5.Imu`) each loop and
  signals input activity on a wrist raise → wakes the panel. Device-only; Z-axis
  threshold tunable on hardware.
- **Volume + touch sounds**: new `system/audio` — `frij_set_volume` →
  `M5.Speaker` (ES8311); a short press-click when "Touch sounds" is on (played
  from the shared press handler). Both applied at boot.
- **Vibration**: `frij_haptic` now pulses the motor via `M5.Power.setVibration`
  on device (per-kind pulse lengths). No more device TODOs in General settings.
- All settings apply at boot (brightness/haptics/animations/volume/touch-sounds);
  the only remaining device stub is Network Wi-Fi (needs on-screen text entry).

## 2026-06-09 — new app: Scoreboard

- **Scoreboard** — a two-player score keeper for board/card game nights. The whole
  area below the header is a **full-bleed left/right split** of two *transparent*
  touch halves (Farzin blue / Farah amber) with a thin center divider + a "VS"
  chip — no panels, borders or buttons. **Tap a half to score +1, hold it to take
  one back** (+1 fires on `SHORT_CLICKED` so a long-press's -1 isn't undone on
  release). A header **reset** action (danger-confirmed modal) zeroes both. New
  `frij_page_full_bleed()` lets a screen skip the header safe-area inset +
  auto-centering so it owns the whole area below the header. Scores persist
  in the shared store under `sb_a` / `sb_b`, so
  they **sync via the cloud** like the other apps and survive leaving the app
  mid-game. Glance shows "Farzin 3 – 2 Farah" + who leads. Registered between
  Stopwatch and Counter. Blue accent. Auto-sync + "Sync now" pull the keys.

## 2026-06-09 — refinement pass (10 bigger changes)

- **Reduce-motion is now complete**: the Animations toggle also gates the modal
  pop, toast rise, slider/progress-ring sweep, check pop, counter pop and the
  all-done celebration (haptics still fire). Was only gating entrance/stagger.
- **Brightness visibly dims the emulator**: a black scrim on the top layer tracks
  the inverse of the slider (gentle — 80% ≈ 8% dim), faking the backlight the
  emulator lacks.
- **Tappable page dots**: tap a carousel dot to jump straight to that page
  (enlarged hit area).
- **Stopwatch lap splits**: each lap shows its split (since the previous lap)
  next to the cumulative time.
- **Stopwatch fastest/slowest** laps are colored green / amber (2+ laps).
- **Bigger header tap targets**: back/action icons get a 10px ext-click area.
- **Section labels restyled**: UPPERCASE + letter-spacing + small font for a
  cleaner grouped-list heading.
- **Counter press-and-hold** to repeat ±; the value is saved once on release.
- **Toast tap-to-dismiss**: tap a toast to fade it out immediately.
- **Gliding seconds ring**: the home seconds hand animates between ticks instead
  of jumping (skips the 59→0 wrap; respects reduce-motion).

## 2026-06-09 — feature pass (10 bigger changes)

- **New Stopwatch app** (on-brand for the dev kit): MM:SS.cs readout, Start/Stop,
  Lap list, and Reset. Timing lives in module state so it keeps running while you
  navigate away; not persisted across restarts. Registered between Todo and
  Counter. Green accent.
- **Reduce-motion setting**: a new "Animations" toggle (Settings ▸ General). When
  off, entrance/stagger animations snap to their final state. Applied at boot.
- **Danger accent**: new `FRIJ_DANGER` red; Reset/Erase confirm buttons use it so
  destructive actions read as dangerous (were app-purple).
- **Toast status variants**: `frij_toast_status(text, ok)` shows a green check or
  red cross. Wired to Reset/Erase and Wi-Fi connect/forget/disconnect.
- **Low-battery warning**: the home + About battery readout turns amber when
  ≤15% and unplugged.
- **Sync now** refreshes the About page in place (Last-sync updates immediately).
- **Counter**: long-press the number to reset it to zero (haptic + toast).
- **Todo all-done celebration**: at 100% the progress ring gives a one-shot pulse
  + a success buzz.
- **Swipe-up affordance**: openable glances show a faint, gently-bobbing up-chevron
  (`frij_swipe_hint`) hinting the card opens on swipe-up. Respects reduce-motion.
- **Relative "ago" time**: new `frij_format_relative`; About's Last sync now reads
  "Just now" / "5m ago" / "2d ago" (clock time past a week).

## 2026-06-09 — polish pass (5 improvements)

- **Home battery stays live**: the watch-face battery readout is now re-read each
  tick (was built once → went stale on a long-running face).
- **Counter value pops**: `+`/`−` now give the number a quick scale-pop so a tap
  reads as a change, not a silent swap.
- **Progress inner fades in**: the %/sub text fades + rises in as the ring sweeps
  (was a flat appearance).
- **Rounded percentage**: Todo progress rounds instead of truncating (2/3 → 67%,
  not 66%).
- **Toast wraps long text**: the snackbar caps its label width and wraps, so long
  messages (e.g. "Connected to <long SSID>") no longer clip the round edge.

## 2026-06-09 — UI/UX motion pass (5 improvements)

- **Strikethrough done todos**: checking an item now strikes the text through and
  dims it (was dim-only), on toggle and on list build.
- **Sliders sweep to value**: Settings sliders animate their fill 0 → value on
  open (`anim_duration` + `LV_ANIM_ON`) instead of snapping.
- **Progress ring sweep**: the big ring fills 0 → % with ease-out on open.
- **Toast rises in**: the snackbar translates up ~16px while fading in (proper
  snackbar feel), then holds + fades out.
- **Modal scale-in pop**: confirm / action-sheet cards scale 0.92 → 1 (center
  pivot) alongside the fade + rise, so dialogs pop rather than appear.

## 2026-06-09 — quality pass (5 improvements)

- **Empty state** uses a neutral muted glyph instead of "+" (which implied add —
  wrong for "No networks").
- New shared **`frij_value_row`** (label + muted value); Settings' About uses it.
- New shared **`frij_circle_button`** (press-pop + haptic); Counter's ± and the
  Todo add button now share it (dropped the duplicated `round_button`).
- **Robustness**: Todo/Settings clear their cached page pointer
  (`s_list_col`/`s_net_col`) on page delete, so a later refresh can't touch freed
  memory.
- Docs: added `bridge/` to the CLAUDE.md structure table; ui README updated.

## 2026-06-09 — Todo app: progress + add screens, focus glance

- **Glance → "Up next"**: shows the next unchecked item big + "N left" (or
  "All done"), instead of the small ring.
- **New Progress screen**: a large on-brand ring with the % in the middle +
  "N of M done" (replaces the tiny stats screen).
- **New Add-by-voice screen**: a big accent mic/＋ button + "Tap to speak" —
  placeholder UI only (no STT yet).
- Checklist screen unchanged. Screens are now: checklist / progress / add.
- Stored item text cap raised 39 → 63 (device `TEXT_LEN` 64): the glance shows
  the **full** todo (wrapped, padded), the list still trims to one line.

## 2026-06-09 — Keep ⇄ Frij two-way (done-state)

- The bridge now syncs **check/uncheck both ways**: a 3-way merge against a saved
  base row (`store:todo_base`) decides which side moved per item; **checked wins**
  on conflict. Watch toggles are written back to Keep (`keep.sync()`); Keep still
  owns structure (add/remove). Matches items by cleaned text. Rides the ~10-min
  cron — not realtime; add/remove stay in Keep (voice-add later).
- Reverted the on-demand `--serve`/`frij_keep_sync` trigger (needed an always-on
  host that can't live on the watch) — the Todo refresh is a plain cloud pull.

## 2026-06-09 — Google Keep → todo bridge (read-only)

- New **`bridge/`**: an off-device Python sync (`keep_to_frij.py`) that reads a
  shared Google Keep checklist via the unofficial `gkeepapi` and upserts it into
  the `store:todo` row the device already pulls — so **read-only needs no
  firmware change**. Includes a master-token helper, an offline mapping test
  (passing), `requirements.txt`, and `.env.example`.
- **GitHub Actions cron** (`.github/workflows/keep-sync.yml`) runs it ~every
  10 min + on demand, driven by repo secrets.
- Docs: `bridge/README.md` (what to provide + token steps), STORAGE.md +
  ROADMAP integration sections.
- The bridge **strips emoji** from items (the device font has no emoji glyphs),
  auto-loads the repo `.env` (one consolidated `.env.example`), and uses
  `keep.resume()` so it works on Python 3.9's `gkeepapi` 0.14.x and newer.

## 2026-06-09 — header polish + doc audit

- **Header tint even darker on top** — 5-stop gradient: the upper ~3/4 is fully
  transparent (shows the dark background), with a single low accent band behind
  the title (`LV_GRADIENT_MAX_STOPS` → 5).
- **Borderless header buttons** — the back/action icons lost their circular
  background; the icon now scale-pops on press (no plate).
- **Doc cross-check for agents** — `src/README` + `ARCHITECTURE` now list `core/`
  and the full `system/` services, and `apps/README` gained a "Building blocks
  you can reuse" palette (components, modals, anim, datetime, store, services) so
  new work composes existing pieces instead of re-deriving them.

## 2026-06-09 — restructure + tint

- **Header tint**: pushed the accent peak lower so the top edge stays
  dark/transparent longer (more "fades into black on top").
- **Package cleanup**: moved the non-UI `datetime` helper out of `ui/` into a new
  **`core/`** package (app-agnostic, non-UI logic). Extracted motion into
  **`ui/anim.*`** (entrance/stagger + shared lv_anim exec callbacks), out of
  `components`. Updated READMEs + the structure table.
- Recorded a **Settings-expansion assessment** in ROADMAP (Bluetooth: yes via a
  BLE mock; GPS: no hardware; Date & time, Sound, Accessibility, Power).

## 2026-06-09 — polish pass 5 (time util + tint)

- **Shared time formatting** (`ui/datetime`: `frij_clock_is_24h` +
  `frij_format_time`) — one source of truth for the 24-hour setting. Last sync
  and the watch face both go through it, so the toggle is respected everywhere.
- **Header tint fades into the background at the top** — 3-stop gradient
  (transparent → accent → transparent) instead of a hard accent band at the edge
  (`LV_GRADIENT_MAX_STOPS` bumped to 3).

## 2026-06-09 — polish pass 4 (layout fixes)

- **Row content vertically centered** — surface rows now use a fixed
  `FRIJ_ROW_H` (was content-height + min-height, which left text near the top).
- **Wi-Fi-off hint centered** — the "turn on Wi-Fi" text floats at the page
  center while the toggle stays pinned at the top.
- **Modal buttons no longer clip on press** — neutralized the default theme's
  grow-on-press transform (it poked past the card's clip); subtle dim instead.
- **More space around the About hero** (Frij + version).

## 2026-06-09 — polish pass 3 (consistency + motion + hardware)

- **Rethought the header glow** — replaced the accent ellipse with a restrained
  top wash that fades down (`frij_top_tint`).
- **Uniform row height** (`FRIJ_ROW_H`) so info rows / action rows / Wi-Fi rows /
  sliders all line up.
- **Modal close now fades out** (was an instant pop); shared `modal_close`
  animates the dim + card out, then deletes.
- **Reusable stagger** (`frij_stagger_in`) — todo, settings and the Wi-Fi list
  all get the staggered fade-in (was todo-only).
- **24-hour toggle reflects live** — the watch face re-reads the setting each
  tick instead of caching it at build.
- **Wi-Fi off no longer jumps** — the toggle stays pinned at the top
  (`frij_page_pin_top`) with a "turn on Wi-Fi…" empty-state hint.
- **About hero** gets breathing room around the name/version.
- **New hardware-backed settings**: Raise to wake (BMI270 IMU) and Touch sounds
  (ES8311 codec) — stored now, wired on device later.

## 2026-06-09 — polish pass 2 (fixes + sync/erase)

- **Fixed the toast** — it never showed: `fade_in` and `fade_out` fought over the
  same opacity property, leaving it transparent. Now sequenced (fade in → hold →
  fade out), with a single tracked instance.
- **Battery moved** under the date on the watch face, in a smaller font
  (`FRIJ_FONT_SMALL` = Montserrat 14).
- **Wi-Fi feedback** — Connect/Disconnect/Forget now show a toast; Network shows
  an empty state when no networks are visible.
- **Auto-sync wired** — when enabled, the apps' cloud data is pulled in the
  background at boot. About shows a **Last sync** time (stamped on Sync now).
- **Erase all data** — About gains a confirm-gated factory reset
  (`frij_store_clear` wipes the local cache; defaults return on next read).

## 2026-06-09 — polish pass (battery, UX, feedback)

- **Battery** — new `system/battery` service (emulator mock + device TODO). The
  watch face shows a battery glyph + % at 12 o'clock; Settings → About has a
  Battery row.
- **Back closes modals first** — the hardware Back / header back now dismisses an
  open dialog or action sheet before navigating (`frij_modal_close_top`).
- **Grouped Settings** — General is split under "Display / Sound / Preferences"
  section headings (new reusable `frij_section_label`).
- **Toasts** — new `frij_toast` snackbar (built on LVGL's fade/auto-delete
  helpers); "Sync now" and "Reset" now show brief confirmation.

## 2026-06-09

- **Wi-Fi settings** — new `system/wifi` service (interface + emulator mock) and
  a real **Network** screen: master toggle, scanned network list (signal /
  Connected / Saved), and a tap **action sheet** to Connect / Disconnect /
  Forget. Device backend is a stub for later (esp_wifi).
- New **`frij_action_sheet`** component (animated modal, stacked options +
  Cancel). **Modals now animate** in (dim fades, card rises).
- **Header glow** is now a wide, short ellipse behind the title (was a square).
- **Slider cards show their value** on the right (e.g. `80%`, `5 min`).
- Fixed **Brightness disappearing**: a centered, overflowing list hid its top
  rows. Pages now center only when they fit and top-align when they overflow
  (new `frij_page_settle`). Added `frij_page_under_header` safe-area helper +
  a little breathing room under the header.
- Moved **Sync now** to About (next to Reset); Network is Wi-Fi only.

## 2026-06-09

- **Smaller header glow** — the accent halo behind the title is now compact
  (~52% of the screen) instead of a big wash.
- **App-page content is screen-centered** — the header no longer pushes content
  low or clips its top; matching bottom padding balances the header so the
  centered column lands at the true screen center.
- New **`frij_confirm`** confirmation dialog (dimmed backdrop + Cancel/Confirm
  card); **Settings → Reset** now asks before wiping settings.
- Cleanup: extracted typed store accessors (`frij_store_load_int/save_int/
  load_bool/save_bool`) and removed the per-app copies (settings, counter, home,
  boot). Fixed stale comments (settings screen map, carousel dots).

## 2026-06-09

- On app pages the accent **glow now sits behind the header title** (on the
  layer), not as a full background.
- Fixed app pages scrolling when content fit: the full-size glow was inflating
  the scroll area. The glow is now **FLOATING** (excluded from scroll bounds), so
  short content stays centered below the header and doesn't scroll.
- Settings **toggle rows are fully tappable** (the whole card flips the switch).
- Fleshed out **Settings**: General gained Sleep timeout + Auto-sync; Network got
  a "Sync now" action; About got "Reset settings". Added `frij_action_row`.

## 2026-06-09

- Page dots **fade out when idle** again (re-added the timer) while staying on
  top of the pages — show on swipe, fade after ~1.4s.
- **Settings** merged Display + Sound into one **General** page; brightness +
  volume now use a new **`frij_slider_row`** card (the whole card is the slider,
  drag to set, accent fill shows the amount).
- **Pages scroll** generically: `frij_page` is now a centered, vertically
  scrollable column. The launcher only treats a vertical swipe as Back when the
  content is already at the scroll edge (otherwise it scrolls). Content sits
  below the persistent header.

## 2026-06-09

- Fixed gradient/glow **banding**: switched `LV_COLOR_DEPTH` 16 → **32** (RGB565
  banded badly) and enabled complex gradients; the glow is now a **true radial
  gradient** (accent → transparent) instead of stacked circles. Smooth.
  (Device: the port flush must match the panel's pixel format — see HARDWARE.md.)

- Added a soft per-app **accent glow** behind every glance + app screen
  (`frij_glow`, layered translucent circles): Home/Settings purple, Todo amber,
  Counter blue. More color/depth without a heavy radial-gradient dependency.

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
