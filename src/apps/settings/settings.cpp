#include "settings.h"

#include <stdlib.h>  // atoi

#include "store/store.h"
#include "system/brightness.h"
#include "system/haptics.h"
#include "ui/components.h"
#include "ui/theme.h"

/*
 * Settings — a normal app (glance unused; reached by swiping down). Screens:
 *   0 Display : brightness slider (persisted; applied on device — TODO)
 *   1 General : 24-hour time toggle (persisted; read by the Home clock)
 *   2 About   : name / version
 *
 * Values persist via the shared store under their own keys.
 */

static const uint32_t ACCENT = FRIJ_PRIMARY;  // purple

// ---- small store helpers --------------------------------------------------

static int load_int(const char* key, int def)
{
    char b[16];
    return frij_store_load(key, b, sizeof(b)) ? atoi(b) : def;
}

static void save_int(const char* key, int v)
{
    char b[16];
    lv_snprintf(b, sizeof(b), "%d", v);
    frij_store_save(key, b);
}

static bool load_bool(const char* key, bool def)
{
    char b[8];
    return frij_store_load(key, b, sizeof(b)) ? (b[0] == '1') : def;
}

static void save_bool(const char* key, bool v)
{
    frij_store_save(key, v ? "1" : "0");
}

// ---- handlers --------------------------------------------------------------

static void on_brightness(lv_event_t* e)
{
    lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
    int       v      = lv_slider_get_value(slider);
    save_int("brightness", v);
    frij_set_brightness((uint8_t)v);
}

static void on_clock24(lv_event_t* e)
{
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    save_bool("clock24", lv_obj_has_state(sw, LV_STATE_CHECKED));
}

static void on_volume(lv_event_t* e)
{
    lv_obj_t* sl = (lv_obj_t*)lv_event_get_target(e);
    save_int("volume", lv_slider_get_value(sl));
    // TODO(device): apply to the ES8311 codec.
}

static void on_vibration(lv_event_t* e)
{
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    bool      on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    frij_haptics_set_enabled(on);
    save_bool("haptics", on);
}

// ---- screens ---------------------------------------------------------------

static void toggle_row(lv_obj_t* col, const char* text, const char* key, bool def, lv_event_cb_t cb)
{
    lv_obj_t* row = frij_surface_row(col);
    lv_obj_t* lbl = frij_label(row, text, FRIJ_FONT_BODY, FRIJ_TEXT);
    lv_obj_set_flex_grow(lbl, 1);
    lv_obj_t* tog = frij_toggle(row, load_bool(key, def), ACCENT);
    lv_obj_add_event_cb(tog, cb, LV_EVENT_VALUE_CHANGED, NULL);
}

static void screen(lv_obj_t* parent, int index)
{
    lv_obj_t* col = frij_page(parent);

    switch (index) {
        case 0:  // General (brightness + volume + 24h + vibration)
            {
                lv_obj_t* b = frij_slider_row(col, "Brightness", 10, 100,
                                              load_int("brightness", 80), ACCENT);
                lv_obj_add_event_cb(b, on_brightness, LV_EVENT_VALUE_CHANGED, NULL);
                lv_obj_t* v = frij_slider_row(col, "Volume", 0, 100,
                                              load_int("volume", 60), ACCENT);
                lv_obj_add_event_cb(v, on_volume, LV_EVENT_VALUE_CHANGED, NULL);
                toggle_row(col, "24-hour time", "clock24", true, on_clock24);
                toggle_row(col, "Vibration", "haptics", true, on_vibration);
            }
            break;

        case 1:  // Network
            {
                lv_obj_t* row = frij_surface_row(col);
                lv_obj_t* lbl = frij_label(row, "Wi-Fi", FRIJ_FONT_BODY, FRIJ_TEXT);
                lv_obj_set_flex_grow(lbl, 1);
                frij_label(row, "on device", FRIJ_FONT_BODY, FRIJ_TEXT_2);
                frij_label(col, "Syncs to the cloud", FRIJ_FONT_BODY, FRIJ_TEXT_2);
            }
            break;

        default:  // About
            frij_label(col, "Frij", FRIJ_FONT_TITLE, FRIJ_TEXT);
            frij_label(col, "on-device UI", FRIJ_FONT_BODY, FRIJ_TEXT_2);
            frij_label(col, "v0.1", FRIJ_FONT_BODY, FRIJ_TEXT_2);
            break;
    }
}

const frij_app_t* settings_app(void)
{
    static const frij_app_t app = {"Settings", ACCENT, NULL, 3, screen};
    return &app;
}
