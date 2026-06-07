#include "lvgl.h"
#include "lvgl_port_m5stack.hpp"

/*
 * user_app() is called once at startup, after the display + LVGL are ready.
 * This is YOUR entry point — build the Frij UI from here.
 *
 * The screen is round, so keep important content near the center.
 *
 * LVGL rule: any time you create/modify UI objects, wrap it in
 * lvgl_port_lock() / lvgl_port_unlock() so you don't fight the render task.
 */
void user_app(void)
{
    if (!lvgl_port_lock()) {
        return;
    }

    // Fill the screen with a dark background.
    lv_obj_t* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101418), LV_PART_MAIN);

    // A centered title label as a starting point.
    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "Frij");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    lvgl_port_unlock();
}
