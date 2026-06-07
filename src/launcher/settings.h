#ifndef FRIJ_SETTINGS_H
#define FRIJ_SETTINGS_H

#include "lvgl.h"

/*
 * Device settings — a carousel of screens, like an app. Add screens here over
 * time (brightness, wifi, …). With one screen it won't loop; with more it does.
 */
int  frij_settings_screen_count(void);
void frij_settings_build_screen(lv_obj_t* parent, int index);

#endif  // FRIJ_SETTINGS_H
