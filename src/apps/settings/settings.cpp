#include "settings.h"

#include <string.h>
#include <time.h>

#include "store/store.h"
#include "system/audio.h"
#include "system/battery.h"
#include "system/brightness.h"
#include "system/haptics.h"
#include "system/storage.h"
#include "system/wifi.h"
#include "core/datetime.h"
#include "ui/anim.h"
#include "ui/components.h"
#include "ui/theme.h"

static void build_about(lv_obj_t* col);  // forward (used by Sync now's refresh)
static lv_obj_t* s_about_col = NULL;     // the About page, for in-place refresh

// Rebuild the About page in place, keeping the user's scroll position.
static void about_refresh(void)
{
    if (!s_about_col) {
        return;
    }
    int32_t y = lv_obj_get_scroll_y(s_about_col);
    lv_obj_clean(s_about_col);
    build_about(s_about_col);
    frij_page_settle_at(s_about_col, y);
}

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

// Sliders: apply LIVE while dragging (cheap), but persist only on release —
// frij_store_save also pushes to the cloud, so saving per-tick during a drag
// would hammer the store. The slider's user_data carries its store key.
static void on_slider_release(lv_event_t* e)
{
    lv_obj_t*   s   = (lv_obj_t*)lv_event_get_current_target(e);
    const char* key = (const char*)lv_obj_get_user_data(s);
    if (key) {
        frij_store_save_int(key, lv_slider_get_value(s));
        frij_haptic(FRIJ_HAPTIC_SELECT);  // value committed
    }
}

static void on_brightness(lv_event_t* e)
{
    lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
    frij_set_brightness((uint8_t)lv_slider_get_value(slider));  // live preview
}

static void on_clock24(lv_event_t* e)
{
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    frij_store_save_bool("clock24", lv_obj_has_state(sw, LV_STATE_CHECKED));
}

static void on_volume(lv_event_t* e)
{
    lv_obj_t* sl = (lv_obj_t*)lv_event_get_target(e);
    frij_set_volume((uint8_t)lv_slider_get_value(sl));  // live; persisted on release
}

static void on_vibration(lv_event_t* e)
{
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    bool      on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    frij_haptics_set_enabled(on);
    frij_store_save_bool("haptics", on);
}

// Sleep needs no live apply — the idle sleep manager (system/sleep) reads the
// stored minutes each tick, and on_slider_release persists them.

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
    about_refresh();  // reflect the new "Last sync" immediately
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
    about_refresh();  // refresh the visible rows
    frij_haptic(FRIJ_HAPTIC_SUCCESS);
    frij_toast_status("Settings reset", true);
}

static void on_reset(lv_event_t* e)
{
    (void)e;  // destructive: full-screen prompt, danger primary
    frij_prompt_screen(LV_SYMBOL_REFRESH, FRIJ_DANGER, "Reset settings?",
                       "Restore everything to defaults.", "Reset", "Cancel", do_reset);
}

static void do_erase(lv_event_t* e)
{
    (void)e;
    frij_store_clear();  // wipe todos / counter / settings; defaults return on next read
    frij_set_brightness(80);
    frij_haptics_set_enabled(true);
    frij_anim_set_enabled(true);
    about_refresh();  // refresh the visible rows (Last sync -> Never, etc.)
    frij_haptic(FRIJ_HAPTIC_SUCCESS);
    frij_toast_status("All data erased", true);
}

static void on_erase(lv_event_t* e)
{
    (void)e;  // destructive: full-screen prompt, danger primary
    // honest scope: this clears the device; cloud-synced rows re-download later
    frij_prompt_screen(LV_SYMBOL_TRASH, FRIJ_DANGER, "Erase all data?",
                       "Clears this device. Synced data re-downloads.", "Erase", "Cancel",
                       do_erase);
}

// ---- row helpers -----------------------------------------------------------

static void toggle_row(lv_obj_t* col, const char* text, const char* key, bool def, lv_event_cb_t cb)
{
    lv_obj_t* sw = frij_toggle_row(col, text, frij_store_load_bool(key, def), ACCENT);
    lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, NULL);
}

static void slider_row(lv_obj_t* col, const char* text, int min, int max, const char* key,
                       int def, lv_event_cb_t apply_cb, const char* unit)
{
    lv_obj_t* s = frij_slider_row(col, text, min, max, frij_store_load_int(key, def), ACCENT, unit);
    lv_obj_set_user_data(s, (void*)key);  // read back by on_slider_release
    if (apply_cb) {
        lv_obj_add_event_cb(s, apply_cb, LV_EVENT_VALUE_CHANGED, NULL);  // live apply
    }
    lv_obj_add_event_cb(s, on_slider_release, LV_EVENT_RELEASED, NULL);  // persist once
}

// ---- Network screen (Wi-Fi) ------------------------------------------------

static lv_obj_t*      s_net_col = NULL;            // the Network page, for in-place refresh
static frij_wifi_net_t s_scan[12];                 // last scan (rows index into this)
static int            s_scan_n   = 0;
typedef enum { NET_NEW, NET_SAVED, NET_CONNECTED } net_kind_t;

static char       s_sel[FRIJ_WIFI_SSID_MAX];  // the network the action sheet acts on
static net_kind_t s_sel_kind = NET_NEW;

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
    int32_t y = lv_obj_get_scroll_y(s_net_col);  // keep the user's place
    lv_obj_clean(s_net_col);  // keeps the page's styles/padding, drops the rows
    build_network(s_net_col);
    frij_page_settle_at(s_net_col, y);
}

static void net_action_cb(int opt, void* user)
{
    (void)user;
    char msg[64];
    bool ok     = true;
    bool forget = (s_sel_kind != NET_NEW && opt == 1);  // option 1 is Forget when present
    if (forget) {
        frij_wifi_forget(s_sel);
        lv_snprintf(msg, sizeof(msg), "Forgot %s", s_sel);
    } else if (s_sel_kind == NET_CONNECTED) {  // Disconnect
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

// Keypad callback: join the selected network with the typed password, then
// conclude the flow with a full-screen result (big check / cross).
static void wifi_pw_done(const char* pw, void* user)
{
    (void)user;
    bool ok = frij_wifi_connect(s_sel, pw);
    frij_haptic(ok ? FRIJ_HAPTIC_SUCCESS : FRIJ_HAPTIC_TAP);
    net_refresh();
    frij_result_screen(ok, ok ? "Connected" : "Couldn't connect", s_sel, ok ? "Done" : "Close");
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
        s_sel_kind = NET_CONNECTED;
        frij_action_sheet(nw->ssid, opt_connected, 2, ACCENT, net_action_cb, NULL);
    } else if (nw->known) {  // saved creds — connect without re-asking
        s_sel_kind = NET_SAVED;
        frij_action_sheet(nw->ssid, opt_saved, 2, ACCENT, net_action_cb, NULL);
    } else if (nw->secured) {  // new + secured — ask for the password (numeric keypad)
        s_sel_kind = NET_NEW;
        char prompt[64];
        lv_snprintf(prompt, sizeof(prompt), "Enter password for\n%s", nw->ssid);
        frij_numpad_prompt(prompt, wifi_pw_done, NULL);
    } else {  // new + open — just join
        s_sel_kind = NET_NEW;
        wifi_pw_done(NULL, NULL);
    }
}

static void wifi_master_cb(lv_event_t* e)
{
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    bool      on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    frij_wifi_set_enabled(on);
    frij_store_save_bool("wifi_on", on);  // restored at boot (user_app)
    net_refresh();
}

static void build_network(lv_obj_t* col)
{
    // Master Wi-Fi switch (whole row toggles it).
    bool      on = frij_wifi_enabled();
    lv_obj_t* sw = frij_toggle_row(col, "Wi-Fi", on, ACCENT);
    lv_obj_add_event_cb(sw, wifi_master_cb, LV_EVENT_VALUE_CHANGED, NULL);
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

    frij_section_label(col, "Networks");  // grouped-list heading, like Display/Sound

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

        // left: an accent check marks the active connection
        if (nw->connected) {
            frij_label(r, LV_SYMBOL_OK, FRIJ_FONT_SYMBOL, ACCENT);
        }
        lv_obj_t* name = frij_label(r, nw->ssid, FRIJ_FONT_BODY,
                                    nw->connected ? ACCENT : FRIJ_TEXT);
        lv_obj_set_flex_grow(name, 1);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);

        // right: "Saved" hint, a lock when secured, and the signal glyph
        if (nw->known && !nw->connected) {
            frij_label(r, "Saved", FRIJ_FONT_SMALL, FRIJ_TEXT_2);
        }
        if (nw->secured) {
            frij_lock_icon(r);
        }
        frij_label(r, LV_SYMBOL_WIFI, FRIJ_FONT_SYMBOL,
                   nw->rssi > -60 ? FRIJ_TEXT : FRIJ_TEXT_2);
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
    // hero: the logo clover + wordmark, with generous breathing room around it
    lv_obj_t* hero = frij_logo(col, 72, true);
    lv_obj_set_style_margin_top(hero, FRIJ_SP_XXL * 2, LV_PART_MAIN);  // 48
    lv_obj_t* ver = frij_label(col, FRIJ_VERSION, FRIJ_FONT_BODY, FRIJ_TEXT_2);
    lv_obj_set_style_margin_bottom(ver, FRIJ_SP_XXL * 2, LV_PART_MAIN);  // 48

    // Battery row — bound to the battery subjects so it tracks live (observer),
    // instead of sampling once when About is built.
    lv_obj_t* brow = frij_value_row(col, "Battery", "");
    lv_obj_t* bval = lv_obj_get_child(brow, 1);
    lv_subject_add_observer_obj(frij_battery_level_subject(), about_battery_cb, bval, NULL);
    lv_subject_add_observer_obj(frij_battery_charging_subject(), about_battery_cb, bval, NULL);

    char stbuf[24];
    frij_storage_free_str(stbuf, sizeof(stbuf));  // "12.3 MB free" (flash on device)
    frij_value_row(col, "Storage", stbuf);

    char sbuf[24];
    int  ls = frij_store_load_int("last_sync", 0);
    if (ls > 0) {
        frij_format_relative(sbuf, sizeof(sbuf), (time_t)ls);  // "Just now" / "5m ago"
    } else {
        lv_snprintf(sbuf, sizeof(sbuf), "Never");
    }
    // One row does both: tap to sync, right side shows when it last happened.
    lv_obj_t* srow = frij_surface_row(col);
    lv_obj_t* slbl = frij_label(srow, "Sync now", FRIJ_FONT_BODY, FRIJ_TEXT);
    lv_obj_set_flex_grow(slbl, 1);
    frij_label(srow, sbuf, FRIJ_FONT_BODY, FRIJ_TEXT_2);
    lv_obj_add_event_cb(srow, on_sync_now, LV_EVENT_CLICKED, NULL);

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
            slider_row(col, "Sleep", 1, 30, "sleep", 5, NULL, " min");
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

// Header action: a rescan button on the Network screen.
static const char* st_action(int index)
{
    return index == 1 ? LV_SYMBOL_REFRESH : NULL;
}

static void st_on_action(int index)
{
    if (index != 1 || !s_net_col) {
        return;
    }
    net_refresh();  // build_network() rescans
    frij_toast("Scanning...");
}

const frij_app_t* settings_app(void)
{
    static const frij_app_t app = {"Settings", ACCENT, NULL, 3, screen, st_action, st_on_action};
    return &app;
}
