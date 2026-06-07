#ifndef FRIJ_LAUNCHER_H
#define FRIJ_LAUNCHER_H

/*
 * The launcher: Frij's home screen.
 *
 * It shows one tile per registered app. Tapping a tile opens that app on a
 * full-screen page with a launcher-owned "Back" button. The apps themselves
 * stay unaware of all this.
 *
 * Call frij_launcher_start() once, after apps are registered.
 */
void frij_launcher_start(void);

#endif  // FRIJ_LAUNCHER_H
