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
    frij_set_brightness((uint8_t)frij_store_load_int("brightness", 80));

    // apply the saved vibration preference (default on)
    frij_haptics_set_enabled(frij_store_load_bool("haptics", true));

    // when auto-sync is on, pull the apps' latest cloud data in the background
    if (frij_store_load_bool("autosync", true)) {
        frij_store_pull_async("todo");
        frij_store_pull_async("counter");
    }

    frij_register_apps();
    frij_launcher_start();
    frij_input_init();
}
