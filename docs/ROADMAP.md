# Roadmap

## Vision

A fridge-mounted touch screen running a small set of daily-life apps on a round
466×466 display. Glanceable home screen; tap to open an app; back to home.

## Apps (planned)

- **Todos** — quick check-off list.
- **Reminders** — time/day-based nudges.
- **Grocery list** — add/remove items, clear bought.
- **Photos** — slideshow of family pics.
- **Clock / weather** — ambient home-screen widgets (nice-to-have).

## Phases

- [x] **P0 — Foundation**: minimal emulator build runs.
- [x] **P1 — App framework**: launcher home screen with app tiles; open/back
  navigation; isolated app contract (`app.h`) + registry; 2 demo apps
  (Todo, Counter) wired via `apps.cpp`.
- [ ] **P2 — Flesh out Todos**: real add/toggle/delete (the demo is read-only checkboxes).
- [ ] **P3 — Persistence**: save state (NVS/flash on device; file on emulator).
- [ ] **P4 — More apps**: grocery, reminders, photos.
- [ ] **P5 — Device bring-up**: resolve M5GFX StopWatch board support, flash, test touch/buttons/IMU.

## Open questions

- M5GFX StopWatch (CO5300) board support — exists upstream yet? Blocks P5.
- Where do photos/data live? On-device flash vs Wi-Fi sync vs SD.
- Navigation model: icon grid vs swipe carousel (round screen friendly?).

## Next step

**P2**: make the Todo app actually add/remove items (currently a fixed checkbox
list). Then add persistence (P3) so todos survive a restart.
