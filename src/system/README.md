# system/

Neutral interfaces for **board services** — things apps need but that are
hardware-specific. The header is target-agnostic; the `.cpp` carries the
emulator + device implementations behind a guard (so it compiles for both,
unlike the device-only port in `utility/`).

Apps call these instead of touching `utility/`, keeping app isolation.

## Now

- `brightness.*` — `frij_set_brightness(pct)`. Black-scrim dim on the emulator;
  `M5.Display.setBrightness()` on device.
- `display.*` — `frij_display_set_on(bool)` (panel sleep). Black overlay on the
  emulator; `M5.Display.sleep()`/`wakeup()` + brightness restore on device.
- `sleep.*` — `frij_sleep_init()`: an idle timer (reads the "Sleep" minutes) that
  turns the display off and wakes on the next touch. Cross-platform.
- `motion.*` — `frij_motion_init()/update()`: raise-to-wake via the BMI270 on
  device (polls `M5.Imu` each loop, signals activity on a wrist raise); no-op on
  the emulator. Threshold tunable on hardware.
- `audio.*` — `frij_set_volume(pct)` + `frij_audio_click()`/enable. ES8311 codec
  via `M5.Speaker` on device; no-op on the emulator.
- `haptics.*` — `frij_haptic(kind)` + an enable flag. UI components call it on
  interactions (`frij_haptic_attach`); Settings → General → Vibration toggles it
  (persisted). No-op on the emulator; pulses the motor via `M5.Power.setVibration`
  on device.
- `battery.*` — `frij_battery_pct()` + `frij_battery_charging()`. A steady mock
  on the emulator (for the watch-face indicator + About); the device reads the
  M5PM1 PMIC (TODO).
- `wifi.*` — scan / connect / disconnect / forget + a master enable. Emulator =
  in-memory mock; device = Arduino `WiFi` scan/connect with one saved network's
  creds in NVS (`Preferences`), auto-reconnected on enable. New secured networks
  prompt for a password via `frij_keyboard_prompt` (Settings → Network).
