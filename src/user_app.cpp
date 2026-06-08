#include <stdlib.h>  // atoi

#include "apps/apps.h"
#include "launcher/input.h"
#include "launcher/launcher.h"
#include "store/store.h"
#include "system/brightness.h"
#include "system/haptics.h"

/*
 * Frij entry point — called once at startup after display + LVGL are ready.
 *
 *   1. init the shared data store
 *   2. register all mini-apps
 *   3. show the launcher (glance carousel)
 *   4. start the input layer (Back button / key)
 *
 * To add an app, edit src/apps/apps.cpp — not this file.
 */
void user_app(void)
{
    frij_store_init();

    // apply the saved brightness (default 80%)
    char b[16];
    frij_set_brightness(frij_store_load("brightness", b, sizeof(b)) ? (uint8_t)atoi(b) : 80);

    // apply the saved vibration preference (default on)
    char h[8];
    frij_haptics_set_enabled(frij_store_load("haptics", h, sizeof(h)) ? (h[0] == '1') : true);

    frij_register_apps();
    frij_launcher_start();
    frij_input_init();
}
