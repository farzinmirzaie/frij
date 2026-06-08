#include "settings.h"

#include <stdlib.h>  // atoi

#include "store/store.h"
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
    // TODO(device): apply to the backlight (e.g. M5.Display.setBrightness).
}

static void on_clock24(lv_event_t* e)
{
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    save_bool("clock24", lv_obj_has_state(sw, LV_STATE_CHECKED));
}

// ---- screens ---------------------------------------------------------------

static void screen(lv_obj_t* parent, int index)
{
    lv_obj_t* col = frij_page(parent);

    if (index == 0) {
        frij_label(col, "Display", FRIJ_FONT_TITLE, FRIJ_TEXT);
        frij_label(col, "Brightness", FRIJ_FONT_BODY, FRIJ_TEXT_2);
        lv_obj_t* sl = frij_slider(col, 10, 100, load_int("brightness", 80), ACCENT);
        lv_obj_add_event_cb(sl, on_brightness, LV_EVENT_VALUE_CHANGED, NULL);

    } else if (index == 1) {
        frij_label(col, "General", FRIJ_FONT_TITLE, FRIJ_TEXT);
        lv_obj_t* row = frij_surface_row(col);
        lv_obj_t* lbl = frij_label(row, "24-hour time", FRIJ_FONT_BODY, FRIJ_TEXT);
        lv_obj_set_flex_grow(lbl, 1);
        lv_obj_t* tog = frij_toggle(row, load_bool("clock24", true), ACCENT);
        lv_obj_add_event_cb(tog, on_clock24, LV_EVENT_VALUE_CHANGED, NULL);

    } else {
        frij_label(col, "Frij", FRIJ_FONT_TITLE, FRIJ_TEXT);
        frij_label(col, "on-device UI", FRIJ_FONT_BODY, FRIJ_TEXT_2);
        frij_label(col, "v0.1", FRIJ_FONT_BODY, FRIJ_TEXT_2);
    }
}

const frij_app_t* settings_app(void)
{
    static const frij_app_t app = {"Settings", ACCENT, NULL, 3, screen};
    return &app;
}
