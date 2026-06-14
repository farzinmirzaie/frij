#include "input.h"

#include "lvgl.h"
#include "lvgl_port_m5stack.hpp"
#include "apps/assistant/assistant.h"
#include "launcher.h"

/*
 * Emulator: poll the SDL keyboard. SDL_GetKeyboardState reads a snapshot kept
 * up to date by the SDL panel's own event loop, so it doesn't steal events.
 * Backspace or Esc = the Back button: a short press goes back one layer, a
 * HOLD (600ms) jumps all the way to the watch face. Short-press fires on
 * release — the only way to tell it apart from the start of a hold.
 * Space = Key B (blue): push-to-talk for Frij AI — only a deliberate HOLD
 * (350ms) opens it (a stray tap shouldn't summon the assistant); release asks.
 * On real hardware this file reads Key A / Key B instead (M5.BtnA / M5.BtnB).
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

    // Key B (Space here): push-to-talk, hold-gated — listening only opens
    // after AI_HOLD_MS, so a stray tap does nothing.
    static bool     ai_was_down = false;
    static bool     ai_active   = false;
    static uint32_t ai_tick     = 0;
    bool ai_down = keys && keys[SDL_SCANCODE_SPACE];
    if (ai_down && !ai_was_down) {
        ai_tick   = lv_tick_get();
        ai_active = false;
    } else if (ai_down && !ai_active && lv_tick_elaps(ai_tick) >= 350) {
        ai_active = true;
        frij_assistant_ptt(true);
    } else if (!ai_down && ai_was_down && ai_active) {
        ai_active = false;
        frij_assistant_ptt(false);
    }
    ai_was_down = ai_down;
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
