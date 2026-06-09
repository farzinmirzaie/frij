# Roadmap

## Vision

A round touch screen running a small set of daily-life apps. Glanceable home
screen; tap to open an app; back to home. Hardware-agnostic — the same apps
target whatever board we run on (see [HARDWARE.md](HARDWARE.md)).

## Apps (planned)

- **Todos** — quick check-off list.
- **Reminders** — time/day-based nudges.
- **Grocery list** — add/remove items, clear bought.
- **Photos** — slideshow of family pics.
- **Clock / weather** — ambient home-screen widgets (nice-to-have).

## Phases

- [x] **P0 — Foundation**: minimal emulator build runs.
- [x] **P1 — App framework**: isolated app contract (`app.h`) + registry.
- [x] **Launcher A**: looping glance carousel, swipe-up to open an app, Back
  via key/button, layer state machine. See [../src/launcher/README.md](../src/launcher/README.md).
- [x] **Launcher B**: Settings (brightness slider + 24h toggle + about, persisted).
- [x] **Home app**: watch-face tile (time + date) registered first as the landing glance.
- [x] **Design system**: `ui/theme.h` tokens + components; per-app accent colors.
- [ ] **Launcher C**: glance data refresh / live tiles (`on_show` hook).
- [ ] **P2 — Flesh out Todos**: real add/toggle/delete (currently a fixed checklist).
- [x] **Storage 1**: shared `frij_store` (key→JSON) + emulator file backend;
  Counter persists. See [STORAGE.md](STORAGE.md).
- [ ] **Storage 2**: Supabase backend on device (wifi, scoped key, offline queue).
- [ ] **Storage 3**: web app reading the same data (Google-Keep-style).
- [ ] **P3 — Persistence**: migrate remaining app state onto `frij_store`.
- [ ] **P4 — More apps**: grocery, reminders, photos.
- [ ] **P5 — Device bring-up**: `esp32s3box` board + M5Unified; init the M5IOE1
  expander (panel reset) around `gfx.init()`; map Back to Key A (G2); flash, test
  touch/buttons. M5GFX/M5Unified support the board — see [HARDWARE.md](HARDWARE.md).

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

**Settings polish (further)**
- On-device Wi-Fi password entry (needs a keyboard/numpad component).
- Per-app settings pages (e.g. Todo: clear completed).
- Date/time + timezone, units, theme/accent picker, factory-reset of all data.
- Storage usage / "about device" (uptime, free space) read from real services.

## Open questions
- Where do photos/data live? On-device flash vs Wi-Fi sync vs SD.

## Next step

**Launcher B** (Settings + brightness) or **P2** (real Todo editing) — owner's pick.
