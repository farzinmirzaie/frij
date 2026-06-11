#include "settings.h"

#include <string.h>
#include <time.h>

#include "store/store.h"
#include "system/audio.h"
#include "system/battery.h"
#include "system/brightness.h"
#include "system/haptics.h"
#include "system/wifi.h"
#include "core/datetime.h"
#include "ui/anim.h"
#include "ui/components.h"
#include "ui/theme.h"

static void build_about(lv_obj_t* col);  // forward (used by Sync now's refresh)
static lv_obj_t* s_about_col = NULL;     // the About page, for in-place refresh

/*
 * Settings — a normal app (glance unused; reached by swiping down). Screens:
 *   0 General : Display (brightness/sleep/raise-to-wake), Sound (volume/touch
 *               sounds), Preferences (24-hour, vibration, auto-sync)
 *   1 Network : Wi-Fi toggle + scanned list (connect/disconnect/forget)
 *   2 About   : name/version, battery, last sync, Sync now, Reset, Erase all
 *
 * Values persist via the shared store under their own keys (see store.h's
 * typed accessors). Destructive actions go through a confirm dialog.
 */

static const uint32_t ACCENT = FRIJ_PRIMARY;  // purple

// ---- handlers --------------------------------------------------------------

static void on_brightness(lv_event_t* e)
{
    lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
    int       v      = lv_slider_get_value(slider);
    frij_store_save_int("brightness", v);
    frij_set_brightness((uint8_t)v);
}

static void on_clock24(lv_event_t* e)
{
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    frij_store_save_bool("clock24", lv_obj_has_state(sw, LV_STATE_CHECKED));
}

static void on_volume(lv_event_t* e)
{
    lv_obj_t* sl = (lv_obj_t*)lv_event_get_target(e);
    int       v  = lv_slider_get_value(sl);
    frij_store_save_int("volume", v);
    frij_set_volume((uint8_t)v);  // ES8311 codec (via M5.Speaker on device)
}

static void on_vibration(lv_event_t* e)
{
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    bool      on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    frij_haptics_set_enabled(on);
    frij_store_save_bool("haptics", on);
}

static void on_sleep(lv_event_t* e)
{
    lv_obj_t* sl = (lv_obj_t*)lv_event_get_target(e);
    frij_store_save_int("sleep", lv_slider_get_value(sl));  // minutes until display sleeps
    // The idle sleep manager (system/sleep) reads this each tick — no apply needed.
}

static void on_autosync(lv_event_t* e)
{
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    frij_store_save_bool("autosync", lv_obj_has_state(sw, LV_STATE_CHECKED));
}

static void on_animations(lv_event_t* e)
{
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    bool      on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    frij_anim_set_enabled(on);  // reduce-motion: takes effect on the next screen
    frij_store_save_bool("anim", on);
}

static void on_raise_wake(lv_event_t* e)
{
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    frij_store_save_bool("raisewake", lv_obj_has_state(sw, LV_STATE_CHECKED));
    // system/motion polls the BMI270 each loop and wakes on a raise when this is on.
}

static void on_touch_sfx(lv_event_t* e)
{
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    bool      on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    frij_audio_set_click_enabled(on);  // press click on the ES8311 codec
    frij_store_save_bool("touchsfx", on);
}

static void on_sync_now(lv_event_t* e)
{
    (void)e;
    frij_store_pull_async("todo");  // best-effort cloud refresh
    frij_store_pull_async("counter");
    frij_store_pull_async("sb_a");
    frij_store_pull_async("sb_b");
    frij_store_save_int("last_sync", (int)time(NULL));
    frij_haptic(FRIJ_HAPTIC_SUCCESS);
    if (s_about_col) {  // reflect the new "Last sync" immediately
        lv_obj_clean(s_about_col);
        build_about(s_about_col);
        frij_page_settle(s_about_col);
    }
    frij_toast("Syncing...");
}

static void do_reset(lv_event_t* e)
{
    (void)e;
    frij_store_save_int("brightness", 80);
    frij_store_save_int("volume", 60);
    frij_store_save_int("sleep", 5);
    frij_store_save_bool("clock24", true);
    frij_store_save_bool("haptics", true);
    frij_store_save_bool("autosync", true);
    frij_store_save_bool("anim", true);
    frij_set_brightness(80);
    frij_haptics_set_enabled(true);
    frij_anim_set_enabled(true);
    frij_haptic(FRIJ_HAPTIC_SUCCESS);
    frij_toast_status("Settings reset", true);
}

static void on_reset(lv_event_t* e)
{
    (void)e;  // destructive: confirm with a danger-colored button
    frij_confirm("Reset settings?", "Restore everything to defaults.", "Reset", FRIJ_DANGER, do_reset);
}

static void do_erase(lv_event_t* e)
{
    (void)e;
    frij_store_clear();  // wipe todos / counter / settings; defaults return on next read
    frij_set_brightness(80);
    frij_haptics_set_enabled(true);
    frij_anim_set_enabled(true);
    frij_haptic(FRIJ_HAPTIC_SUCCESS);
    frij_toast_status("All data erased", true);
}

static void on_erase(lv_event_t* e)
{
    (void)e;  // destructive: confirm with a danger-colored button
    frij_confirm("Erase all data?", "Removes todos, counter and settings.", "Erase", FRIJ_DANGER, do_erase);
}

// ---- row helpers -----------------------------------------------------------

// The whole row toggles (not just the switch), so tapping anywhere flips it.
static void on_toggle_row_click(lv_event_t* e)
{
    lv_obj_t* row = (lv_obj_t*)lv_event_get_current_target(e);
    lv_obj_t* tog = lv_obj_get_child(row, lv_obj_get_child_count(row) - 1);
    bool      on  = !lv_obj_has_state(tog, LV_STATE_CHECKED);
    if (on) {
        lv_obj_add_state(tog, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(tog, LV_STATE_CHECKED);
    }
    lv_obj_send_event(tog, LV_EVENT_VALUE_CHANGED, NULL);  // run the persist handler
}

static void toggle_row(lv_obj_t* col, const char* text, const char* key, bool def, lv_event_cb_t cb)
{
    lv_obj_t* row = frij_surface_row(col);
    lv_obj_t* lbl = frij_label(row, text, FRIJ_FONT_BODY, FRIJ_TEXT);
    lv_obj_set_flex_grow(lbl, 1);
    lv_obj_t* tog = frij_toggle(row, frij_store_load_bool(key, def), ACCENT);
    lv_obj_remove_flag(tog, LV_OBJ_FLAG_CLICKABLE);  // the row drives it
    lv_obj_add_event_cb(tog, cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(row, on_toggle_row_click, LV_EVENT_CLICKED, NULL);
}

static void slider_row(lv_obj_t* col, const char* text, int min, int max, const char* key,
                       int def, lv_event_cb_t cb, const char* unit)
{
    lv_obj_t* s = frij_slider_row(col, text, min, max, frij_store_load_int(key, def), ACCENT, unit);
    lv_obj_add_event_cb(s, cb, LV_EVENT_VALUE_CHANGED, NULL);
}

// ---- Network screen (Wi-Fi) ------------------------------------------------

static lv_obj_t*      s_net_col = NULL;            // the Network page, for in-place refresh
static frij_wifi_net_t s_scan[12];                 // last scan (rows index into this)
static int            s_scan_n   = 0;
static char           s_sel[FRIJ_WIFI_SSID_MAX];   // the network the action sheet acts on
static int            s_sel_kind = 0;              // 0 new, 1 saved, 2 connected

static void build_network(lv_obj_t* col);

// Clear the cached Network page pointer when its page is destroyed.
static void on_net_deleted(lv_event_t* e)
{
    if (s_net_col == lv_event_get_target(e)) {
        s_net_col = NULL;
    }
}

static void net_refresh(void)
{
    if (!s_net_col) {
        return;
    }
    lv_obj_clean(s_net_col);  // keeps the page's styles/padding, drops the rows
    build_network(s_net_col);
    frij_page_settle(s_net_col);
}

static void net_action_cb(int opt, void* user)
{
    (void)user;
    char msg[64];
    bool ok     = true;
    bool forget = (s_sel_kind != 0 && opt == 1);  // option 1 is Forget when present
    if (forget) {
        frij_wifi_forget(s_sel);
        lv_snprintf(msg, sizeof(msg), "Forgot %s", s_sel);
    } else if (s_sel_kind == 2) {  // connected, Disconnect
        frij_wifi_disconnect();
        lv_snprintf(msg, sizeof(msg), "Disconnected");
    } else {  // Connect with saved/no credentials
        ok = frij_wifi_connect(s_sel, NULL);
        lv_snprintf(msg, sizeof(msg), ok ? "Connected to %s" : "Couldn't connect", s_sel);
    }
    frij_haptic(FRIJ_HAPTIC_SELECT);
    net_refresh();
    frij_toast_status(msg, ok);
}

// Keyboard prompt callback: join the selected network with the typed password.
static void wifi_pw_done(const char* pw, void* user)
{
    (void)user;
    bool ok = frij_wifi_connect(s_sel, pw);
    frij_haptic(FRIJ_HAPTIC_SELECT);
    net_refresh();
    char msg[64];
    lv_snprintf(msg, sizeof(msg), ok ? "Connected to %s" : "Wrong password?", s_sel);
    frij_toast_status(msg, ok);
}

static void net_row_cb(lv_event_t* e)
{
    lv_obj_t* row = (lv_obj_t*)lv_event_get_current_target(e);
    int       i   = (int)(intptr_t)lv_obj_get_user_data(row);
    if (i < 0 || i >= s_scan_n) {
        return;
    }
    frij_wifi_net_t* nw = &s_scan[i];
    strncpy(s_sel, nw->ssid, sizeof(s_sel) - 1);
    s_sel[sizeof(s_sel) - 1] = '\0';

    static const char* opt_connected[] = {"Disconnect", "Forget"};
    static const char* opt_saved[]     = {"Connect", "Forget"};
    if (nw->connected) {
        s_sel_kind = 2;
        frij_action_sheet(nw->ssid, opt_connected, 2, ACCENT, net_action_cb, NULL);
    } else if (nw->known) {  // saved creds — connect without re-asking
        s_sel_kind = 1;
        frij_action_sheet(nw->ssid, opt_saved, 2, ACCENT, net_action_cb, NULL);
    } else if (nw->secured) {  // new + secured — ask for the password (numeric keypad)
        s_sel_kind = 0;
        frij_numpad_prompt(nw->ssid, wifi_pw_done, NULL);
    } else {  // new + open — just join
        s_sel_kind = 0;
        wifi_pw_done(NULL, NULL);
    }
}

static void wifi_master_cb(lv_event_t* e)
{
    (void)e;
    frij_wifi_set_enabled(!frij_wifi_enabled());
    net_refresh();
}

static void build_network(lv_obj_t* col)
{
    // Master Wi-Fi switch (whole row toggles it).
    bool      on  = frij_wifi_enabled();
    lv_obj_t* row = frij_surface_row(col);
    lv_obj_t* lbl = frij_label(row, "Wi-Fi", FRIJ_FONT_BODY, FRIJ_TEXT);
    lv_obj_set_flex_grow(lbl, 1);
    lv_obj_t* tog = frij_toggle(row, on, ACCENT);
    lv_obj_remove_flag(tog, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, wifi_master_cb, LV_EVENT_CLICKED, NULL);
    if (!on) {
        // radio off: toggle stays pinned at the top; float the hint at the
        // screen's center (FLOATING so it ignores the top-pinned flex flow).
        lv_obj_t* hint = frij_label(col, "Turn on Wi-Fi to see\nnearby networks",
                                    FRIJ_FONT_BODY, FRIJ_TEXT_2);
        lv_obj_set_width(hint, LV_PCT(100));
        lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_add_flag(hint, LV_OBJ_FLAG_FLOATING);
        lv_obj_align(hint, LV_ALIGN_CENTER, 0, 60);  // centered in the page area
        frij_stagger_in(col, 40);
        return;
    }

    // Visible networks; tap one for Connect / Disconnect / Forget.
    s_scan_n = frij_wifi_scan(s_scan, (int)(sizeof(s_scan) / sizeof(s_scan[0])));
    if (s_scan_n == 0) {
        frij_empty_state(col, "No networks");
        frij_stagger_in(col, 40);
        return;
    }
    for (int i = 0; i < s_scan_n; i++) {
        frij_wifi_net_t* nw  = &s_scan[i];
        lv_obj_t*        r   = frij_surface_row(col);
        lv_obj_set_user_data(r, (void*)(intptr_t)i);
        lv_obj_add_event_cb(r, net_row_cb, LV_EVENT_CLICKED, NULL);

        // signal glyph, brighter the stronger the network
        lv_obj_t* sig = frij_label(r, LV_SYMBOL_WIFI, FRIJ_FONT_SYMBOL,
                                   nw->rssi > -60 ? FRIJ_TEXT : FRIJ_TEXT_2);
        (void)sig;
        lv_obj_t* name = frij_label(r, nw->ssid, FRIJ_FONT_BODY,
                                    nw->connected ? ACCENT : FRIJ_TEXT);
        lv_obj_set_flex_grow(name, 1);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);

        const char* status = nw->connected ? "Connected" : (nw->known ? "Saved" : "");
        if (status[0]) {
            frij_label(r, status, FRIJ_FONT_BODY, nw->connected ? ACCENT : FRIJ_TEXT_2);
        }
    }
    frij_stagger_in(col, 35);  // staggered list entrance
}

// ---- About screen ----------------------------------------------------------

// Clear the cached About page pointer when its page is destroyed.
static void on_about_deleted(lv_event_t* e)
{
    if (s_about_col == lv_event_get_target(e)) {
        s_about_col = NULL;
    }
}

// Observer: keep the About "Battery" value live (bound to both battery subjects).
static void about_battery_cb(lv_observer_t* obs, lv_subject_t* subject)
{
    (void)subject;
    lv_obj_t* val      = (lv_obj_t*)lv_observer_get_target(obs);
    uint8_t   pct      = frij_battery_pct();
    bool      charging = frij_battery_charging();
    lv_label_set_text_fmt(val, "%d%%%s", pct, charging ? "  " LV_SYMBOL_CHARGE : "");
    lv_obj_set_style_text_color(
        val, lv_color_hex((pct <= 15 && !charging) ? FRIJ_WARNING : FRIJ_TEXT_2), LV_PART_MAIN);
}

static void build_about(lv_obj_t* col)
{
    // hero: name + version, with generous breathing room around it
    lv_obj_t* hero = frij_label(col, "Frij", FRIJ_FONT_TITLE, FRIJ_TEXT);
    lv_obj_set_style_margin_top(hero, FRIJ_SP_XXL * 4, LV_PART_MAIN);  // 36
    lv_obj_t* ver = frij_label(col, "on-device UI  -  v0.1", FRIJ_FONT_BODY, FRIJ_TEXT_2);
    lv_obj_set_style_margin_bottom(ver, FRIJ_SP_XXL * 4, LV_PART_MAIN);  // 36

    // Battery row — bound to the battery subjects so it tracks live (observer),
    // instead of sampling once when About is built.
    lv_obj_t* brow = frij_value_row(col, "Battery", "");
    lv_obj_t* bval = lv_obj_get_child(brow, 1);
    lv_subject_add_observer_obj(frij_battery_level_subject(), about_battery_cb, bval, NULL);
    lv_subject_add_observer_obj(frij_battery_charging_subject(), about_battery_cb, bval, NULL);

    char sbuf[24];
    int  ls = frij_store_load_int("last_sync", 0);
    if (ls > 0) {
        frij_format_relative(sbuf, sizeof(sbuf), (time_t)ls);  // "Just now" / "5m ago"
    } else {
        lv_snprintf(sbuf, sizeof(sbuf), "Never");
    }
    frij_value_row(col, "Last sync", sbuf);

    frij_action_row(col, "Sync now", on_sync_now);
    frij_action_row(col, "Reset settings", on_reset);
    frij_action_row(col, "Erase all data", on_erase);
    frij_stagger_in(col, 30);
}

// ---- screens ---------------------------------------------------------------

static void screen(lv_obj_t* parent, int index)
{
    lv_obj_t* col = frij_page(parent);

    switch (index) {
        case 0:  // General
            frij_section_label(col, "Display");
            slider_row(col, "Brightness", 10, 100, "brightness", 80, on_brightness, "%");
            slider_row(col, "Sleep", 1, 30, "sleep", 5, on_sleep, " min");
            toggle_row(col, "Raise to wake", "raisewake", true, on_raise_wake);  // BMI270 IMU
            frij_section_label(col, "Sound");
            slider_row(col, "Volume", 0, 100, "volume", 60, on_volume, "%");
            toggle_row(col, "Touch sounds", "touchsfx", false, on_touch_sfx);  // ES8311 codec
            frij_section_label(col, "Preferences");
            toggle_row(col, "24-hour time", "clock24", true, on_clock24);
            toggle_row(col, "Vibration", "haptics", true, on_vibration);
            toggle_row(col, "Animations", "anim", true, on_animations);
            toggle_row(col, "Auto-sync", "autosync", true, on_autosync);
            frij_stagger_in(col, 30);
            break;

        case 1:  // Network
            s_net_col = col;
            lv_obj_add_event_cb(col, on_net_deleted, LV_EVENT_DELETE, NULL);
            frij_page_pin_top(col);  // keep the Wi-Fi toggle at the top, on or off
            build_network(col);
            break;

        default:  // System / About
            s_about_col = col;
            lv_obj_add_event_cb(col, on_about_deleted, LV_EVENT_DELETE, NULL);
            build_about(col);
            break;
    }
}

const frij_app_t* settings_app(void)
{
    static const frij_app_t app = {"Settings", ACCENT, NULL, 3, screen};
    return &app;
}
