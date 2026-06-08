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
- [ ] **Launcher B**: real Settings layer (swipe-down) — brightness; wifi is device-only.
- [ ] **Launcher C**: multi-screen app carousels + glance data refresh (`on_show`).
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

## Open questions
- Where do photos/data live? On-device flash vs Wi-Fi sync vs SD.

## Next step

**Launcher B** (Settings + brightness) or **P2** (real Todo editing) — owner's pick.
