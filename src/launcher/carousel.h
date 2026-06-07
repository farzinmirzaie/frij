#ifndef FRIJ_CAROUSEL_H
#define FRIJ_CAROUSEL_H

#include "lvgl.h"

/*
 * A horizontal, looping, drag-following pager.
 *
 * The page content is built by `builder(page, index, user)`. While the user
 * drags horizontally the current and incoming pages follow the finger; on
 * release the carousel snaps to whichever is mostly in view (wrapping at the
 * ends).
 *
 * A vertical drag is reported to `vswipe(dir, user)` on release (LV_DIR_TOP /
 * LV_DIR_BOTTOM) — the launcher uses this for open / settings. `vswipe` may be
 * NULL (e.g. inside an app, where vertical does nothing).
 */
typedef void (*frij_page_builder)(lv_obj_t* page, int index, void* user);
typedef void (*frij_vswipe_cb)(lv_dir_t dir, void* user);

typedef struct {
    lv_obj_t*         viewport;   // clips the pages
    lv_obj_t*         cur;        // page currently centered
    lv_obj_t*         adj;        // incoming neighbor during a drag (or NULL)

    int               count;
    int               index;
    int               adj_index;  // index the neighbor was built for
    frij_page_builder builder;
    frij_vswipe_cb    vswipe;
    void*             user;

    lv_point_t        start;      // press origin
    int               axis;       // 0 undecided, 1 horizontal, 2 vertical
    int               dir_sign;   // -1 dragging left (next), +1 right (prev)
    bool              busy;       // a snap animation is running
} frij_carousel_t;

void frij_carousel_init(frij_carousel_t* c, lv_obj_t* parent, int count,
                        frij_page_builder builder, frij_vswipe_cb vswipe, void* user);

int frij_carousel_index(const frij_carousel_t* c);

#endif  // FRIJ_CAROUSEL_H
