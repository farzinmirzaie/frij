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

// loose coupling: the launcher provides this; we don't include launcher.h
extern void frij_launcher_refresh_action(void);

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
        if (strcmp(key, "volume") == 0) {
            frij_audio_click();  // preview the level you just set (device speaker)
        }
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
    frij_clock_set_24h(lv_obj_has_state(sw, LV_STATE_CHECKED));  // caches + persists
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

// Debug overlay: toggles LVGL's built-in FPS/CPU performance monitor (centered).
static void on_debug(lv_event_t* e)
{
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    bool      on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    frij_store_save_bool("debug", on);
    frij_debug_overlay_set(on);
}

static void on_sync_now(lv_event_t* e)
{
    (void)e;
    frij_store_pull_async("todo");  // best-effort cloud refresh
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
    frij_clock_set_24h(true);  // updates the in-memory cache too
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
    frij_store_clear();  // wipe todos / events / scoreboard / settings; defaults return on next read
    // Wi-Fi credentials live in NVS, not the store — drop them too so "all
    // data" means all data.
    const char* cur = frij_wifi_connected();
    if (cur) {
        frij_wifi_forget(cur);
    }
    frij_set_brightness(80);
    frij_haptics_set_enabled(true);
    frij_anim_set_enabled(true);
    frij_clock_set_24h(true);  // re-sync the in-memory cache with the defaults
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

// Async Wi-Fi state: scan + connect run off the LVGL thread (they used to block
// it for seconds), and a poll timer reflects the result when ready.
static bool        s_net_scanning = false;
static bool        s_connecting   = false;
static bool        s_conn_via_pw  = false;  // connect started from the password keypad
static char        s_conn_ssid_ui[FRIJ_WIFI_SSID_MAX];
static lv_timer_t* s_net_poll = NULL;

static void build_network(lv_obj_t* col, bool rescan);

// Clear the cached Network page pointer when its page is destroyed.
static void on_net_deleted(lv_event_t* e)
{
    if (s_net_col == lv_event_get_target(e)) {
        s_net_col = NULL;
        // Leaving the screen: stop polling. Any in-flight connect still finishes
        // in the background; we just won't pop a result over another screen.
        s_net_scanning = false;
        s_connecting   = false;
        if (s_net_poll) {
            lv_timer_delete(s_net_poll);
            s_net_poll = NULL;
        }
    }
}

// Rebuild the Network page. `rescan` runs a radio scan — only wanted for the
// explicit refresh action / radio-on; row actions (connect/forget/disconnect)
// update the cached scan in place, since a device scan blocks for 1–2s.
static void net_refresh(bool rescan)
{
    if (!s_net_col) {
        return;
    }
    int32_t y = lv_obj_get_scroll_y(s_net_col);  // keep the user's place
    lv_obj_clean(s_net_col);  // keeps the page's styles/padding, drops the rows
    build_network(s_net_col, rescan);
    frij_page_settle_at(s_net_col, y);
}

// Find a cached scan entry by SSID (rows act on these).
static frij_wifi_net_t* scan_find(const char* ssid)
{
    for (int i = 0; i < s_scan_n; i++) {
        if (strncmp(s_scan[i].ssid, ssid, FRIJ_WIFI_SSID_MAX) == 0) {
            return &s_scan[i];
        }
    }
    return NULL;
}

static void scan_mark_connected(const char* ssid)  // NULL = nothing connected
{
    for (int i = 0; i < s_scan_n; i++) {
        s_scan[i].connected = ssid && strncmp(s_scan[i].ssid, ssid, FRIJ_WIFI_SSID_MAX) == 0;
        if (s_scan[i].connected) {
            s_scan[i].known = true;
        }
    }
}

// Order the scan: the connected network first, then strongest signal (rssi closer
// to 0) to weakest. Insertion sort — the list is tiny (<= 12). Called before the
// rows are built so the row index -> s_scan mapping stays consistent.
static void scan_sort(void)
{
    for (int i = 1; i < s_scan_n; i++) {
        frij_wifi_net_t key = s_scan[i];
        int             j   = i - 1;
        while (j >= 0 && ((key.connected && !s_scan[j].connected) ||
                          (key.connected == s_scan[j].connected && key.rssi > s_scan[j].rssi))) {
            s_scan[j + 1] = s_scan[j];
            j--;
        }
        s_scan[j + 1] = key;
    }
}

// Poll the async scan/connect and reflect results when ready. Self-stops when
// nothing is pending.
static void net_poll_cb(lv_timer_t* t)
{
    if (s_net_scanning) {
        int n = frij_wifi_scan_poll(s_scan, (int)(sizeof(s_scan) / sizeof(s_scan[0])));
        if (n >= 0) {  // scan finished
            s_scan_n       = n;
            s_net_scanning = false;
            net_refresh(false);  // swap the spinner for the list
        }
    }
    if (s_connecting) {
        frij_wifi_state_t st = frij_wifi_connect_poll();
        if (st != FRIJ_WIFI_CONNECTING) {
            s_connecting = false;
            bool ok      = (st == FRIJ_WIFI_CONNECTED);
            if (ok) {
                scan_mark_connected(s_conn_ssid_ui);
            }
            frij_haptic(ok ? FRIJ_HAPTIC_SUCCESS : FRIJ_HAPTIC_SELECT);
            net_refresh(false);
            if (s_conn_via_pw) {
                frij_result_screen(ok, ok ? "Connected" : "Couldn't connect", s_conn_ssid_ui,
                                   ok ? "Done" : "Close");
            } else {
                char msg[64];
                lv_snprintf(msg, sizeof(msg), ok ? "Connected to %s" : "Couldn't connect",
                            s_conn_ssid_ui);
                frij_toast_status(msg, ok);
            }
        }
    }
    if (!s_net_scanning && !s_connecting) {
        lv_timer_delete(t);
        s_net_poll = NULL;
    }
}

static void net_poll_ensure(void)
{
    if (!s_net_poll) {
        s_net_poll = lv_timer_create(net_poll_cb, 150, NULL);
    }
}

// Kick a background scan (non-blocking) and show the spinner until it lands.
static void net_scan_start(void)
{
    frij_wifi_scan_start();
    s_net_scanning = true;
    net_poll_ensure();
}

static void net_action_cb(int opt, void* user)
{
    (void)user;
    char msg[64];
    bool ok     = true;
    bool forget = (s_sel_kind != NET_NEW && opt == 1);  // option 1 is Forget when present
    if (forget) {
        frij_wifi_forget(s_sel);
        frij_wifi_net_t* nw = scan_find(s_sel);
        if (nw) {
            nw->known     = false;
            nw->connected = false;
        }
        lv_snprintf(msg, sizeof(msg), "Forgot %s", s_sel);
    } else if (s_sel_kind == NET_CONNECTED) {  // Disconnect
        frij_wifi_disconnect();
        scan_mark_connected(NULL);
        lv_snprintf(msg, sizeof(msg), "Disconnected");
    } else {  // Connect with saved/no credentials — async, result via the poll
        strncpy(s_conn_ssid_ui, s_sel, sizeof(s_conn_ssid_ui) - 1);
        s_conn_ssid_ui[sizeof(s_conn_ssid_ui) - 1] = '\0';
        s_conn_via_pw = false;
        frij_wifi_connect_start(s_sel, NULL);
        s_connecting = true;
        net_poll_ensure();
        frij_haptic(FRIJ_HAPTIC_SELECT);
        frij_toast("Connecting...");
        return;  // toast with the outcome comes from net_poll_cb
    }
    frij_haptic(FRIJ_HAPTIC_SELECT);
    net_refresh(false);  // statuses updated locally — no blocking radio rescan
    frij_toast_status(msg, ok);
}

// Keypad callback: join the selected network with the typed password, then
// conclude the flow with a full-screen result (big check / cross).
static void wifi_pw_done(const char* pw, void* user)
{
    (void)user;
    // Async connect — the keypad closes, a "Connecting..." toast shows, and the
    // poll delivers the full-screen result. (Used to block ~8s and freeze the UI.)
    strncpy(s_conn_ssid_ui, s_sel, sizeof(s_conn_ssid_ui) - 1);
    s_conn_ssid_ui[sizeof(s_conn_ssid_ui) - 1] = '\0';
    s_conn_via_pw = true;
    frij_wifi_connect_start(s_sel, pw);
    s_connecting = true;
    net_poll_ensure();
    frij_toast("Connecting...");
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
    net_refresh(true);  // radio state changed — scan fresh
    frij_launcher_refresh_action();  // show/hide the header rescan button
}

static void build_network(lv_obj_t* col, bool rescan)
{
    // Master Wi-Fi switch (whole row toggles it).
    bool      on = frij_wifi_enabled();
    lv_obj_t* sw = frij_toggle_row(col, "Wi-Fi", on, ACCENT);
    lv_obj_add_event_cb(sw, wifi_master_cb, LV_EVENT_VALUE_CHANGED, NULL);
    if (!on) {
        s_net_scanning = false;  // radio off — nothing to scan
        // radio off: toggle stays pinned at the top; float the hint at the
        // screen's center (FLOATING so it ignores the top-pinned flex flow).
        lv_obj_t* hint = frij_empty_state(col, "Wi-Fi is off",
                                          "Turn on to see\nnearby networks");
        lv_obj_add_flag(hint, LV_OBJ_FLAG_FLOATING);
        lv_obj_align(hint, LV_ALIGN_CENTER, 0, 60);  // centered in the page area
        frij_stagger_in(col, 40);
        return;
    }

    frij_section_label(col, "Networks");  // grouped-list heading, like Display/Sound

    // Kick a background scan when asked (refresh / radio just turned on) or when
    // we have nothing cached yet. It runs off-thread — no UI freeze.
    if (rescan || (s_scan_n == 0 && !s_net_scanning)) {
        net_scan_start();
    }
    if (s_net_scanning) {  // scan in flight — keep the connection visible meanwhile
        const char* cur = frij_wifi_connected();
        if (cur && cur[0]) {
            // Show the active network (not a blank "scanning" screen) so it's clear
            // something's connected; not tappable mid-scan (no s_scan entry yet).
            lv_obj_t* r    = frij_surface_row(col);
            frij_label(r, LV_SYMBOL_OK, FRIJ_FONT_SYMBOL, ACCENT);
            lv_obj_t* name = frij_label(r, cur, FRIJ_FONT_BODY, ACCENT);
            lv_obj_set_flex_grow(name, 1);
            lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
            frij_label(r, LV_SYMBOL_WIFI, FRIJ_FONT_SYMBOL, FRIJ_TEXT);
            frij_label(col, "Scanning...", FRIJ_FONT_SMALL, FRIJ_TEXT_2);  // footer hint
        } else {
            lv_obj_t* hint = frij_empty_state(col, "Scanning...", "Looking for\nnearby networks");
            lv_obj_add_flag(hint, LV_OBJ_FLAG_FLOATING);
            lv_obj_align(hint, LV_ALIGN_CENTER, 0, 60);
        }
        frij_stagger_in(col, 40);
        return;
    }
    if (s_scan_n == 0) {
        char diag[48];
        frij_wifi_diag(diag, sizeof(diag));
        char sub[80];
        lv_snprintf(sub, sizeof(sub), "Tap refresh to scan\n%s", diag);
        frij_empty_state(col, "No networks", sub);
        frij_stagger_in(col, 40);
        return;
    }
    scan_sort();  // connected first, then strongest signal
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
    lv_obj_t* val = (lv_obj_t*)lv_observer_get_target(obs);
    // Just the level %, no charge bolt (kept simple). Read the subject, not the
    // live PMIC call.
    int pct = lv_subject_get_int(frij_battery_level_subject());
    lv_label_set_text_fmt(val, "%d%%", pct);
    lv_obj_set_style_text_color(val, lv_color_hex(pct <= 15 ? FRIJ_WARNING : FRIJ_TEXT_2),
                                LV_PART_MAIN);
}

static void build_about(lv_obj_t* col)
{
    // hero: the logo clover + wordmark, with generous breathing room around it
    lv_obj_t* hero = frij_logo(col, 72, true);
    lv_obj_set_style_margin_top(hero, FRIJ_SP_XXL * 2, LV_PART_MAIN);  // 48
    // version + when this firmware was compiled ("which build is on the fridge?")
    lv_obj_t* ver = frij_label(col, FRIJ_VERSION ", " __DATE__, FRIJ_FONT_BODY, FRIJ_TEXT_2);
    lv_obj_set_style_margin_bottom(ver, FRIJ_SP_XXL * 2, LV_PART_MAIN);  // 48

    // Battery row — bound to the battery subjects so it tracks live (observer),
    // instead of sampling once when About is built.
    // uptime since boot — a quick "has it been rebooting?" health check
    uint32_t  up = lv_tick_get() / 60000u;  // minutes
    char      upbuf[20];
    if (up >= 60) {
        lv_snprintf(upbuf, sizeof(upbuf), "%uh %um", (unsigned)(up / 60), (unsigned)(up % 60));
    } else {
        lv_snprintf(upbuf, sizeof(upbuf), "%um", (unsigned)up);
    }
    frij_value_row(col, "Uptime", upbuf);

    frij_battery_poll();  // sample now so the percent is right on open
    lv_obj_t* brow = frij_value_row(col, "Battery", "");
    lv_obj_t* bval = lv_obj_get_child(brow, 1);
    lv_subject_add_observer_obj(frij_battery_level_subject(), about_battery_cb, bval, NULL);

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

    frij_section_label(col, "Debug");
    // Build from the LIVE overlay state, not the store: the device store doesn't
    // persist yet, so reading it showed the toggle off while the overlay was on.
    lv_obj_t* dsw = frij_toggle_row(col, "Performance overlay", frij_debug_overlay_get(), ACCENT);
    lv_obj_add_event_cb(dsw, on_debug, LV_EVENT_VALUE_CHANGED, NULL);

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
            build_network(col, true);
            break;

        default:  // System / About
            s_about_col = col;
            lv_obj_add_event_cb(col, on_about_deleted, LV_EVENT_DELETE, NULL);
            build_about(col);
            break;
    }
}

// Header action: a rescan button on the Network screen — but only while Wi-Fi
// is on (nothing to scan when the radio is off).
static const char* st_action(int index)
{
    return (index == 1 && frij_wifi_enabled()) ? LV_SYMBOL_REFRESH : NULL;
}

static void st_on_action(int index)
{
    if (index != 1 || !s_net_col) {
        return;
    }
    net_refresh(true);  // explicit rescan
    frij_toast("Scanning...");
}

const frij_app_t* settings_app(void)
{
    static const frij_app_t app = {"Settings", ACCENT, NULL, 3, screen, st_action, st_on_action};
    return &app;
}
