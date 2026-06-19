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

void frij_audio_status(bool ok)
{
    (void)ok;
}

void frij_audio_idle_tick(void) {}

#else

#include <M5Unified.h>

static uint32_t s_last_sound = 0;  // millis() of the last tone (0 = amp already off)

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
        M5.Speaker.begin();  // the amp is kept off (no idle hiss); wake it for the tone
        M5.Speaker.tone(2500.0f, 6);  // short, quiet UI tick
        s_last_sound = millis();
    }
}

void frij_audio_status(bool ok)
{
    if (!s_click_enabled) {  // same master switch as the touch click
        return;
    }
    M5.Speaker.begin();  // amp is off by default; wake it for the feedback tone
    if (ok) {  // quick rising two-tone = done
        M5.Speaker.tone(1800.0f, 40);
        M5.Speaker.tone(2400.0f, 60);
    } else {  // single low buzz = failed
        M5.Speaker.tone(600.0f, 120);
    }
    s_last_sound = millis();
}

// Idle amp cleanup: once nothing has played for ~2s (and no tone is mid-flight),
// power the ES8311 down — leaving it begun hisses (white noise). Called every
// loop. 2s is long enough that normal click bursts never cycle the amp.
void frij_audio_idle_tick(void)
{
    if (s_last_sound && millis() - s_last_sound > 2000 && !M5.Speaker.isPlaying()) {
        M5.Speaker.end();
        s_last_sound = 0;
    }
}

#endif
