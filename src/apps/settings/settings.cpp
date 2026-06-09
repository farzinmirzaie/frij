#include "settings.h"

#include <string.h>
#include <time.h>

#include "store/store.h"
#include "system/battery.h"
#include "system/brightness.h"
#include "system/haptics.h"
#include "system/wifi.h"
#include "core/datetime.h"
#include "ui/anim.h"
#include "ui/components.h"
#include "ui/theme.h"

/*
 * Settings — a normal app (glance unused; reached by swiping down). Screens:
 *   0 General : brightness / volume / sleep sliders + 24-hour, vibration and
 *               auto-sync toggles
 *   1 Network : Wi-Fi status + "Sync now"
 *   2 About   : name / version + "Reset settings" (behind a confirm dialog)
 *
 * Values persist via the shared store under their own keys (see store.h's
 * typed accessors).
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
    frij_store_save_int("volume", lv_slider_get_value(sl));
    // TODO(device): apply to the ES8311 codec.
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
    // TODO(device): apply a screen-off timeout.
}

static void on_autosync(lv_event_t* e)
{
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    frij_store_save_bool("autosync", lv_obj_has_state(sw, LV_STATE_CHECKED));
}

static void on_raise_wake(lv_event_t* e)
{
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    frij_store_save_bool("raisewake", lv_obj_has_state(sw, LV_STATE_CHECKED));
    // TODO(device): arm BMI270 tilt/raise interrupt to wake the display.
}

static void on_touch_sfx(lv_event_t* e)
{
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    frij_store_save_bool("touchsfx", lv_obj_has_state(sw, LV_STATE_CHECKED));
    // TODO(device): play a click on the ES8311 codec on touch.
}

static void on_sync_now(lv_event_t* e)
{
    (void)e;
    frij_store_pull_async("todo");  // best-effort cloud refresh
    frij_store_pull_async("counter");
    frij_store_save_int("last_sync", (int)time(NULL));
    frij_haptic(FRIJ_HAPTIC_SUCCESS);
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
    frij_set_brightness(80);
    frij_haptics_set_enabled(true);
    frij_haptic(FRIJ_HAPTIC_SUCCESS);
    frij_toast("Settings reset");
}

static void on_reset(lv_event_t* e)
{
    (void)e;  // destructive: confirm first
    frij_confirm("Reset settings?", "Restore everything to defaults.", "Reset", ACCENT, do_reset);
}

static void do_erase(lv_event_t* e)
{
    (void)e;
    frij_store_clear();  // wipe todos / counter / settings; defaults return on next read
    frij_set_brightness(80);
    frij_haptics_set_enabled(true);
    frij_haptic(FRIJ_HAPTIC_SUCCESS);
    frij_toast("All data erased");
}

static void on_erase(lv_event_t* e)
{
    (void)e;  // destructive: confirm first
    frij_confirm("Erase all data?", "Removes todos, counter and settings.", "Erase", ACCENT, do_erase);
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
    bool forget = (s_sel_kind != 0 && opt == 1);  // option 1 is Forget when present
    if (forget) {
        frij_wifi_forget(s_sel);
        lv_snprintf(msg, sizeof(msg), "Forgot %s", s_sel);
    } else if (s_sel_kind == 2) {  // connected, Disconnect
        frij_wifi_disconnect();
        lv_snprintf(msg, sizeof(msg), "Disconnected");
    } else {  // Connect (new or saved)
        frij_wifi_connect(s_sel, NULL);
        lv_snprintf(msg, sizeof(msg), "Connected to %s", s_sel);
    }
    frij_haptic(FRIJ_HAPTIC_SELECT);
    net_refresh();
    frij_toast(msg);
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
    static const char* opt_new[]       = {"Connect"};
    if (nw->connected) {
        s_sel_kind = 2;
        frij_action_sheet(nw->ssid, opt_connected, 2, ACCENT, net_action_cb, NULL);
    } else if (nw->known) {
        s_sel_kind = 1;
        frij_action_sheet(nw->ssid, opt_saved, 2, ACCENT, net_action_cb, NULL);
    } else {
        s_sel_kind = 0;
        frij_action_sheet(nw->ssid, opt_new, 1, ACCENT, net_action_cb, NULL);
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
            {
                // hero: name + version, with generous breathing room around it
                lv_obj_t* hero = frij_label(col, "Frij", FRIJ_FONT_TITLE, FRIJ_TEXT);
                lv_obj_set_style_margin_top(hero, FRIJ_SP_XXL * 4, LV_PART_MAIN);  // 36
                lv_obj_t* ver = frij_label(col, "on-device UI  -  v0.1", FRIJ_FONT_BODY, FRIJ_TEXT_2);
                lv_obj_set_style_margin_bottom(ver, FRIJ_SP_XXL * 4, LV_PART_MAIN);  // 36

                char bbuf[24];
                lv_snprintf(bbuf, sizeof(bbuf), "%d%%%s", frij_battery_pct(),
                            frij_battery_charging() ? "  " LV_SYMBOL_CHARGE : "");
                frij_value_row(col, "Battery", bbuf);

                char        sbuf[24];
                int         ls = frij_store_load_int("last_sync", 0);
                if (ls > 0) {
                    time_t    tt = (time_t)ls;
                    struct tm tmv;
                    localtime_r(&tt, &tmv);
                    frij_format_time(sbuf, sizeof(sbuf), &tmv);  // respects 24-hour setting
                } else {
                    lv_snprintf(sbuf, sizeof(sbuf), "Never");
                }
                frij_value_row(col, "Last sync", sbuf);

                frij_action_row(col, "Sync now", on_sync_now);
                frij_action_row(col, "Reset settings", on_reset);
                frij_action_row(col, "Erase all data", on_erase);
                frij_stagger_in(col, 30);
            }
            break;
    }
}

const frij_app_t* settings_app(void)
{
    static const frij_app_t app = {"Settings", ACCENT, NULL, 3, screen};
    return &app;
}
