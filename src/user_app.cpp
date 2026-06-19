#include <time.h>

#include "lvgl.h"

#include "apps/apps.h"
#include "apps/events/events.h"
#include "apps/home/home.h"
#include "apps/settings/settings.h"
#include "apps/todo/todo.h"
#include "launcher/input.h"
#include "launcher/launcher.h"
#include "launcher/registry.h"
#include "store/store.h"
#include "system/audio.h"
#include "system/battery.h"
#include "system/brightness.h"
#include "system/haptics.h"
#include "system/motion.h"
#include "system/sleep.h"
#include "system/timesync.h"
#include "system/wifi.h"
#include "ui/anim.h"
#include "ui/carousel.h"
#include "ui/components.h"
#include "platform/lvgl_port_m5stack.hpp"

#ifdef FRIJ_NEW_LAUNCHER
// ---- new carousel launcher -------------------------------------------------
// Vertical Home<->Settings<->App layers that slide; Home is a horizontal app-
// glance row, Settings/App are horizontal screen carousels. Built to stay smooth
// on the GPU-less panel (plain slides, no per-frame scale/fade). Selected with
// the FRIJ_NEW_LAUNCHER build flag (env:device_new); plain env:device uses the
// original frij_launcher_start.
static frij_carousel_t s_home;   // horizontal: app glances (Clock/Events/Todo)
static frij_carousel_t s_set;    // horizontal: Settings screens
static lv_obj_t*       s_layer_home;  // persistent full-screen layers that slide
static lv_obj_t*       s_layer_set;   // vertically (Home / Settings)
static lv_obj_t*       s_iso_header;
static lv_obj_t*       s_iso_fade;
static lv_point_t      s_iso_start;
static int             s_iso_axis;    // 0 undecided, 1 horizontal, 2 vertical
static bool            s_v_transition;  // this vertical gesture navigates (vs scrolls)
static int             s_v_sign;      // locked vertical direction: +1 down, -1 up
static uint32_t        s_iso_press_tick;  // press start, for fling detection
static int             s_cur;         // 0 = home, 1 = settings, 2 = app
static bool            s_vanim;       // vertical snap animating
static lv_obj_t*       s_layer_app;   // opened-app layer (parked below home)
static frij_carousel_t s_app;         // the opened app's screens
static int             s_app_for = -1;  // home glance index the app layer was built for
static lv_obj_t*       s_app_header;   // header on the opened-app layer
static bool            s_iso_show_hint = true;  // swipe-up affordance; retires after a few boots

// Header action wiring: show/run the app's per-screen action (e.g. Settings
// Network rescan, Events list refresh) as pages change — the launcher's
// layer_change_cb / header_action behavior.
static void set_change_cb(int index, void* user)
{
    (void)user;
    const frij_app_t* a = frij_registry_settings();
    frij_header_set_action(s_iso_header, (a && a->action_symbol) ? a->action_symbol(index) : NULL);
}
static void set_action_cb(lv_event_t* e)
{
    (void)e;
    frij_header_spin_action(s_iso_header);  // refresh affordance
    const frij_app_t* a = frij_registry_settings();
    if (a && a->on_action) {
        a->on_action(frij_carousel_index(&s_set));
    }
}
static void app_change_cb(int index, void* user)
{
    (void)user;
    const frij_app_t* a = frij_registry_get(s_app_for);
    frij_header_set_action(s_app_header, (a && a->action_symbol) ? a->action_symbol(index) : NULL);
}
static void app_action_cb(lv_event_t* e)
{
    (void)e;
    frij_header_spin_action(s_app_header);  // refresh affordance
    const frij_app_t* a = frij_registry_get(s_app_for);
    if (a && a->on_action) {
        a->on_action(frij_carousel_index(&s_app));
    }
}

static void home_builder(lv_obj_t* page, int index, void* user)
{
    (void)user;
    frij_apply_bg(page);
    const frij_app_t* app = frij_registry_get(index);  // all registered home apps
    if (!app) {
        return;
    }
    frij_glow(page, app->color);
    if (app->build_glance) {
        app->build_glance(page);
    }
    if (s_iso_show_hint && app->build_screen && app->screen_count >= 1) {
        frij_swipe_hint(page);
    }
    frij_page_settle(page);
}

static void set_builder(lv_obj_t* page, int index, void* user)
{
    (void)user;
    frij_apply_bg(page);
    frij_registry_settings()->build_screen(page, index);
    frij_page_under_header(page, frij_header_zone());
    frij_page_settle(page);
}

static lv_obj_t* iso_make_layer(lv_obj_t* root)
{
    lv_obj_t* o = lv_obj_create(root);
    lv_obj_set_size(o, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(o, 0, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(o, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(o, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_x(o, lv_pct(50), LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(o, lv_pct(50), LV_PART_MAIN);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(o, LV_OBJ_FLAG_EVENT_BUBBLE);
    return o;
}

static void anim_y_exec(void* o, int32_t v) { lv_obj_set_y((lv_obj_t*)o, (lv_coord_t)v); }
static void vanim_done(lv_anim_t* a) { (void)a; s_vanim = false; }

// Returning from an app: rebuild the home glance so it shows fresh data (new
// todo pick, latest events) instead of the page that was built on the way in.
static void vanim_done_refresh_home(lv_anim_t* a)
{
    (void)a;
    s_vanim = false;
    frij_carousel_refresh(&s_home);
}

// Slide a layer to `to`; `done` (or NULL) is the completion callback. Runs the
// callback even if the layer is missing, so s_vanim never strands.
static void slide_layer_to(lv_obj_t* o, int to, lv_anim_completed_cb_t done)
{
    if (o == NULL) {
        if (done) {
            done(NULL);
        }
        return;
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_values(&a, lv_obj_get_y(o), to);
    lv_anim_set_duration(&a, 160);
    lv_anim_set_exec_cb(&a, anim_y_exec);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    if (done) {
        lv_anim_set_completed_cb(&a, done);
    }
    lv_anim_start(&a);
}

// `last` = this is the slide that owns the s_vanim flag (clears it on completion).
static void slide_layer(lv_obj_t* o, int to, bool last)
{
    slide_layer_to(o, to, last ? vanim_done : NULL);
}

static const frij_app_t* home_app_at(int idx) { return frij_registry_get(idx); }

static void app_screen_builder(lv_obj_t* page, int index, void* user)
{
    const frij_app_t* app = (const frij_app_t*)user;
    frij_apply_bg(page);
    app->build_screen(page, index);
    frij_page_under_header(page, frij_header_zone());
    frij_page_settle(page);
}

// Build (or reuse) the opened-app layer for the current home glance, parked
// below home. Returns false if that app has no screens to open.
static bool ensure_app_layer(void)
{
    int               idx = frij_carousel_index(&s_home);
    const frij_app_t* app = home_app_at(idx);
    if (!app->build_screen || app->screen_count < 1) {
        return false;
    }
    if (s_layer_app && s_app_for == idx) {
        return true;  // already built for this app
    }
    if (s_layer_app) {
        lv_obj_delete(s_layer_app);
        s_layer_app = NULL;
    }
    int h       = frij_screen_min();
    s_layer_app = iso_make_layer(lv_screen_active());
    frij_carousel_init(&s_app, s_layer_app, app->screen_count, app_screen_builder, (void*)app,
                       app->color);
    frij_carousel_set_change_cb(&s_app, app_change_cb, NULL);
    lv_obj_set_y(s_app.viewport, frij_header_zone());
    lv_obj_set_height(s_app.viewport, h - frij_header_zone());
    frij_header_fade(s_layer_app, frij_header_zone());
    s_app_header = frij_header(s_layer_app, app->name, app->color, app_action_cb);
    lv_obj_set_y(s_layer_app, h);  // parked below home
    s_app_for = idx;
    app_change_cb(frij_carousel_index(&s_app), NULL);  // initial action icon
    return true;
}

static frij_carousel_t* active_inner(void)
{
    return s_cur == 0 ? &s_home : s_cur == 1 ? &s_set : &s_app;
}

static void iso_input(lv_event_t* e)
{
    lv_event_code_t code  = lv_event_get_code(e);
    lv_indev_t*     indev = lv_indev_active();
    if (indev == NULL) {
        return;
    }
    int        h = frij_screen_min();
    lv_point_t p;
    if (code == LV_EVENT_PRESSED) {
        lv_indev_get_point(indev, &s_iso_start);
        s_iso_axis       = 0;
        s_iso_press_tick = lv_tick_get();
    } else if (code == LV_EVENT_PRESSING) {
        if (s_vanim) {
            return;
        }
        lv_indev_get_point(indev, &p);
        int dx = p.x - s_iso_start.x;
        int dy = p.y - s_iso_start.y;
        if (s_iso_axis == 0 && (abs(dx) > 8 || abs(dy) > 8)) {
            s_iso_axis = (abs(dx) >= abs(dy)) ? 1 : 2;
            if (s_iso_axis == 2) {
                // Lock the vertical direction for the whole gesture so reversing
                // past the start can't reveal the opposite layer (which used to
                // strand a layer half-on-screen). Transition only at the content's
                // scroll edge, else let LVGL scroll the list.
                s_v_sign            = (dy >= 0) ? 1 : -1;
                lv_obj_t* content   = active_inner()->cur;
                if (s_cur == 0) {
                    // down -> settings; up -> open the app (if it has screens)
                    s_v_transition = (s_v_sign > 0) ? true : ensure_app_layer();
                } else if (s_cur == 1) {
                    s_v_transition = (s_v_sign < 0) && lv_obj_get_scroll_bottom(content) <= 0;
                } else {
                    s_v_transition = (s_v_sign > 0) && lv_obj_get_scroll_top(content) <= 0;
                }
            }
        }
        if (s_iso_axis == 1) {
            frij_carousel_drag(active_inner(), dx);
        } else if (s_iso_axis == 2 && s_v_transition) {
            // Clamp to the locked direction (reversing past start does nothing)
            // and to one screen height — only home + the one target layer move.
            int d = dy;
            if (s_v_sign > 0 && d < 0) d = 0;
            if (s_v_sign < 0 && d > 0) d = 0;
            if (d > h) d = h;
            if (d < -h) d = -h;
            if (s_cur == 0 && s_v_sign > 0) {  // home -> settings (from above)
                lv_obj_set_y(s_layer_home, d);
                lv_obj_set_y(s_layer_set, -h + d);
            } else if (s_cur == 0) {  // home -> app (from below)
                lv_obj_set_y(s_layer_home, d);
                lv_obj_set_y(s_layer_app, h + d);
            } else if (s_cur == 1) {  // settings -> home
                lv_obj_set_y(s_layer_set, d);
                lv_obj_set_y(s_layer_home, h + d);
            } else {  // app -> home
                lv_obj_set_y(s_layer_app, d);
                lv_obj_set_y(s_layer_home, -h + d);
            }
        }
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        lv_indev_get_point(indev, &p);
        int dy   = p.y - s_iso_start.y;
        int dist = (dy < 0) ? -dy : dy;  // travel in the locked direction
        // A fast, short flick commits too — waiting for the full 30% makes quick
        // swipes feel ignored (the old launcher's snappiness).
        int thr = h * 30 / 100;
        if (dist > 40 && lv_tick_elaps(s_iso_press_tick) < 260) {
            thr = 40;
        }
        if (s_iso_axis == 1) {
            frij_carousel_end(active_inner(), p.x - s_iso_start.x);
        } else if (s_iso_axis == 2 && s_v_transition && !s_vanim) {
            bool commit = dist > thr;
            s_vanim     = true;
            // Only home + the one locked target ever moved, so both always slide
            // to a clean rest position here — nothing is left stranded mid-screen.
            if (s_cur == 0 && s_v_sign > 0) {  // home <-> settings
                if (commit) {
                    frij_haptic(FRIJ_HAPTIC_TAP);
                    slide_layer(s_layer_home, h, false);
                    slide_layer(s_layer_set, 0, true);
                    s_cur = 1;
                } else {
                    slide_layer(s_layer_set, -h, false);
                    slide_layer(s_layer_home, 0, true);
                }
            } else if (s_cur == 0) {  // home <-> app
                if (commit) {
                    frij_haptic(FRIJ_HAPTIC_TAP);
                    slide_layer(s_layer_home, -h, false);
                    slide_layer(s_layer_app, 0, true);
                    s_cur = 2;
                } else {
                    slide_layer(s_layer_app, h, false);
                    slide_layer(s_layer_home, 0, true);
                }
            } else if (s_cur == 1) {  // settings -> home
                if (commit) {
                    frij_haptic(FRIJ_HAPTIC_TAP);
                    slide_layer(s_layer_set, -h, false);
                    slide_layer(s_layer_home, 0, true);
                    s_cur = 0;
                } else {
                    slide_layer(s_layer_set, 0, false);
                    slide_layer(s_layer_home, h, true);
                }
            } else {  // app -> home
                if (commit) {
                    frij_haptic(FRIJ_HAPTIC_TAP);
                    slide_layer(s_layer_app, h, false);
                    slide_layer_to(s_layer_home, 0, vanim_done_refresh_home);  // fresh glance
                    s_cur = 0;
                } else {
                    slide_layer(s_layer_app, 0, false);
                    slide_layer(s_layer_home, -h, true);
                }
            }
        }
        s_iso_axis = 0;
    }
}

// Back button for the iso carousel launcher. Mirrors the real frij_back: close an
// open dialog first, else step a multi-screen app/settings to its first screen,
// else slide the open layer back down to home.
//
// iso_back_impl is the core; the caller MUST already hold the LVGL lock.
static void iso_back_impl(void)
{
    int h = frij_screen_min();
    if (frij_modal_close_top()) {
        // a dialog/sheet was open: Back closed it instead of navigating
    } else if (s_vanim) {
        // mid-transition: ignore
    } else if (s_cur == 2) {  // in an app
        if (frij_carousel_index(&s_app) != 0) {
            frij_carousel_goto(&s_app, 0);  // first to the app's main screen
        } else {
            s_vanim = true;
            slide_layer(s_layer_app, h, false);
            slide_layer_to(s_layer_home, 0, vanim_done_refresh_home);  // fresh glance on return
            s_cur = 0;
        }
    } else if (s_cur == 1) {  // in settings
        if (frij_carousel_index(&s_set) != 0) {
            frij_carousel_goto(&s_set, 0);
        } else {
            s_vanim = true;
            slide_layer(s_layer_set, -h, false);
            slide_layer(s_layer_home, 0, true);
            s_cur = 0;
        }
    } else {  // already home: jump to the default glance (the clock)
        if (frij_carousel_index(&s_home) != 0) {
            frij_carousel_goto(&s_home, 0);
        }
    }
}

// From main.cpp's loop() (the device Key B) — a different task than the LVGL
// render task — so it takes the lock around the work, then runs the core.
void frij_iso_back(void)
{
    if (!lvgl_port_lock()) {
        return;
    }
    iso_back_impl();
    lvgl_port_unlock();
}

// From an LVGL event callback (the header's on-screen Back button) — already on
// the render task with the lock held, so call the core directly. Re-locking here
// would dead-lock the non-recursive LVGL mutex.
void frij_iso_back_from_ui(void)
{
    iso_back_impl();
}

// Re-query the active layer's header action when its availability changes
// without a page change (Settings calls this when Wi-Fi is toggled off, to hide
// the Network rescan icon). Replaces the real launcher's no-op in this build.
// Called from a Settings UI callback — already on the LVGL thread, no lock.
void frij_launcher_refresh_action(void)
{
    if (s_cur == 1) {
        set_change_cb(frij_carousel_index(&s_set), NULL);
    } else if (s_cur == 2) {
        app_change_cb(frij_carousel_index(&s_app), NULL);
    }
}

// Hold-Back lands on the watch face (glance 0) after the layer slides home.
static void vanim_done_home0(lv_anim_t* a)
{
    (void)a;
    s_vanim = false;
    if (frij_carousel_index(&s_home) != 0) {
        frij_carousel_goto(&s_home, 0);
    }
}

// Key B held (device): shed dialogs, slide back to home, and continue all the
// way to glance 0. Called from main.cpp's loop() — locks like frij_iso_back.
void frij_iso_home(void)
{
    if (!lvgl_port_lock()) {
        return;
    }
    while (frij_modal_close_top()) {
    }  // shed every open dialog/sheet first
    if (!s_vanim) {
        int h = frij_screen_min();
        if (s_cur == 2) {
            s_vanim = true;
            slide_layer(s_layer_app, h, false);
            slide_layer_to(s_layer_home, 0, vanim_done_home0);
            s_cur = 0;
        } else if (s_cur == 1) {
            s_vanim = true;
            slide_layer(s_layer_set, -h, false);
            slide_layer_to(s_layer_home, 0, vanim_done_home0);
            s_cur = 0;
        } else if (frij_carousel_index(&s_home) != 0) {
            frij_carousel_goto(&s_home, 0);  // already home: jump to the clock
        }
    }
    lvgl_port_unlock();
}
#endif  // FRIJ_NEW_LAUNCHER

// The cloud-synced keys: pulled at boot and on the periodic auto-sync tick.
static void pull_synced_keys(void)
{
    frij_store_pull_async("todo");
    frij_store_pull_async("events");
    frij_store_pull_async("counter");
    frij_store_pull_async("sb_a");
    frij_store_pull_async("sb_b");
    // Stamp when the todo list was last synced, for its "Updated Xm ago"
    // footer. Done here (the real sync points) not on screen-open, so the
    // footer actually ages instead of always reading "just now".
    frij_store_save_int("todo_synced", (int)time(NULL));
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
    // Seed LVGL's PRNG — it boots with a fixed seed, so anything "random"
    // (e.g. the Todo glance's pick) would repeat the same sequence every boot.
    lv_rand_set_seed((uint32_t)time(NULL));

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

#if !defined(FRIJ_NO_NET) && !defined(FRIJ_NEW_LAUNCHER)
    // bring up Wi-Fi with the saved master-switch state (the device kicks off a
    // background reconnect to the saved network — no boot stall)
    frij_wifi_init();
    frij_wifi_set_enabled(frij_store_load_bool("wifi_on", true));
    frij_time_sync_init();  // SNTP: correct the clock once Wi-Fi is up

    // when auto-sync is on, pull the apps' latest cloud data in the background
    if (frij_store_load_bool("autosync", true)) {
        pull_synced_keys();
    }
#endif  // new launcher does its own Wi-Fi/sync below; FRIJ_NO_NET skips it entirely

    // Battery subjects MUST exist before any screen binds to them (the home
    // glance binds at launcher start) — binding to uninitialized subjects left
    // the readout showing LVGL's default "Text".
    if (lvgl_port_lock()) {
        frij_battery_init();
        // LVGL auto-shows its perf overlay at init; honor the saved debug toggle
        // (default off) so it's hidden unless the user turned it on.
        frij_debug_overlay_set(frij_store_load_bool("debug", false));
        lvgl_port_unlock();
    }

#ifdef FRIJ_NEW_LAUNCHER
    // New carousel launcher (env:device_new). Bring up Wi-Fi + the boot Supabase
    // pulls, then SNTP, then the apps.
    frij_wifi_init();
    frij_wifi_set_enabled(frij_store_load_bool("wifi_on", true));
    frij_time_sync_init();  // SNTP: correct the clock once Wi-Fi is up
    if (frij_store_load_bool("autosync", true)) {
        pull_synced_keys();
    }

    // Swipe-up hint: show it for the first few boots, then retire it (the gesture
    // is learned by then and the bobbing chevron is just noise).
    int hint_seen   = frij_store_load_int("hint_seen", 0);
    s_iso_show_hint = hint_seen < 5;
    if (s_iso_show_hint) {
        frij_store_save_int("hint_seen", hint_seen + 1);
    }

    frij_register_apps();
    const frij_app_t* set_app = frij_registry_settings();

    if (lvgl_port_lock()) {
        lv_obj_t* root = lv_screen_active();
        frij_apply_bg(root);
        lv_obj_add_flag(root, LV_OBJ_FLAG_EVENT_BUBBLE);
        // The layers parked off-screen (settings at -h, app at +h) extend the
        // screen's scroll area — left scrollable, the screen shows a scrollbar AND
        // its native scroll fights the launcher's manual layer drag (jank). Lock it.
        lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(root, LV_SCROLLBAR_MODE_OFF);
        int h = frij_screen_min();

        // Two persistent full-screen layers that slide vertically, each hosting
        // its own horizontal carousel (Home glances / Settings screens).
        s_layer_home = iso_make_layer(root);
        frij_carousel_init(&s_home, s_layer_home, frij_registry_count(), home_builder, NULL,
                           frij_registry_get(0)->color);

        s_layer_set = iso_make_layer(root);
        frij_carousel_init(&s_set, s_layer_set, set_app->screen_count, set_builder, NULL,
                           set_app->color);
        frij_carousel_set_change_cb(&s_set, set_change_cb, NULL);
        lv_obj_set_y(s_set.viewport, frij_header_zone());
        lv_obj_set_height(s_set.viewport, h - frij_header_zone());
        s_iso_fade   = frij_header_fade(s_layer_set, frij_header_zone());
        s_iso_header = frij_header(s_layer_set, set_app->name, set_app->color, set_action_cb);
        set_change_cb(0, NULL);  // initial action icon (General screen)

        lv_obj_set_y(s_layer_home, 0);   // home visible
        lv_obj_set_y(s_layer_set, -h);   // settings parked above, off-screen
        s_cur = 0;

        lv_obj_add_event_cb(root, iso_input, LV_EVENT_ALL, NULL);

        frij_sleep_init();  // idle-sleep manager (0.5s timer)
        // Periodic auto-sync, same as the real launcher: re-pull the cloud keys
        // every 5 min while the setting is on (boot does the first pull above).
        lv_timer_create(autosync_tick, 5 * 60 * 1000, NULL);
        lvgl_port_unlock();
    }
    return;
#endif

    frij_register_apps();
    frij_launcher_start();
    frij_input_init();

    frij_motion_init();  // raise-to-wake (device IMU; no-op on the emulator)

    // idle-sleep manager (turns the panel off after the "Sleep" minutes). Create
    // its timer under the LVGL lock since we're outside the LVGL task here.
    if (lvgl_port_lock()) {
        frij_sleep_init();
#if !defined(FRIJ_NO_NET) && !defined(FRIJ_NEW_LAUNCHER)
        lv_timer_create(autosync_tick, 5 * 60 * 1000, NULL);  // periodic auto-sync
#endif
        lvgl_port_unlock();
    }
}
