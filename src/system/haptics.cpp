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

void frij_haptic(frij_haptic_t kind)
{
    if (!s_enabled) {
        return;
    }
    // TODO(device): drive the StopWatch vibration motor. Pulse length by kind:
    //   TAP ~12ms, SELECT ~20ms, SUCCESS ~ two short pulses.
    (void)kind;
}

#endif
