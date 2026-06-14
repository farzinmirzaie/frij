#include "display.h"

/*
 * Display power (panel sleep). Board-specific, compiled for both targets behind
 * the same SDL/device guard as brightness.cpp.
 */
#if defined(__has_include) && (__has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>))

// Emulator: no real panel. The visual blackout + tap shield is the sleep
// manager's shade (system/sleep), which covers both targets — nothing to do.
void frij_display_set_on(bool on)
{
    (void)on;
}

#else

#include <M5Unified.h>

#include "brightness.h"
#include "store/store.h"

void frij_display_set_on(bool on)
{
    if (on) {
        M5.Display.wakeup();
        frij_set_brightness((uint8_t)frij_store_load_int("brightness", 80));  // restore
    } else {
        M5.Display.setBrightness(0);
        M5.Display.sleep();
    }
}

#endif
