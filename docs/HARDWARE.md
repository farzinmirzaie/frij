# Hardware targets

Frij is **target-agnostic**. Apps are pure LVGL and don't know the board; only
`src/utility/` (the M5GFX port) is board-specific. Adding a board = a new
`[env:...]` in `platformio.ini` + the right `M5GFX_BOARD` value. The app code is
untouched.

This file is just reference notes for whichever boards we actually run on.

## Emulator (daily driver)

- PC + SDL2, env `emulator_Dial`. No hardware needed.
- Uses a round 240×240 frame as a stand-in. Design for a **round** screen.

## First dev board (current)

The board this was started on — an ESP32-S3 round-display dev kit:

- MCU: ESP32-S3 (16MB flash, 8MB PSRAM)
- Display: round AMOLED, ~466×466, driver CO5300
- Input: capacitive touch (CST820B), 2 programmable buttons + power, IMU (BMI270)

Status: WIP. M5GFX has no board enum for this panel yet, so the `device` env is
a placeholder — verify panel support before flashing.

## Adding another board

1. Copy the `[env:device]` block in `platformio.ini`, rename it.
2. Set `board = ...` and `-D M5GFX_BOARD=board_...` for the new target.
3. Confirm M5GFX supports the panel; adjust `src/utility/` only if needed.
4. Apps and the launcher need no changes.
