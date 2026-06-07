#ifndef FRIJ_SETTINGS_H
#define FRIJ_SETTINGS_H

#include "app.h"

// Device settings. It's an ordinary app; the launcher registers it in the
// "settings" slot (reached by swiping down) rather than the home carousel,
// so its glance is unused. Add screens over time (brightness, wifi, …).
const frij_app_t* settings_app(void);

#endif  // FRIJ_SETTINGS_H
