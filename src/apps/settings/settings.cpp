#include "settings.h"

/*
 * Settings — a normal app (glance unused; reached by swiping down). Each screen
 * is one settings area. Add more screens here over time; with >1 it loops.
 */

static void title_and_hint(lv_obj_t* parent, const char* title, const char* hint)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t* t = lv_label_create(parent);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_color(t, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

    lv_obj_t* h = lv_label_create(parent);
    lv_label_set_text(h, hint);
    lv_obj_set_style_text_color(h, lv_color_hex(0x9AA3AD), LV_PART_MAIN);
    lv_obj_set_style_text_align(h, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
}

static void screen(lv_obj_t* parent, int index)
{
    if (index == 0) {
        title_and_hint(parent, "Display", "brightness\n(soon)");
    } else {
        title_and_hint(parent, "About", "Frij\non-device UI");
    }
}

const frij_app_t* settings_app(void)
{
    static const frij_app_t app = {"Settings", 0x22262B, NULL, 2, screen};
    return &app;
}
