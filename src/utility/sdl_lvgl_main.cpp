/*
 * 466x466 round emulator built on LVGL's own SDL driver (not M5GFX).
 *
 * Used by the `emulator_StopWatch` env to preview the UI at the real device
 * resolution + round shape. App code is unchanged — it's pure LVGL. We provide
 * the lvgl_port_lock/unlock the apps expect (single SDL thread, so no-ops) and
 * our own main(); the M5GFX files are excluded from this env's build.
 */
#if defined(FRIJ_USE_LV_SDL)

#define SDL_MAIN_HANDLED  // we keep our own main()
#include <SDL2/SDL.h>

#include "lvgl.h"

extern void user_app(void);

extern "C" {
bool lvgl_port_lock(void)
{
    return true;
}
void lvgl_port_unlock(void) {}
}

static const int FRIJ_RES = 466;  // StopWatch round AMOLED

static void add_round_bezel(void)
{
    // A thin circle on the top layer marks where the round panel clips content.
    lv_obj_t* ring = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, FRIJ_RES, FRIJ_RES);
    lv_obj_center(ring);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(ring, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(ring, lv_color_hex(0x2A2A33), LV_PART_MAIN);
    lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(lv_layer_top(), LV_OBJ_FLAG_CLICKABLE);  // let input pass through
}

int main(int, char**)
{
    SDL_SetMainReady();
    lv_init();
    lv_sdl_window_create(FRIJ_RES, FRIJ_RES);
    lv_sdl_mouse_create();  // drag = touch

    add_round_bezel();
    user_app();

    while (true) {
        lv_timer_handler();
        SDL_Delay(5);
    }
    return 0;
}

#endif  // FRIJ_USE_LV_SDL
