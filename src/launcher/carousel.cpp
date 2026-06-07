#include "carousel.h"

#include <stdlib.h>  // abs

static const int SNAP_PERCENT = 35;  // drag past this % of width to commit
static const int ANIM_MS      = 160;

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
    lv_obj_add_flag(p, LV_OBJ_FLAG_EVENT_BUBBLE);
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
    lv_anim_set_duration(&a, ANIM_MS);
    lv_anim_set_exec_cb(&a, anim_x);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    if (done) {
        lv_anim_set_completed_cb(&a, done);
        a.user_data = c;
    }
    lv_anim_start(&a);
}

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

void frij_carousel_init(frij_carousel_t* c, lv_obj_t* parent, int count,
                        frij_page_builder builder, void* user)
{
    c->count     = count < 1 ? 1 : count;
    c->index     = 0;
    c->adj_index = 0;
    c->dir_sign  = 0;
    c->busy      = false;
    c->builder   = builder;
    c->user      = user;
    c->adj       = NULL;

    c->viewport = lv_obj_create(parent);
    lv_obj_set_size(c->viewport, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(c->viewport, 0, 0);
    lv_obj_set_style_bg_opa(c->viewport, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(c->viewport, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(c->viewport, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(c->viewport, 0, LV_PART_MAIN);
    lv_obj_clear_flag(c->viewport, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(c->viewport, LV_OBJ_FLAG_EVENT_BUBBLE);

    c->cur = make_page(c);
    build_into(c, c->cur, 0);
}

void frij_carousel_drag(frij_carousel_t* c, int dx)
{
    if (c->count <= 1 || c->busy || dx == 0) {
        return;
    }
    int w    = lv_obj_get_width(c->viewport);
    int sign = dx < 0 ? -1 : +1;
    ensure_neighbor(c, sign);
    lv_obj_set_x(c->cur, dx);
    lv_obj_set_x(c->adj, (sign < 0 ? w : -w) + dx);
}

void frij_carousel_end(frij_carousel_t* c, int dx)
{
    if (c->busy || c->adj == NULL) {
        return;
    }
    int w   = lv_obj_get_width(c->viewport);
    c->busy = true;
    if (abs(dx) > w * SNAP_PERCENT / 100) {
        slide(c, c->cur, c->dir_sign < 0 ? -w : w, NULL);
        slide(c, c->adj, 0, commit_done);
    } else {
        slide(c, c->adj, c->dir_sign < 0 ? w : -w, revert_done);
        slide(c, c->cur, 0, NULL);
    }
}

int frij_carousel_index(const frij_carousel_t* c)
{
    return c->index;
}
