#ifndef FRIJ_HAPTICS_H
#define FRIJ_HAPTICS_H

#include <stdbool.h>

/*
 * Haptic feedback — a board service (vibration motor on device, no-op on the
 * emulator). UI components call frij_haptic() on interactions; the user can
 * turn it off in Settings.
 */
typedef enum {
    FRIJ_HAPTIC_TAP,      // light: presses, slider grab
    FRIJ_HAPTIC_SELECT,   // medium: toggles
    FRIJ_HAPTIC_SUCCESS,  // double: completing something (e.g. checking a todo)
} frij_haptic_t;

void frij_haptic(frij_haptic_t kind);  // pulse, if enabled
void frij_haptics_set_enabled(bool on);
bool frij_haptics_enabled(void);

#endif  // FRIJ_HAPTICS_H
