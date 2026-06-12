# Roadmap

## Vision

A round touch screen running a small set of daily-life apps. Glanceable home
screen; tap to open an app; back to home. Hardware-agnostic — the same apps
target whatever board we run on (see [HARDWARE.md](HARDWARE.md)).

## Apps (planned)

- **Todos** — quick check-off list.
- **Stopwatch** — count-up timer with laps (shipped; on-brand for the dev kit).
- **Scoreboard** — two-player, cloud-synced score keeper for game nights (shipped).
- **Events** — countdowns to the family Google Calendar's events, via the
  iCal bridge (shipped).
- **Reminders** — time/day-based nudges.
- **Grocery list** — add/remove items, clear bought.
- **Photos** — slideshow of family pics.
- **Clock / weather** — ambient home-screen widgets (nice-to-have).
- **Frij AI** — voice assistant. UI shell + backend shipped (push-to-talk
  overlay; Supabase Edge Function "ask" -> Gemini with store tools — see
  [AI.md](AI.md)) — incl. device mic capture (M5.Mic -> audio POST,
  compile-only, verify on flash). Next: bridge pickup of `todo_inbox`, then
  flash + tune the mic path on hardware.

## Phases

- [x] **P0 — Foundation**: minimal emulator build runs.
- [x] **P1 — App framework**: isolated app contract (`app.h`) + registry.
- [x] **Launcher A**: looping glance carousel, swipe-up to open an app, Back
  via key/button, layer state machine. See [../src/launcher/README.md](../src/launcher/README.md).
- [x] **Launcher B**: Settings (brightness slider + 24h toggle + about, persisted).
- [x] **Home app**: watch-face tile (time + date) registered first as the landing glance.
- [x] **Design system**: `ui/theme.h` tokens + components; per-app accent colors.
- [x] **Launcher C**: glances refresh when you return home (`frij_carousel_refresh`).
- [ ] **P2 — Flesh out Todos**: on-device add/delete (today: toggle on device,
  add/remove via Keep through the bridge; AI voice-add queues to `todo_inbox`).
- [x] **Storage 1**: shared `frij_store` (key→JSON) + emulator file backend;
  Counter persists. See [STORAGE.md](STORAGE.md).
- [ ] **Storage 2**: Supabase backend on device (wifi, scoped key, offline queue).
- [ ] **Storage 3**: web app reading the same data (Google-Keep-style).
- [x] **Keep sync**: off-device bridge syncs a shared Google Keep list with
  `store:todo` ([`../bridge/`](../bridge/README.md)). **Done-state is two-way**
  (3-way merge + `todo_base`, checked-wins); Keep owns add/remove. Live on the
  GitHub cron. Next: on-device **voice add** → write new items back to Keep.
- [ ] **P3 — Persistence**: migrate remaining app state onto `frij_store`.
- [x] **Calendar sync**: `bridge/calendar_to_frij.py` mirrors the family
  Google Calendar (secret iCal URL) into `store:events`; the Events app
  renders countdowns. Hourly GitHub cron.
- [ ] **P4 — More apps**: grocery, reminders, photos.
- [ ] **P5 — Device bring-up**: code side largely DONE (M5Unified `M5.begin` in
  main.cpp handles the M5IOE1 panel reset; brightness/sleep/raise-to-wake/audio/
  vibration/Wi-Fi all wired; device target compiles). Remaining: **flash + verify
  on hardware**, map Back to Key A (G2), tune `RAISE_Z`, device store backend.
  See [HARDWARE.md](HARDWARE.md).

## Polish & tech notes (assessment)

Ideas surfaced while polishing; not yet committed to a phase.

**Custom code vs libraries**
- **Reactive settings via LVGL Observer** (`lv_subject`/`lv_observer`, built into
  v9): today the Home clock re-reads `clock24` from the store on each build, and
  brightness/volume apply imperatively. A subject per setting would let widgets
  bind to values and update live without manual re-reads. Worth adopting if
  settings grow.
- **JSON** already uses ArduinoJson (good). **HTTP** uses libcurl on the
  emulator; the device will use `WiFiClientSecure`. No change needed.
- **Carousel / radial glow** are intentionally custom — LVGL has no equivalent
  and they're small. Keep.
- Prefer LVGL built-ins where they exist (we now use `lv_obj_fade_in/out` +
  `lv_obj_delete_delayed` for toasts instead of hand-rolled timers).

**UX/UI ideas (cheap wins)**
- Live brightness preview while dragging (apply on `VALUE_CHANGED`, persist on
  release) — partly there.
- A persistent top status sliver (time + battery) across app screens.
- Pull-to-refresh affordance on cloud screens instead of a Settings button.
- Sync state: show a spinner/`lv_spinner` while a pull is in flight, then a toast.
- Empty/error states for the network list (no networks, connect failure).

**More Settings — what fits the StopWatch hardware**
- **Bluetooth: yes, eventually.** The ESP32-S3 has BLE. A Network-style screen
  (master toggle + paired/nearby list + connect/forget via the action sheet) is
  a natural fit once we need a phone link/notifications. Build it on a
  `system/bluetooth` mock first, exactly like Wi-Fi.
- **GPS: no.** There is no GNSS module on the StopWatch — skip it. (Location, if
  ever needed, would have to come from a paired phone over BLE.)
- **Date & time** (RX8130CE RTC): set time/zone manually + NTP sync over Wi-Fi.
  High value — pairs with the new 24-hour setting and `core/datetime`.
- **Sound** (ES8311): once a `system/audio` service exists, wire Touch sounds +
  a UI-sounds toggle (today it's a stored pref only).
- **Display**: always-on / raise-to-wake (BMI270) sensitivity, auto-dim.
- **Accessibility**: larger text, reduce motion (gate `anim` durations).
- **Power** (M5PM1): battery-saver mode, charge limit.

**Settings polish (further)**
- On-device Wi-Fi password entry (needs a keyboard/numpad component).
- Per-app settings pages (e.g. Todo: clear completed).
- Date/time + timezone, units, theme/accent picker, factory-reset of all data.
- Storage usage / "about device" (uptime, free space) read from real services.

## Open questions
- Where do photos/data live? On-device flash vs Wi-Fi sync vs SD.

## Next step

**P5 device bring-up** — flash + verify on hardware (panel, touch, raise-to-wake,
Wi-Fi, the Frij AI mic path), then the device `frij_store` backend (Storage 2)
and bridge pickup of `todo_inbox` (AI voice-adds → Keep). Owner's pick.
