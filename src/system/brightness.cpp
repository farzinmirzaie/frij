#include "brightness.h"

/*
 * Brightness is board-specific. This file is compiled for both targets (unlike
 * the device-only port), so it carries both implementations behind a guard.
 */
#if defined(__has_include) && (__has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>))

#include "lvgl.h"

// Emulator: no physical backlight, so fake it with a black scrim on the top
// layer (above everything, including modals). Its opacity tracks the inverse of
// brightness, so the slider visibly dims the screen like the real panel would.
void frij_set_brightness(uint8_t pct)
{
    if (pct > 100) {
        pct = 100;
    }
    static lv_obj_t* scrim = NULL;
    if (scrim == NULL) {
        scrim = lv_obj_create(lv_layer_top());
        lv_obj_remove_style_all(scrim);
        lv_obj_set_size(scrim, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(scrim, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_add_flag(scrim, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_clear_flag(scrim, LV_OBJ_FLAG_CLICKABLE);  // taps pass through
        lv_obj_clear_flag(scrim, LV_OBJ_FLAG_SCROLLABLE);
    }
    // Gentle: 100%→clear, 80% (default)→~8% dim, 10% (floor)→~35% dim.
    lv_obj_set_style_bg_opa(scrim, (lv_opa_t)(100 - pct), LV_PART_MAIN);
}

#else

#include <M5Unified.h>

void frij_set_brightness(uint8_t pct)
{
    if (pct > 100) {
        pct = 100;
    }
    M5.Display.setBrightness((uint8_t)(pct * 255 / 100));
}

#endif
