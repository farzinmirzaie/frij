#include "apps/apps.h"
#include "launcher/input.h"
#include "launcher/launcher.h"
#include "store/store.h"

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
    frij_register_apps();
    frij_launcher_start();
    frij_input_init();
}
