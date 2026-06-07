#include "settings.h"

// One entry per settings screen. Add more here later (brightness, wifi, …).
int frij_settings_screen_count(void)
{
    return 2;
}

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

void frij_settings_build_screen(lv_obj_t* parent, int index)
{
    if (index == 0) {
        title_and_hint(parent, "Display", "brightness\n(soon)");
    } else {
        title_and_hint(parent, "About", "Frij\non-device UI");
    }
}
