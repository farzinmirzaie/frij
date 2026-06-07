#include "apps/apps.h"
#include "launcher/input.h"
#include "launcher/launcher.h"

/*
 * Frij entry point — called once at startup after display + LVGL are ready.
 *
 *   1. register all mini-apps
 *   2. show the launcher (glance carousel)
 *   3. start the input layer (Back button / key)
 *
 * To add an app, edit src/apps/apps.cpp — not this file.
 */
void user_app(void)
{
    frij_register_apps();
    frij_launcher_start();
    frij_input_init();
}
