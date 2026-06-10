#include "display.h"

/*
 * Display power (panel sleep). Board-specific, compiled for both targets behind
 * the same SDL/device guard as brightness.cpp.
 */
#if defined(__has_include) && (__has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>))

#include "lvgl.h"

// Emulator: no real panel, so "off" is a full black overlay on the top layer.
// It's CLICKABLE while off so the waking tap is absorbed (it still resets LVGL's
// inactivity timer, which is what the sleep manager watches) instead of also
// pressing whatever is underneath.
void frij_display_set_on(bool on)
{
    static lv_obj_t* shade = NULL;
    if (shade == NULL) {
        shade = lv_obj_create(lv_layer_top());
        lv_obj_remove_style_all(shade);
        lv_obj_set_size(shade, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(shade, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_add_flag(shade, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_clear_flag(shade, LV_OBJ_FLAG_SCROLLABLE);
    }
    lv_obj_set_style_bg_opa(shade, on ? LV_OPA_TRANSP : LV_OPA_COVER, LV_PART_MAIN);
    if (on) {
        lv_obj_clear_flag(shade, LV_OBJ_FLAG_CLICKABLE);  // let taps through to the UI
    } else {
        lv_obj_add_flag(shade, LV_OBJ_FLAG_CLICKABLE);  // absorb the waking tap
    }
}

#else

#include <M5Unified.h>

#include "brightness.h"
#include "store/store.h"

void frij_display_set_on(bool on)
{
    if (on) {
        M5.Display.wakeup();
        frij_set_brightness((uint8_t)frij_store_load_int("brightness", 80));  // restore
    } else {
        M5.Display.setBrightness(0);
        M5.Display.sleep();
    }
}

#endif
