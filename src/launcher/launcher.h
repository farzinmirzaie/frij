#ifndef FRIJ_LAUNCHER_H
#define FRIJ_LAUNCHER_H

/*
 * The launcher: a looping carousel of app glances, plus navigation.
 *
 *   swipe left/right : move between app glances (wraps)
 *   swipe up         : open the current app (its own screen carousel)
 *   swipe down       : device settings
 *   Back (button)    : return to the launcher from an app or settings
 *
 * Call frij_launcher_start() once, after apps are registered.
 */
void frij_launcher_start(void);

// Return to the launcher from an app/settings layer. Called by the input layer
// (a hardware button on device, a key in the emulator). No-op on the launcher.
void frij_back(void);

#endif  // FRIJ_LAUNCHER_H
