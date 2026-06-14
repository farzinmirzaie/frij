#include "haptics.h"

// Shared on/off state (set from Settings, applied at boot).
static bool s_enabled = true;

void frij_haptics_set_enabled(bool on)
{
    s_enabled = on;
}

bool frij_haptics_enabled(void)
{
    return s_enabled;
}

#if defined(__has_include) && (__has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>))

// Emulator: no motor.
void frij_haptic(frij_haptic_t kind)
{
    (void)kind;
}

#else

#include <M5Unified.h>

// One motor pulse: `level` for `ms`, then off. Short blocking is fine — haptics
// fire on discrete taps. M5.Power.setVibration is a no-op on boards without a
// motor, so this stays safe.
static void buzz(uint8_t level, uint32_t ms)
{
    M5.Power.setVibration(level);
    delay(ms);
    M5.Power.setVibration(0);
}

void frij_haptic(frij_haptic_t kind)
{
    if (!s_enabled) {
        return;
    }
    switch (kind) {
        case FRIJ_HAPTIC_TAP:
            buzz(170, 12);
            break;
        case FRIJ_HAPTIC_SELECT:
            buzz(220, 20);
            break;
        case FRIJ_HAPTIC_SUCCESS:  // double tick
            buzz(220, 14);
            delay(40);
            buzz(220, 14);
            break;
    }
}

#endif
