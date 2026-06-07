#ifndef FRIJ_CAROUSEL_H
#define FRIJ_CAROUSEL_H

#include "lvgl.h"

/*
 * A horizontal, looping pager. Shows one page at a time; next()/prev() wrap
 * around. The page content is (re)built by `builder(page, index, user)` each
 * time the index changes.
 *
 * The carousel does not handle gestures itself — the launcher detects swipes
 * and calls next()/prev(). This keeps gesture meaning (which differs per
 * layer) in one place.
 */
typedef void (*frij_page_builder)(lv_obj_t* page, int index, void* user);

typedef struct {
    lv_obj_t*         page;
    int               count;
    int               index;
    frij_page_builder builder;
    void*             user;
} frij_carousel_t;

// Create a carousel filling `parent` and build page 0.
void frij_carousel_init(frij_carousel_t* c, lv_obj_t* parent, int count,
                        frij_page_builder builder, void* user);

void frij_carousel_next(frij_carousel_t* c);  // +1, wraps
void frij_carousel_prev(frij_carousel_t* c);  // -1, wraps
int  frij_carousel_index(const frij_carousel_t* c);

#endif  // FRIJ_CAROUSEL_H
