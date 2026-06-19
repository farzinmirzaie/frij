#ifndef FRIJ_AUDIO_H
#define FRIJ_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Audio — a board service (ES8311 codec + amp on device, no-op on the emulator).
 * Apps/components call these neutral functions; the board layer drives M5.Speaker.
 */

// Output volume, 0–100%.
void frij_set_volume(uint8_t pct);

// UI touch-click sound: enable/disable, and play one short click (only if
// enabled). frij_haptic_attach()'d widgets play a click on press.
void frij_audio_set_click_enabled(bool on);
void frij_audio_click(void);

// Outcome chirp for status toasts: rising two-tone on success, low buzz on
// failure. Gated by the same touch-sounds switch as the click.
void frij_audio_status(bool ok);

// Call once per main-loop iteration: powers the ES8311 amp down a beat after the
// last tone so it doesn't sit on and hiss (white noise). No-op on the emulator.
void frij_audio_idle_tick(void);

#endif  // FRIJ_AUDIO_H
