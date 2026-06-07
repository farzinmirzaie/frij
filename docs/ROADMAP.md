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
  via key/button, layer state machine. See [LAUNCHER.md](LAUNCHER.md).
- [ ] **Launcher B**: real Settings layer (swipe-down) — brightness; wifi is device-only.
- [ ] **Launcher C**: multi-screen app carousels + glance data refresh (`on_show`).
- [ ] **P2 — Flesh out Todos**: real add/toggle/delete (currently a fixed checklist).
- [ ] **P3 — Persistence**: save state (NVS/flash on device; file on emulator).
- [ ] **P4 — More apps**: grocery, reminders, photos.
- [ ] **P5 — Device bring-up**: confirm M5GFX panel support for the target board, flash, test touch/buttons.

## Open questions

- Does M5GFX support the target's round panel yet? Blocks P5 (see [HARDWARE.md](HARDWARE.md)).
- Where do photos/data live? On-device flash vs Wi-Fi sync vs SD.

## Next step

**Launcher B** (Settings + brightness) or **P2** (real Todo editing) — owner's pick.
