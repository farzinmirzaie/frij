#ifndef FRIJ_BRIGHTNESS_H
#define FRIJ_BRIGHTNESS_H

#include <stdint.h>

// Set display brightness, 0–100%. Board-specific (implemented in the board
// layer); a no-op where there's no real backlight (the emulator). Apps call
// this neutral function — they don't touch the port directly.
void frij_set_brightness(uint8_t pct);

#endif  // FRIJ_BRIGHTNESS_H
