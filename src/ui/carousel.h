#ifndef FRIJ_CAROUSEL_H
#define FRIJ_CAROUSEL_H

#include "lvgl.h"

/*
 * A horizontal, looping pager that follows the finger.
 *
 * It does NOT read input itself — the launcher owns one input handler (so it
 * can decide horizontal-vs-vertical) and drives the carousel:
 *   drag(dx) during the gesture, end(dx) on release.
 *
 * With count <= 1 there is nothing to page, so drag/end do nothing (no loop).
 */
typedef void (*frij_page_builder)(lv_obj_t* page, int index, void* user);

typedef struct {
    lv_obj_t*         viewport;
    lv_obj_t*         cur;
    lv_obj_t*         adj;         // neighbor shown during a drag (or NULL)
    int               count;
    int               index;
    int               adj_index;
    int               dir_sign;    // -1 dragging left (next), +1 right (prev)
    bool              busy;        // snap animation running
    frij_page_builder builder;
    void*             user;
    lv_obj_t*         dots;        // page indicator (NULL when count <= 1)
    lv_timer_t*       hide_timer;  // idle timer that fades the dots out
    uint32_t          accent;      // active-dot color (0xRRGGBB)
} frij_carousel_t;

// `accent` colors the active page dot. The indicator auto-shows on a swipe and
// fades out when idle; it's omitted entirely when count <= 1.
void frij_carousel_init(frij_carousel_t* c, lv_obj_t* parent, int count,
                        frij_page_builder builder, void* user, uint32_t accent);

void frij_carousel_drag(frij_carousel_t* c, int dx);  // live horizontal offset
void frij_carousel_end(frij_carousel_t* c, int dx);   // release: commit or revert
int  frij_carousel_index(const frij_carousel_t* c);

#endif  // FRIJ_CAROUSEL_H
