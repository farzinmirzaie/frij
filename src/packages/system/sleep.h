#ifndef FRIJ_SLEEP_H
#define FRIJ_SLEEP_H

#include <stdbool.h>

// Start the idle-sleep manager: a timer that turns the display off after the
// configured idle period ("sleep" minutes in the store) and back on at the next
// touch (or a raise, via system/motion). Cross-platform — works on the emulator
// (black overlay) and the device (panel sleep). Call once at startup.
void frij_sleep_init(void);

// Block (or release) idle-sleep — e.g. while the stopwatch is running the
// screen must stay on. Counts as activity, so the dim warning resets too.
void frij_sleep_inhibit(bool on);

// True while the panel is asleep (the black shade is up). The button handler
// uses this to wake-only: a press while asleep wakes the screen instead of
// firing its action.
bool frij_sleep_active(void);

#endif  // FRIJ_SLEEP_H
