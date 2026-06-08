# Hardware targets

Frij is **target-agnostic**. Apps are pure LVGL and don't know the board; only
`src/utility/` (the M5GFX port) is board-specific. Adding a board = a new
`[env:...]` in `platformio.ini` + the right `M5GFX_BOARD` value. The app code is
untouched.

This file is just reference notes for whichever boards we actually run on.

## Emulator (PC, no hardware)

- **`emulator_StopWatch`** (default) — LVGL's own SDL driver at the real
  **466×466**. The launcher clips its UI to a circle and fills the corners with
  the "outside" color, so the round shape is simulated. Bypasses M5GFX
  (`src/utility/sdl_lvgl_main.cpp`); app code is unchanged. Design for a **round** screen.

## First dev board (current) — M5Stack StopWatch (C152)

ESP32-S3 round-display dev kit. Source: M5Stack docs
(<https://docs.m5stack.com/en/core/StopWatch>).

- **MCU:** ESP32-S3R8, dual Xtensa LX7 @240MHz, 16MB flash, 8MB PSRAM, 2.4G Wi-Fi.
- **Display:** 1.75" round AMOLED **466×466**, driver **CO5300 over QSPI**
  (G39/G40/G38/G41/G42/G46/G45). Panel **reset is via the M5IOE1 I/O expander**,
  not a plain GPIO — init isn't just `gfx.init()`.
- **Touch:** CST820B on I2C (SDA G47, SCL G48), INT G13.
- **Buttons:** Key A = **G2**, Key B = **G1**, plus a separate power button.
- **IMU:** BMI270 (I2C 0x68).  **RTC:** RX8130CE (0x32).
- **Audio:** ES8311 codec + AW8737A amp (I2S G18/G17/G16/G15/G21).
- **Power:** 450mAh battery, M5PM1 PMIC (I2C), USB-C.
- Shared I2C bus (G47/G48): touch, IMU, RTC, PMIC, IO expander.

**M5GFX / M5Unified support this board** — it's not blocked. Use the `esp32s3box`
PlatformIO board. Suggested `device` env when we do bring-up:

```ini
[env:device]
platform = espressif32@6.12.0
board = esp32s3box
framework = arduino
board_build.partitions = default_16MB.csv
board_upload.flash_size = 16MB
lib_deps =
  ${env.lib_deps}
  m5stack/M5Unified
  m5stack/M5GFX        ; already a base dep
  ; M5PM1, M5IOE1      ; PMIC + IO expander (panel reset)
```

**Bring-up gotchas (for `src/utility/` on device):**
- Init the M5IOE1 expander and release the panel reset before/around `gfx.init()`
  (M5Unified's board init handles this — easiest path is to let M5Unified bring
  the board up, then hand the display to our LVGL port).
- Map Back to **G2** (Key A); the second button (G1) is free for later.
- Buttons + IMU come from M5Unified (`M5.update()`, `M5.BtnA`, `M5.Imu`).

## Map of which buttons/keys Frij uses

- **Back** → Key A (G2). On the emulator this is Backspace (see `src/launcher/input.*`).

## Adding another board

1. Copy the `[env:device]` block in `platformio.ini`, rename it.
2. Set `board = ...` (and `M5GFX_BOARD` if needed) for the new target.
3. Confirm M5GFX supports the panel; adjust `src/utility/` only if needed.
4. Apps and the launcher need no changes.
