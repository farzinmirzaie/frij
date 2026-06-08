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
