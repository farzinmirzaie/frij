#include "apps/apps.h"
#include "launcher/input.h"
#include "launcher/launcher.h"
#include "store/store.h"
#include "system/audio.h"
#include "system/battery.h"
#include "system/brightness.h"
#include "system/haptics.h"
#include "system/motion.h"
#include "system/sleep.h"
#include "system/wifi.h"
#include "ui/anim.h"
#include "utility/lvgl_port_m5stack.hpp"

// The cloud-synced keys: pulled at boot and on the periodic auto-sync tick.
static void pull_synced_keys(void)
{
    frij_store_pull_async("todo");
    frij_store_pull_async("events");
    frij_store_pull_async("counter");
    frij_store_pull_async("sb_a");
    frij_store_pull_async("sb_b");
}

// Auto-sync: periodically pull the cloud-synced keys while the setting is on
// (a boot-only pull made the toggle nearly meaningless).
static void autosync_tick(lv_timer_t* t)
{
    (void)t;
    if (!frij_store_load_bool("autosync", true)) {
        return;
    }
    pull_synced_keys();
}

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

    // bring up Wi-Fi with the saved master-switch state (the device kicks off a
    // background reconnect to the saved network — no boot stall)
    frij_wifi_init();
    frij_wifi_set_enabled(frij_store_load_bool("wifi_on", true));

    // when auto-sync is on, pull the apps' latest cloud data in the background
    if (frij_store_load_bool("autosync", true)) {
        pull_synced_keys();
    }

    // Battery subjects MUST exist before any screen binds to them (the home
    // glance binds at launcher start) — binding to uninitialized subjects left
    // the readout showing LVGL's default "Text".
    if (lvgl_port_lock()) {
        frij_battery_init();
        lvgl_port_unlock();
    }

    frij_register_apps();
    frij_launcher_start();
    frij_input_init();

    frij_motion_init();  // raise-to-wake (device IMU; no-op on the emulator)

    // idle-sleep manager (turns the panel off after the "Sleep" minutes). Create
    // its timer under the LVGL lock since we're outside the LVGL task here.
    if (lvgl_port_lock()) {
        frij_sleep_init();
        lv_timer_create(autosync_tick, 5 * 60 * 1000, NULL);  // periodic auto-sync
        lvgl_port_unlock();
    }
}
