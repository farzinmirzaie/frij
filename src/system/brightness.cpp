#include "brightness.h"

/*
 * Brightness is board-specific. This file is compiled for both targets (unlike
 * the device-only port), so it carries both implementations behind a guard.
 */
#if defined(__has_include) && (__has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>))

// Emulator: no physical backlight.
void frij_set_brightness(uint8_t pct)
{
    (void)pct;
}

#else

#include <M5GFX.h>
extern M5GFX gfx;  // defined in src/main.cpp

void frij_set_brightness(uint8_t pct)
{
    if (pct > 100) {
        pct = 100;
    }
    gfx.setBrightness((uint8_t)(pct * 255 / 100));
}

#endif
