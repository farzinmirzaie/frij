#ifndef FRIJ_DISPLAY_H
#define FRIJ_DISPLAY_H

#include <stdbool.h>

// Turn the panel on/off (display sleep). Board-specific: on device this sleeps
// the AMOLED panel and drops the backlight to 0; on the emulator it drops a
// black overlay. Waking restores the saved brightness. Apps don't call this
// directly — the idle sleep manager (system/sleep) does.
void frij_display_set_on(bool on);

#endif  // FRIJ_DISPLAY_H
