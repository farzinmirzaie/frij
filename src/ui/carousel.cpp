#include "carousel.h"

#include <stdlib.h>  // abs

#include "anim.h"
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

static void dot_width_exec(void* obj, int32_t v)
{
    lv_obj_set_width((lv_obj_t*)obj, (lv_coord_t)v);
}

static void refresh_dots(frij_carousel_t* c)
{
    if (!c->dots) {
        return;
    }
    for (int i = 0; i < c->count; i++) {
        lv_obj_t* d      = lv_obj_get_child(c->dots, i);
        bool      active = (i == c->index);
        int       target = active ? DOT_ACTIVE_W : DOT;
        int       cur    = lv_obj_get_width(d);
        if (frij_anim_enabled() && cur != target && cur > 0) {
            lv_anim_t a;  // morph the dot into/out of the pill instead of snapping
            lv_anim_init(&a);
            lv_anim_set_var(&a, d);
            lv_anim_set_exec_cb(&a, dot_width_exec);
            lv_anim_set_values(&a, cur, target);
            lv_anim_set_duration(&a, 160);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
            lv_anim_start(&a);
        } else {
            lv_obj_set_width(d, target);
        }
        lv_obj_set_style_bg_color(d, lv_color_hex(active ? c->accent : FRIJ_TEXT_2), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(d, active ? LV_OPA_COVER : LV_OPA_40, LV_PART_MAIN);
    }
}

// During a drag, hand the active pill over to the target dot proportionally —
// width, color and opacity all track the finger (`p` = 0..256 progress).
static void dots_drag_fx(frij_carousel_t* c, int32_t p)
{
    if (!c->dots || !frij_anim_enabled()) {
        return;
    }
    lv_obj_t* cur = lv_obj_get_child(c->dots, c->index);
    lv_obj_t* tgt = lv_obj_get_child(c->dots, c->adj_index);
    if (cur == NULL || tgt == NULL || cur == tgt) {
        return;
    }
    if (p < 0) p = 0;
    if (p > 256) p = 256;
    lv_anim_delete(cur, dot_width_exec);  // don't fight a snap morph
    lv_anim_delete(tgt, dot_width_exec);
    lv_obj_set_width(cur, DOT_ACTIVE_W - (DOT_ACTIVE_W - DOT) * p / 256);
    lv_obj_set_width(tgt, DOT + (DOT_ACTIVE_W - DOT) * p / 256);
    uint8_t  mix    = (uint8_t)(255 * p / 256);
    lv_color_t on   = lv_color_hex(c->accent);
    lv_color_t off  = lv_color_hex(FRIJ_TEXT_2);
    lv_obj_set_style_bg_color(cur, lv_color_mix(off, on, mix), LV_PART_MAIN);
    lv_obj_set_style_bg_color(tgt, lv_color_mix(on, off, mix), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(cur, 255 - (255 - LV_OPA_40) * p / 256, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tgt, LV_OPA_40 + (255 - LV_OPA_40) * p / 256, LV_PART_MAIN);
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

// Transition FX: pages zoom + fade slightly as they leave/enter, tied to the
// drag. `p` is "outness": 0 = fully in (native), 256 = fully out.
#define FX_SCALE_MIN 220  // ~0.86 at fully-out
#define FX_OPA_MIN   90   // ~35% opacity at fully-out

static void page_fx(lv_obj_t* page, int32_t p)
{
    if (page == NULL || !frij_anim_enabled()) {
        return;
    }
    if (p < 0) p = 0;
    if (p > 256) p = 256;
    lv_obj_set_style_transform_scale(page, 256 - (256 - FX_SCALE_MIN) * p / 256, LV_PART_MAIN);
    lv_obj_set_style_opa(page, 255 - (255 - FX_OPA_MIN) * p / 256, LV_PART_MAIN);
}

static void page_fx_exec(void* page, int32_t p)
{
    page_fx((lv_obj_t*)page, p);
}

// Animate a page's outness `from` → `to` alongside the snap slide.
static void page_fx_to(lv_obj_t* page, int32_t from, int32_t to)
{
    if (page == NULL || !frij_anim_enabled()) {
        return;
    }
    lv_anim_delete(page, page_fx_exec);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, page);
    lv_anim_set_exec_cb(&a, page_fx_exec);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, ANIM_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
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
    lv_obj_set_style_transform_pivot_x(p, lv_pct(50), LV_PART_MAIN);  // zoom from center
    lv_obj_set_style_transform_pivot_y(p, lv_pct(50), LV_PART_MAIN);
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
    page_fx(c->cur, 0);  // make sure the settled page is at native scale/opa
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
    page_fx(c->cur, 0);
    refresh_dots(c);  // the drag left the dots mid-handover — settle them back
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
    // zoom/fade follows the finger: the leaving page recedes, the entering one grows
    int p = 256 * abs(dx) / (w > 0 ? w : 1);
    page_fx(c->cur, p);
    page_fx(c->adj, 256 - p);
    dots_drag_fx(c, p);  // the indicator pill hands over with the drag too
    dots_show(c);
}

void frij_carousel_end(frij_carousel_t* c, int dx)
{
    dots_show(c);  // keep the dots up through the snap, then they idle out
    if (c->busy || c->adj == NULL) {
        return;
    }
    int w   = lv_obj_get_width(c->viewport);
    int p   = 256 * abs(dx) / (w > 0 ? w : 1);
    if (p > 256) p = 256;
    c->busy = true;
    if (abs(dx) > w * SNAP_PERCENT / 100) {
        slide(c, c->cur, c->dir_sign < 0 ? -w : w, NULL);
        slide(c, c->adj, 0, commit_done);
        page_fx_to(c->cur, p, 256);  // finish receding
        page_fx_to(c->adj, 256 - p, 0);
    } else {
        slide(c, c->adj, c->dir_sign < 0 ? w : -w, revert_done);
        slide(c, c->cur, 0, NULL);
        page_fx_to(c->cur, p, 0);  // come back to native
        page_fx_to(c->adj, 256 - p, 256);
    }
}

void frij_carousel_goto(frij_carousel_t* c, int index)
{
    if (c->busy) {
        return;
    }
    index = wrap(index, c->count);
    if (index == c->index) {
        dots_show(c);
        return;
    }
    if (c->adj) {
        lv_obj_delete(c->adj);
        c->adj = NULL;
    }
    if (!frij_anim_enabled()) {  // reduce-motion: jump in place
        c->index = index;
        lv_obj_set_x(c->cur, 0);
        build_into(c, c->cur, c->index);
        refresh_dots(c);
        dots_show(c);
        notify(c);
        return;
    }
    // slide toward the target like a swipe would (forward = new page from the right)
    int w        = lv_obj_get_width(c->viewport);
    int sign     = index > c->index ? -1 : +1;
    c->dir_sign  = sign;
    c->adj_index = index;
    c->adj       = make_page(c);
    build_into(c, c->adj, index);
    lv_obj_set_x(c->adj, sign < 0 ? w : -w);
    page_fx(c->adj, 256);  // enters from fully-out
    c->busy = true;
    slide(c, c->cur, sign < 0 ? -w : w, NULL);
    slide(c, c->adj, 0, commit_done);  // commit_done updates index/dots/notify
    page_fx_to(c->cur, 0, 256);
    page_fx_to(c->adj, 256, 0);
}

int frij_carousel_index(const frij_carousel_t* c)
{
    return c->index;
}
