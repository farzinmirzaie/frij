#include "input.h"

#include "lvgl.h"
#include "lvgl_port_m5stack.hpp"
#include "launcher.h"

/*
 * Emulator: poll the SDL keyboard. SDL_GetKeyboardState reads a snapshot kept
 * up to date by the SDL panel's own event loop, so it doesn't steal events.
 * Backspace = Back. On real hardware this file will read a button GPIO instead.
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
static void poll_back(lv_timer_t* timer)
{
    (void)timer;
    static bool was_down = false;
    const Uint8* keys    = SDL_GetKeyboardState(NULL);
    bool down            = keys && keys[SDL_SCANCODE_BACKSPACE];
    if (down && !was_down) {
        frij_back();
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
