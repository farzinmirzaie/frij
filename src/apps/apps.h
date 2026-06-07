#ifndef FRIJ_APPS_H
#define FRIJ_APPS_H

/*
 * The ONE place where apps are wired into the launcher.
 *
 * To add a new app: build it under src/apps/<name>/, then add one line to
 * frij_register_apps() in apps.cpp. Nothing else in the launcher changes.
 */
void frij_register_apps(void);

#endif  // FRIJ_APPS_H
