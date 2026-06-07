#include "carousel.h"

#include <stdlib.h>  // abs

// --- small helpers ---------------------------------------------------------

static int wrap(int i, int n)
{
    return ((i % n) + n) % n;
}

static void build_into(frij_carousel_t* c, lv_obj_t* page, int index)
{
    lv_obj_clean(page);
    c->builder(page, index, c->user);
}

static lv_obj_t* make_page(frij_carousel_t* c)
{
    lv_obj_t* p = lv_obj_create(c->viewport);
    lv_obj_set_size(p, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(p, 0, 0);
    lv_obj_set_style_border_width(p, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(p, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(p, 0, LV_PART_MAIN);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(p, LV_OBJ_FLAG_EVENT_BUBBLE);  // press/drag bubbles to viewport
    return p;
}

static void anim_x(void* obj, int32_t v)
{
    lv_obj_set_x((lv_obj_t*)obj, (lv_coord_t)v);
}

static void slide(frij_carousel_t* c, lv_obj_t* obj, int to, lv_anim_completed_cb_t done)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, lv_obj_get_x(obj), to);
    lv_anim_set_duration(&a, 160);
    lv_anim_set_exec_cb(&a, anim_x);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    if (done) {
        lv_anim_set_completed_cb(&a, done);
        a.user_data = c;
    }
    lv_anim_start(&a);
}

// --- snap-animation completions -------------------------------------------

static void commit_done(lv_anim_t* a)
{
    frij_carousel_t* c = (frij_carousel_t*)a->user_data;
    lv_obj_delete(c->cur);
    c->cur   = c->adj;
    c->adj   = NULL;
    c->index = c->adj_index;
    c->busy  = false;
}

static void revert_done(lv_anim_t* a)
{
    frij_carousel_t* c = (frij_carousel_t*)a->user_data;
    if (c->adj) {
        lv_obj_delete(c->adj);
        c->adj = NULL;
    }
    c->busy = false;
}

// --- input -----------------------------------------------------------------

static void ensure_neighbor(frij_carousel_t* c, int sign)
{
    if (c->adj && c->dir_sign == sign) {
        return;
    }
    if (c->adj) {
        lv_obj_delete(c->adj);
        c->adj = NULL;
    }
    c->dir_sign  = sign;
    c->adj_index = wrap(c->index + (sign < 0 ? +1 : -1), c->count);
    c->adj       = make_page(c);
    build_into(c, c->adj, c->adj_index);
}

static void on_event(lv_event_t* e)
{
    frij_carousel_t* c = (frij_carousel_t*)lv_event_get_user_data(e);
    lv_event_code_t  code = lv_event_get_code(e);
    lv_indev_t*      indev = lv_indev_active();
    if (c->busy || indev == NULL) {
        return;
    }

    if (code == LV_EVENT_PRESSED) {
        lv_indev_get_point(indev, &c->start);
        c->axis = 0;
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        lv_point_t p;
        lv_indev_get_point(indev, &p);
        int dx = p.x - c->start.x;
        int dy = p.y - c->start.y;

        if (c->axis == 0 && (abs(dx) > 8 || abs(dy) > 8)) {
            c->axis = (abs(dx) >= abs(dy)) ? 1 : 2;
        }
        if (c->axis == 1) {
            int w = lv_obj_get_width(c->viewport);
            ensure_neighbor(c, dx < 0 ? -1 : +1);
            lv_obj_set_x(c->cur, dx);
            lv_obj_set_x(c->adj, (c->dir_sign < 0 ? w : -w) + dx);
        }
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        lv_point_t p;
        lv_indev_get_point(indev, &p);

        if (c->axis == 1 && c->adj) {
            int w  = lv_obj_get_width(c->viewport);
            int dx = p.x - c->start.x;
            c->busy = true;
            if (abs(dx) > w * 35 / 100) {            // commit the swipe
                slide(c, c->cur, c->dir_sign < 0 ? -w : w, NULL);
                slide(c, c->adj, 0, commit_done);
            } else {                                 // not far enough: revert
                slide(c, c->adj, c->dir_sign < 0 ? w : -w, revert_done);
                slide(c, c->cur, 0, NULL);
            }
        } else if (c->axis == 2 && c->vswipe) {
            int dy = p.y - c->start.y;
            if (abs(dy) > 40) {
                c->vswipe(dy < 0 ? LV_DIR_TOP : LV_DIR_BOTTOM, c->user);
            }
        }
        c->axis = 0;
    }
}

// --- public ----------------------------------------------------------------

void frij_carousel_init(frij_carousel_t* c, lv_obj_t* parent, int count,
                        frij_page_builder builder, frij_vswipe_cb vswipe, void* user)
{
    c->count     = count < 1 ? 1 : count;
    c->index     = 0;
    c->adj_index = 0;
    c->builder   = builder;
    c->vswipe    = vswipe;
    c->user      = user;
    c->adj       = NULL;
    c->axis      = 0;
    c->dir_sign  = 0;
    c->busy      = false;

    c->viewport = lv_obj_create(parent);
    lv_obj_set_size(c->viewport, LV_PCT(100), LV_PCT(100));
    lv_obj_center(c->viewport);
    lv_obj_set_style_bg_opa(c->viewport, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(c->viewport, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(c->viewport, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(c->viewport, 0, LV_PART_MAIN);
    lv_obj_clear_flag(c->viewport, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(c->viewport, on_event, LV_EVENT_ALL, c);

    c->cur = make_page(c);
    build_into(c, c->cur, 0);
}

int frij_carousel_index(const frij_carousel_t* c)
{
    return c->index;
}
