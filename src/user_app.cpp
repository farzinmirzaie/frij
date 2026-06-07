#include "apps/apps.h"
#include "launcher/launcher.h"

/*
 * Frij entry point — called once at startup after display + LVGL are ready.
 *
 * Two steps:
 *   1. register all mini-apps with the launcher
 *   2. show the launcher (home screen)
 *
 * To add an app, edit src/apps/apps.cpp — not this file.
 */
void user_app(void)
{
    frij_register_apps();
    frij_launcher_start();
}
