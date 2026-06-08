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

int main(int, char**)
{
    SDL_SetMainReady();
    lv_init();
    lv_tick_set_cb(SDL_GetTicks);  // give LVGL a millisecond clock (else nothing renders)
    lv_sdl_window_create(FRIJ_RES, FRIJ_RES);
    lv_sdl_mouse_create();  // drag = touch

    // The launcher clips its UI to a circle and fills the corners with the
    // "outside" color, so the round shape is simulated here automatically.
    user_app();

    while (true) {
        lv_timer_handler();
        SDL_Delay(5);
    }
    return 0;
}

#endif  // FRIJ_USE_LV_SDL
