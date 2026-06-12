#ifndef FRIJ_APP_ASSISTANT_H
#define FRIJ_APP_ASSISTANT_H

#include <stdbool.h>

#include "app.h"

#ifdef __cplusplus
extern "C" {
#endif

const frij_app_t* assistant_app(void);

// Push-to-talk from the input layer (Key B on device, Space in the emulator):
// `pressed`=true opens the listening overlay and (on device) starts mic
// capture; false stops capture and asks the cloud — device sends the recorded
// audio, the emulator sends a sample question. Falls back to a canned answer
// only when the cloud is unconfigured.
void frij_assistant_ptt(bool pressed);

#if defined(FRIJ_SNAPSHOT)
void frij_assistant_demo_error(void);     // snapshot-only: render the error state
void frij_assistant_demo_thinking(void);  // snapshot-only: render the thinking state
#endif

#ifdef __cplusplus
}
#endif

#endif  // FRIJ_APP_ASSISTANT_H
