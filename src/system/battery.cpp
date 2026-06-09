#include "battery.h"

/*
 * Board-specific, like brightness/haptics: one file, both targets behind a
 * guard. The emulator reports a fixed, plausible charge; the device will read
 * the M5PM1 PMIC.
 */
#if defined(__has_include) && (__has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>))

// Emulator: no real cell — report a steady, believable level.
void    frij_battery_init(void) {}
uint8_t frij_battery_pct(void) { return 76; }
bool    frij_battery_charging(void) { return false; }

#else

// Device: TODO — read the M5PM1 PMIC (charge gauge + charging status).
void    frij_battery_init(void) {}
uint8_t frij_battery_pct(void) { return 100; }
bool    frij_battery_charging(void) { return false; }

#endif
