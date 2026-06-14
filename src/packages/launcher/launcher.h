#ifndef FRIJ_LAUNCHER_H
#define FRIJ_LAUNCHER_H

/*
 * The launcher: a looping carousel of app glances, plus navigation.
 *
 *   swipe left/right : move between app glances (wraps)
 *   swipe up         : open the current app (its own screen carousel)
 *   swipe down       : device settings
 *   Back (button)    : return to the launcher from an app or settings
 *   Back (hold)      : jump all the way to the main glance (the watch face)
 *
 * Call frij_launcher_start() once, after apps are registered.
 */
void frij_launcher_start(void);

// Return to the launcher from an app/settings layer. Called by the input layer
// (a hardware button on device, a key in the emulator). No-op on the launcher.
void frij_back(void);

// Hold-Back: close any modal/layer and land on the main glance (index 0),
// wherever the user is. The "take me home" escape hatch.
void frij_home(void);

// Re-evaluate the open app's header action symbol (show/hide) without a screen
// change — call when an app's action availability changes (e.g. Wi-Fi off).
void frij_launcher_refresh_action(void);

#endif  // FRIJ_LAUNCHER_H
