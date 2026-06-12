#ifndef FRIJ_AI_H
#define FRIJ_AI_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Frij AI — board service for the cloud assistant (the Supabase "ask" edge
 * function, which holds the Gemini key and the store tools). Asks run on a
 * worker thread; the UI polls. Emulator: libcurl + the store's .env config.
 * Device: TODO (WiFiClientSecure) — unavailable means the UI mocks locally.
 */

typedef enum {
    FRIJ_AI_IDLE,   // nothing in flight
    FRIJ_AI_BUSY,   // waiting on the cloud
    FRIJ_AI_DONE,   // an answer is ready — frij_ai_take() it
    FRIJ_AI_ERROR,  // the ask failed — frij_ai_take() returns the error text
} frij_ai_state_t;

// True when the cloud is configured (store has Supabase creds).
bool frij_ai_available(void);

// Start a text ask on a worker thread. False if unavailable or one is already
// in flight. (Emulator path — the question is typed/mocked.)
bool frij_ai_ask(const char* question);

// ---- voice (device mic) ----------------------------------------------------
// Push-to-talk capture. On the device these record from the ES8311 mic and
// send the audio to the cloud (Gemini transcribes + answers in one call). On
// the emulator they are NO-OPS that return false, so the caller falls back to
// the text/mock path.

// Begin capturing while the button is held. True if real capture started
// (device + cloud available); false on the emulator.
bool frij_ai_listen_start(void);

// Stop capturing and send the clip as an ask (worker thread; poll state).
// True if an audio ask was dispatched; false if nothing was captured.
bool frij_ai_listen_ask(void);

frij_ai_state_t frij_ai_state(void);

// Copy the finished question/answer (or error text into `a`) and go IDLE.
void frij_ai_take(char* q, size_t q_n, char* a, size_t a_n);

#endif  // FRIJ_AI_H
