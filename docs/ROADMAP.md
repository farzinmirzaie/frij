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

- [ ] **P0 — Foundation (now)**: minimal emulator build runs; single hello screen. ✅ build base done.
- [ ] **P1 — App framework**: home screen with app icons; screen navigation (open app / back); shared theme. Round-layout helpers.
- [ ] **P2 — First app (Todos)**: in-memory list, add/toggle/delete via touch.
- [ ] **P3 — Persistence**: save state (NVS/flash on device; file on emulator).
- [ ] **P4 — More apps**: grocery, reminders, photos.
- [ ] **P5 — Device bring-up**: resolve M5GFX StopWatch board support, flash, test touch/buttons/IMU.

## Open questions

- M5GFX StopWatch (CO5300) board support — exists upstream yet? Blocks P5.
- Where do photos/data live? On-device flash vs Wi-Fi sync vs SD.
- Navigation model: icon grid vs swipe carousel (round screen friendly?).

## Next step

Start **P1**: design the home screen + navigation pattern. Keep it simple and
readable — owner is learning C/LVGL.
