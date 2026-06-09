# system/

Neutral interfaces for **board services** — things apps need but that are
hardware-specific. The header is target-agnostic; the `.cpp` carries the
emulator + device implementations behind a guard (so it compiles for both,
unlike the device-only port in `utility/`).

Apps call these instead of touching `utility/`, keeping app isolation.

## Now

- `brightness.*` — `frij_set_brightness(pct)`. No-op on the emulator; sets the
  panel backlight on device.
- `haptics.*` — `frij_haptic(kind)` + an enable flag. UI components call it on
  interactions (`frij_haptic_attach`); Settings → General → Vibration toggles it
  (persisted). No-op on the emulator; pulses the motor on device (TODO: wire the
  StopWatch motor).
- `wifi.*` — scan / connect / disconnect / forget + a master enable, behind a
  neutral interface. The emulator ships a working **in-memory mock** (a fake
  neighbourhood with connected/saved state) so Settings → Network is fully
  interactive; the device backend is a stub (TODO: `esp_wifi` + NVS creds).
- `battery.*` — `frij_battery_pct()` + `frij_battery_charging()`. A steady mock
  on the emulator (for the watch-face indicator + About); the device reads the
  M5PM1 PMIC (TODO).
