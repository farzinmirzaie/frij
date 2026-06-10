#include "apps/apps.h"
#include "launcher/input.h"
#include "launcher/launcher.h"
#include "store/store.h"
#include "system/audio.h"
#include "system/brightness.h"
#include "system/haptics.h"
#include "system/motion.h"
#include "system/sleep.h"
#include "ui/anim.h"
#include "utility/lvgl_port_m5stack.hpp"

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

    // apply the saved reduce-motion preference (default: animations on)
    frij_anim_set_enabled(frij_store_load_bool("anim", true));

    // apply the saved volume + touch-sound preferences
    frij_set_volume((uint8_t)frij_store_load_int("volume", 60));
    frij_audio_set_click_enabled(frij_store_load_bool("touchsfx", false));

    // when auto-sync is on, pull the apps' latest cloud data in the background
    if (frij_store_load_bool("autosync", true)) {
        frij_store_pull_async("todo");
        frij_store_pull_async("counter");
        frij_store_pull_async("sb_a");
        frij_store_pull_async("sb_b");
    }

    frij_register_apps();
    frij_launcher_start();
    frij_input_init();

    frij_motion_init();  // raise-to-wake (device IMU; no-op on the emulator)

    // idle-sleep manager (turns the panel off after the "Sleep" minutes). Create
    // its timer under the LVGL lock since we're outside the LVGL task here.
    if (lvgl_port_lock()) {
        frij_sleep_init();
        lvgl_port_unlock();
    }
}
