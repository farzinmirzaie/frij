#ifndef FRIJ_APP_H
#define FRIJ_APP_H

#include "lvgl.h"

/*
 * The contract between a mini-app and Frij.
 *
 * A mini-app knows ONLY this header. It does not know about the launcher,
 * the registry, or other apps. It just describes itself and provides one
 * function that builds its UI inside a parent container it is handed.
 *
 * Navigation (opening, the back button) is the launcher's job — an app never
 * has to think about it.
 */
typedef struct {
    const char* name;   // shown on the launcher tile
    uint32_t    color;  // tile color, 0xRRGGBB

    // Build the app's UI inside `parent`. Called each time the app is opened.
    void (*open)(lv_obj_t* parent);
} frij_app_t;

#endif  // FRIJ_APP_H
