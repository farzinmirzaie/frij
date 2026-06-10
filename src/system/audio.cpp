#include "audio.h"

/*
 * Audio is board-specific. Compiled for both targets behind the same SDL/device
 * guard as brightness.cpp. The click-enabled flag lives outside the guard so the
 * setting is tracked identically on both.
 */
static bool s_click_enabled = false;

#if defined(__has_include) && (__has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>))

// Emulator: no speaker.
void frij_set_volume(uint8_t pct)
{
    (void)pct;
}

void frij_audio_set_click_enabled(bool on)
{
    s_click_enabled = on;
}

void frij_audio_click(void) {}

#else

#include <M5Unified.h>

void frij_set_volume(uint8_t pct)
{
    if (pct > 100) {
        pct = 100;
    }
    M5.Speaker.setVolume((uint8_t)(pct * 255 / 100));  // ES8311 via M5Unified
}

void frij_audio_set_click_enabled(bool on)
{
    s_click_enabled = on;
}

void frij_audio_click(void)
{
    if (s_click_enabled) {
        M5.Speaker.tone(2500.0f, 6);  // short, quiet UI tick
    }
}

#endif
