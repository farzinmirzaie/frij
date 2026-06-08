#ifndef FRIJ_APP_H
#define FRIJ_APP_H

#include "lvgl.h"

/*
 * The contract between a mini-app and Frij.
 *
 * An app knows ONLY this header — not the launcher, the registry, or other
 * apps. It declares two things:
 *
 *   build_glance : fills a full-screen card shown in the launcher carousel.
 *                  Informational, minimal interactivity.
 *   build_screen : fills screen `index` of the app's own left/right carousel,
 *                  opened when the user swipes up on the glance. Interactive.
 *
 * The launcher provides the carousel, gestures, and the Back button; an app
 * just fills the container it is handed.
 */
typedef struct {
    const char* name;
    uint32_t    color;                            // the app's accent (0xRRGGBB),
                                                  // picked from ui/theme.h; the
                                                  // page background stays Surface-1

    void (*build_glance)(lv_obj_t* parent);

    int  screen_count;                            // app's own screens (>= 1)
    void (*build_screen)(lv_obj_t* parent, int index);
} frij_app_t;

#endif  // FRIJ_APP_H
