# system/

Neutral interfaces for **board services** — things apps need but that are
hardware-specific. The header is target-agnostic; the `.cpp` carries the
emulator + device implementations behind a guard (so it compiles for both,
unlike the device-only port in `utility/`).

Apps call these instead of touching `utility/`, keeping app isolation.

## Now

- `brightness.*` — `frij_set_brightness(pct)`. No-op on the emulator; sets the
  panel backlight on device.
