#include "input.h"

#include "lvgl.h"
#include "lvgl_port_m5stack.hpp"
#include "launcher.h"

/*
 * Emulator: poll the SDL keyboard. SDL_GetKeyboardState reads a snapshot kept
 * up to date by the SDL panel's own event loop, so it doesn't steal events.
 * Backspace or Esc = the Back button: a short press goes back one layer, a
 * HOLD (600ms) jumps all the way to the watch face. Short-press fires on
 * release — the only way to tell it apart from the start of a hold.
 * On real hardware this file reads Key A instead (M5.BtnA short/hold).
 */
#if defined(__has_include)
#  if __has_include(<SDL2/SDL.h>)
#    include <SDL2/SDL.h>
#    define FRIJ_HAS_SDL 1
#  elif __has_include(<SDL.h>)
#    include <SDL.h>
#    define FRIJ_HAS_SDL 1
#  endif
#endif

#ifdef FRIJ_HAS_SDL
#define HOLD_MS 600

static void poll_back(lv_timer_t* timer)
{
    (void)timer;
    static bool     was_down  = false;
    static bool     consumed  = false;  // this press already did its action
    static uint32_t down_tick = 0;
    const Uint8*    keys      = SDL_GetKeyboardState(NULL);
    bool down = keys && (keys[SDL_SCANCODE_BACKSPACE] || keys[SDL_SCANCODE_ESCAPE]);

    if (down && !was_down) {  // press started
        down_tick = lv_tick_get();
        consumed  = false;
    } else if (down && !consumed && lv_tick_elaps(down_tick) >= HOLD_MS) {
        consumed = true;  // held long enough: take me home (once)
        frij_home();
    } else if (!down && was_down && !consumed) {
        frij_back();  // short press, decided on release
    }
    was_down = down;
}
#endif

void frij_input_init(void)
{
#ifdef FRIJ_HAS_SDL
    if (lvgl_port_lock()) {
        lv_timer_create(poll_back, 50, NULL);
        lvgl_port_unlock();
    }
#endif
}
