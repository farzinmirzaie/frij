#include "carousel.h"

#include <stdlib.h>  // abs

#include "theme.h"

static const int SNAP_PERCENT = 35;   // drag past this % of width to commit
static const int ANIM_MS      = 160;
static const int DOT          = 6;    // inactive dot diameter
static const int DOT_ACTIVE_W = 16;   // active dot width (pill)

static int wrap(int i, int n)
{
    return ((i % n) + n) % n;
}

// ---- page indicator (fades in on swipe, idles out; kept above the pages) ---

static void refresh_dots(frij_carousel_t* c)
{
    if (!c->dots) {
        return;
    }
    for (int i = 0; i < c->count; i++) {
        lv_obj_t* d      = lv_obj_get_child(c->dots, i);
        bool      active = (i == c->index);
        lv_obj_set_width(d, active ? DOT_ACTIVE_W : DOT);
        lv_obj_set_style_bg_color(d, lv_color_hex(active ? c->accent : FRIJ_TEXT_2), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(d, active ? LV_OPA_COVER : LV_OPA_40, LV_PART_MAIN);
    }
}

static void dots_to_front(frij_carousel_t* c)
{
    if (c->dots) {
        lv_obj_move_foreground(c->dots);  // stay above any newly built page
    }
}

static void set_opa(void* obj, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, LV_PART_MAIN);
}

static void fade_dots(frij_carousel_t* c, lv_opa_t to)
{
    if (!c->dots) {
        return;
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, c->dots);
    lv_anim_set_exec_cb(&a, set_opa);
    lv_anim_set_values(&a, lv_obj_get_style_opa(c->dots, LV_PART_MAIN), to);
    lv_anim_set_duration(&a, 220);
    lv_anim_start(&a);
}

static void dots_hide_cb(lv_timer_t* t)
{
    fade_dots((frij_carousel_t*)lv_timer_get_user_data(t), LV_OPA_TRANSP);
    lv_timer_pause(t);
}

// Show the dots (fade in) and restart the idle-hide timer.
static void dots_show(frij_carousel_t* c)
{
    if (!c->dots) {
        return;
    }
    dots_to_front(c);
    fade_dots(c, LV_OPA_COVER);
    lv_timer_reset(c->hide_timer);
    lv_timer_resume(c->hide_timer);
}

static void on_viewport_delete(lv_event_t* e)
{
    frij_carousel_t* c = (frij_carousel_t*)lv_event_get_user_data(e);
    if (c->hide_timer) {
        lv_timer_delete(c->hide_timer);
        c->hide_timer = NULL;
    }
    c->dots = NULL;
}

// Tap a dot to jump straight to that page.
static void dot_click_cb(lv_event_t* e)
{
    frij_carousel_t* c = (frij_carousel_t*)lv_event_get_user_data(e);
    lv_obj_t*        d = (lv_obj_t*)lv_event_get_target(e);
    frij_carousel_goto(c, (int)(intptr_t)lv_obj_get_user_data(d));
}

static void make_dots(frij_carousel_t* c)
{
    c->dots = lv_obj_create(c->viewport);
    lv_obj_remove_style_all(c->dots);
    lv_obj_set_size(c->dots, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(c->dots, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_set_flex_flow(c->dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(c->dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(c->dots, 6, LV_PART_MAIN);
    lv_obj_clear_flag(c->dots, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(c->dots, LV_OBJ_FLAG_EVENT_BUBBLE);

    for (int i = 0; i < c->count; i++) {
        lv_obj_t* d = lv_obj_create(c->dots);
        lv_obj_remove_style_all(d);
        lv_obj_set_size(d, DOT, DOT);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        // tap a dot to jump to its page (dots are tiny, so grow the hit area)
        lv_obj_add_flag(d, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(d, 10);
        lv_obj_set_user_data(d, (void*)(intptr_t)i);
        lv_obj_add_event_cb(d, dot_click_cb, LV_EVENT_CLICKED, c);
    }
    lv_obj_set_style_opa(c->dots, LV_OPA_TRANSP, LV_PART_MAIN);  // hidden until a swipe
    refresh_dots(c);
}

// ---- pages ----------------------------------------------------------------

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
    dots_to_front(c);  // a fresh page is on top — push the dots back above it
    return p;
}

static void notify(frij_carousel_t* c)
{
    if (c->on_change) {
        c->on_change(c->index, c->change_user);
    }
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
    refresh_dots(c);
    dots_show(c);
    notify(c);
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

// ---- public ----------------------------------------------------------------

void frij_carousel_init(frij_carousel_t* c, lv_obj_t* parent, int count,
                        frij_page_builder builder, void* user, uint32_t accent)
{
    c->count       = count < 1 ? 1 : count;
    c->index       = 0;
    c->adj_index   = 0;
    c->dir_sign    = 0;
    c->busy        = false;
    c->builder     = builder;
    c->user        = user;
    c->adj         = NULL;
    c->dots        = NULL;
    c->hide_timer  = NULL;
    c->accent      = accent;
    c->on_change   = NULL;
    c->change_user = NULL;

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

    if (c->count > 1) {
        make_dots(c);
        c->hide_timer = lv_timer_create(dots_hide_cb, 1400, c);
        lv_timer_pause(c->hide_timer);
        lv_obj_add_event_cb(c->viewport, on_viewport_delete, LV_EVENT_DELETE, c);
    }
}

void frij_carousel_set_change_cb(frij_carousel_t* c, void (*cb)(int, void*), void* user)
{
    c->on_change   = cb;
    c->change_user = user;
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
    dots_show(c);
}

void frij_carousel_end(frij_carousel_t* c, int dx)
{
    dots_show(c);  // keep the dots up through the snap, then they idle out
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

void frij_carousel_goto(frij_carousel_t* c, int index)
{
    if (c->busy) {
        return;
    }
    if (c->adj) {
        lv_obj_delete(c->adj);
        c->adj = NULL;
    }
    c->index = wrap(index, c->count);
    lv_obj_set_x(c->cur, 0);
    build_into(c, c->cur, c->index);
    refresh_dots(c);
    dots_show(c);
    notify(c);
}

int frij_carousel_index(const frij_carousel_t* c)
{
    return c->index;
}
