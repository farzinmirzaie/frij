#include "components.h"

#include <stdint.h>

#include "anim.h"
#include "system/audio.h"
#include "system/haptics.h"
#include "theme.h"

static void on_press_haptic(lv_event_t* e)
{
    (void)e;
    frij_haptic(FRIJ_HAPTIC_TAP);
    frij_audio_click();  // touch sound (only if enabled in Settings)
}

// ---- helpers ---------------------------------------------------------------

static void strip(lv_obj_t* o)
{
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(o, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(o, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

// Apply a subtle top-to-bottom gradient between two colors to an object's MAIN.
static void grad_v(lv_obj_t* o, uint32_t top, uint32_t bottom)
{
    lv_obj_set_style_bg_color(o, lv_color_hex(top), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(o, lv_color_hex(bottom), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
}

// ---- public ---------------------------------------------------------------

int frij_screen_min(void)
{
    lv_display_t* d = lv_display_get_default();
    int32_t       w = lv_display_get_horizontal_resolution(d);
    int32_t       h = lv_display_get_vertical_resolution(d);
    return (int)(w < h ? w : h);
}

void frij_apply_bg(lv_obj_t* obj)
{
    // calm dark page wash, slightly darker toward the bottom
    grad_v(obj, FRIJ_SURFACE_1, 0x07070A);
}

void frij_haptic_attach(lv_obj_t* obj)
{
    lv_obj_add_event_cb(obj, on_press_haptic, LV_EVENT_PRESSED, NULL);
}

static void glow_free_cb(lv_event_t* e)
{
    lv_free(lv_event_get_user_data(e));  // the gradient descriptor outlives the call
}

lv_obj_t* frij_glow(lv_obj_t* parent, uint32_t accent)
{
    int       sz = frij_screen_min() * 80 / 100;
    lv_obj_t* g  = lv_obj_create(parent);
    lv_obj_remove_style_all(g);
    lv_obj_set_size(g, sz, sz);
    lv_obj_center(g);
    // FLOATING: fixed background that doesn't scroll or grow the scroll area
    lv_obj_add_flag(g, LV_OBJ_FLAG_FLOATING);
    lv_obj_clear_flag(g, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g, LV_OBJ_FLAG_SCROLLABLE);

    // true radial gradient: accent at the center fading to transparent at the edge
    lv_grad_dsc_t* d = (lv_grad_dsc_t*)lv_malloc(sizeof(lv_grad_dsc_t));
    if (d == NULL) {
        return g;  // OOM — a plain (gradient-less) object is fine
    }
    lv_memzero(d, sizeof(*d));
    lv_grad_radial_init(d, LV_GRAD_CENTER, LV_GRAD_CENTER, LV_GRAD_RIGHT, LV_GRAD_CENTER,
                        LV_GRAD_EXTEND_PAD);
    d->stops_count   = 2;
    d->stops[0].color = lv_color_hex(accent);
    d->stops[0].opa   = 120;
    d->stops[0].frac  = 0;
    d->stops[1].color = lv_color_hex(accent);
    d->stops[1].opa   = 0;
    d->stops[1].frac  = 255;

    lv_obj_set_style_bg_grad(g, d, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_event_cb(g, glow_free_cb, LV_EVENT_DELETE, d);
    return g;
}

lv_obj_t* frij_top_tint(lv_obj_t* parent, uint32_t accent)
{
    // A restrained accent wash along the top edge that fades to nothing — a
    // per-app color cue behind the header, without a glowing blob.
    lv_obj_t* g = lv_obj_create(parent);
    lv_obj_remove_style_all(g);
    lv_obj_set_size(g, LV_PCT(100), frij_screen_min() * 38 / 100);
    lv_obj_align(g, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_flag(g, LV_OBJ_FLAG_FLOATING);
    lv_obj_clear_flag(g, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g, LV_OBJ_FLAG_SCROLLABLE);

    // Vertical: fades up FROM the background at the very top edge, peaks behind
    // the title, then fades back to nothing — so it never hits the top harshly.
    lv_grad_dsc_t* d = (lv_grad_dsc_t*)lv_malloc(sizeof(lv_grad_dsc_t));
    if (d == NULL) {
        return g;  // OOM — skip the tint
    }
    lv_memzero(d, sizeof(*d));
    // Top→bottom (frac 0 = top): the upper 3/4 is fully transparent (shows the
    // dark background), then a single accent band low down, fading out at the
    // very bottom. Keeps the top edge dark and the cue subtle, behind the title.
    d->dir            = LV_GRAD_DIR_VER;
    d->stops_count    = 5;
    d->stops[0].color = lv_color_hex(accent);
    d->stops[0].opa   = 0;
    d->stops[0].frac  = 0;
    d->stops[1].color = lv_color_hex(accent);
    d->stops[1].opa   = 10;
    d->stops[1].frac  = 60;
    d->stops[2].color = lv_color_hex(accent);
    d->stops[2].opa   = 80;
    d->stops[2].frac  = 150;
    d->stops[3].color = lv_color_hex(accent);
    d->stops[3].opa   = 100;  // the accent band, low in the rect
    d->stops[3].frac  = 200;
    d->stops[4].color = lv_color_hex(accent);
    d->stops[4].opa   = 0;
    d->stops[4].frac  = 255;

    lv_obj_set_style_bg_grad(g, d, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_event_cb(g, glow_free_cb, LV_EVENT_DELETE, d);
    return g;
}

lv_obj_t* frij_col(lv_obj_t* parent, int gap)
{
    lv_obj_t* c = lv_obj_create(parent);
    lv_obj_set_size(c, LV_PCT(100), LV_SIZE_CONTENT);
    strip(c);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(c, gap, LV_PART_MAIN);
    return c;
}

lv_obj_t* frij_page(lv_obj_t* parent)
{
    // Configure the page itself as a centered, vertically-scrollable column.
    // Short content sits centered; taller content scrolls (the launcher turns a
    // swipe past the scroll edge into Back).
    int hpad = frij_screen_min() * 8 / 100;
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(parent, FRIJ_SP_S, LV_PART_MAIN);
    lv_obj_set_style_pad_left(parent, hpad, LV_PART_MAIN);
    lv_obj_set_style_pad_right(parent, hpad, LV_PART_MAIN);
    lv_obj_set_style_pad_top(parent, FRIJ_SP_L, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(parent, FRIJ_SP_XL, LV_PART_MAIN);
    lv_obj_add_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(parent, LV_DIR_VER);
    lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLL_ELASTIC);  // firm edges for the back gesture
    lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);
    return parent;
}

// Full-bleed pages (e.g. the Scoreboard's split) own the whole area below the
// header and want NO safe-area inset or auto-centering. Reuse a spare flag.
#define FRIJ_FLAG_FULL_BLEED LV_OBJ_FLAG_USER_2

void frij_page_full_bleed(lv_obj_t* page)
{
    lv_obj_add_flag(page, FRIJ_FLAG_FULL_BLEED);
}

void frij_page_under_header(lv_obj_t* page, int header_px)
{
    if (lv_obj_has_flag(page, FRIJ_FLAG_FULL_BLEED)) {
        return;  // full-bleed: no safe-area padding
    }
    // The page lives in the area below the header. Add breathing room under the
    // bar, then a matching bottom inset so the *centered* content still lands at
    // the screen's true middle (not the middle of the shorter area). Reusable
    // "safe area" for any screen that sits beneath a top bar.
    int pad_top = lv_obj_get_style_pad_top(page, LV_PART_MAIN) + FRIJ_SP_M;
    lv_obj_set_style_pad_top(page, pad_top, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(page, pad_top + header_px, LV_PART_MAIN);
}

// Pages can opt out of auto-centering (e.g. a list whose first row must stay put
// whether it has 1 or 10 items). We reuse a spare LVGL object flag for the mark.
#define FRIJ_FLAG_PIN_TOP LV_OBJ_FLAG_USER_1

void frij_page_pin_top(lv_obj_t* page)
{
    lv_obj_add_flag(page, FRIJ_FLAG_PIN_TOP);
}

void frij_page_settle(lv_obj_t* page)
{
    if (lv_obj_has_flag(page, FRIJ_FLAG_FULL_BLEED)) {
        return;  // full-bleed owns its own layout
    }
    // Pinned pages always stay top-aligned (content doesn't jump when it's short).
    if (lv_obj_has_flag(page, FRIJ_FLAG_PIN_TOP)) {
        lv_obj_set_flex_align(page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_scroll_to_y(page, 0, LV_ANIM_OFF);
        return;
    }
    // Otherwise center the content when it fits; top-align when it overflows so
    // the first row stays visible (a centered overflowing list hides its top).
    lv_obj_set_flex_align(page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_update_layout(page);
    if (lv_obj_get_scroll_bottom(page) <= 0) {
        lv_obj_set_flex_align(page, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    } else {
        lv_obj_scroll_to_y(page, 0, LV_ANIM_OFF);  // start at the top
    }
}

lv_obj_t* frij_label(lv_obj_t* parent, const char* text, const lv_font_t* font, uint32_t color)
{
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN);
    return l;
}

lv_obj_t* frij_surface_row(lv_obj_t* parent)
{
    static lv_style_prop_t  tr_props[] = {LV_STYLE_BG_COLOR, LV_STYLE_PROP_INV};
    static lv_style_transition_dsc_t tr;
    static bool tr_ready = false;
    if (!tr_ready) {
        lv_style_transition_dsc_init(&tr, tr_props, lv_anim_path_ease_out, 140, 0, NULL);
        tr_ready = true;
    }

    lv_obj_t* r = lv_obj_create(parent);
    lv_obj_set_width(r, LV_PCT(100));
    lv_obj_set_height(r, FRIJ_ROW_H);  // fixed: uniform height + content stays vertically centered
    grad_v(r, FRIJ_SURFACE_2, 0x101216);  // subtle depth
    lv_obj_set_style_radius(r, FRIJ_RADIUS_M, LV_PART_MAIN);
    lv_obj_set_style_border_width(r, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_top(r, FRIJ_SP_M, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(r, FRIJ_SP_M, LV_PART_MAIN);
    lv_obj_set_style_pad_left(r, FRIJ_SP_M, LV_PART_MAIN);
    lv_obj_set_style_pad_right(r, FRIJ_SP_M, LV_PART_MAIN);
    lv_obj_set_style_pad_column(r, FRIJ_SP_M, LV_PART_MAIN);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // press feedback: lift to Surface-3 with a quick fade
    lv_obj_set_style_bg_color(r, lv_color_hex(FRIJ_SURFACE_3), LV_STATE_PRESSED);
    lv_obj_set_style_transition(r, &tr, LV_PART_MAIN);
    frij_haptic_attach(r);
    return r;
}

lv_obj_t* frij_check(lv_obj_t* parent, bool checked, uint32_t accent)
{
    const int sz = 28;
    lv_obj_t* c  = lv_obj_create(parent);
    lv_obj_set_size(c, sz, sz);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(c, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(c, 0, LV_PART_MAIN);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_CLICKABLE);  // the row handles taps
    lv_obj_set_style_transform_pivot_x(c, sz / 2, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(c, sz / 2, LV_PART_MAIN);
    lv_obj_set_user_data(c, (void*)(intptr_t)accent);

    lv_obj_t* tick = lv_label_create(c);
    lv_label_set_text(tick, LV_SYMBOL_OK);
    lv_obj_set_style_text_font(tick, FRIJ_FONT_SYMBOL, LV_PART_MAIN);
    lv_obj_set_style_text_color(tick, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(tick);

    frij_check_set(c, checked, false);
    return c;
}

void frij_check_set(lv_obj_t* check, bool checked, bool animate)
{
    uint32_t  accent = (uint32_t)(intptr_t)lv_obj_get_user_data(check);
    lv_obj_t* tick   = lv_obj_get_child(check, 0);

    if (checked) {
        lv_obj_set_style_bg_color(check, lv_color_hex(accent), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(check, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_color(check, lv_color_hex(accent), LV_PART_MAIN);
        if (tick) {
            lv_obj_set_style_opa(tick, LV_OPA_COVER, LV_PART_MAIN);
        }
    } else {
        lv_obj_set_style_bg_opa(check, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_color(check, lv_color_hex(FRIJ_BORDER), LV_PART_MAIN);
        if (tick) {
            lv_obj_set_style_opa(tick, LV_OPA_TRANSP, LV_PART_MAIN);
        }
    }

    if (animate) {
        if (frij_anim_enabled()) {
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, check);
            lv_anim_set_exec_cb(&a, frij_anim_exec_scale);
            lv_anim_set_values(&a, 210, 256);  // 256 = 100%
            lv_anim_set_duration(&a, 180);
            lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
            lv_anim_start(&a);
        }
        frij_haptic(FRIJ_HAPTIC_SUCCESS);  // haptic regardless of reduce-motion
    }
}

lv_obj_t* frij_progress_ring(lv_obj_t* parent, int size, int pct, uint32_t accent)
{
    lv_obj_t* a = lv_arc_create(parent);
    lv_obj_set_size(a, size, size);
    lv_arc_set_rotation(a, 270);
    lv_arc_set_bg_angles(a, 0, 360);
    lv_arc_set_range(a, 0, 100);
    lv_arc_set_value(a, pct);
    lv_obj_remove_style(a, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(a, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_width(a, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(a, lv_color_hex(FRIJ_SURFACE_3), LV_PART_MAIN);
    lv_obj_set_style_arc_color(a, lv_color_hex(accent), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(a, true, LV_PART_INDICATOR);
    return a;
}

lv_obj_t* frij_empty_state(lv_obj_t* parent, const char* text)
{
    lv_obj_t* box = frij_col(parent, FRIJ_SP_M);

    lv_obj_t* circle = lv_obj_create(box);
    lv_obj_set_size(circle, 72, 72);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(circle, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(circle, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(circle, lv_color_hex(FRIJ_BORDER), LV_PART_MAIN);
    lv_obj_clear_flag(circle, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* icon = lv_label_create(circle);
    lv_label_set_text(icon, LV_SYMBOL_LIST);  // neutral — not "+" (which implies add)
    lv_obj_set_style_text_font(icon, FRIJ_FONT_SYMBOL, LV_PART_MAIN);
    lv_obj_set_style_text_color(icon, lv_color_hex(FRIJ_TEXT_2), LV_PART_MAIN);
    lv_obj_center(icon);

    frij_label(box, text, FRIJ_FONT_BODY, FRIJ_TEXT_2);
    return box;
}

lv_obj_t* frij_swipe_hint(lv_obj_t* parent)
{
    lv_obj_t* hint = lv_label_create(parent);
    lv_label_set_text(hint, LV_SYMBOL_UP);
    lv_obj_set_style_text_font(hint, FRIJ_FONT_SYMBOL, LV_PART_MAIN);
    lv_obj_set_style_text_color(hint, lv_color_hex(FRIJ_TEXT_2), LV_PART_MAIN);
    lv_obj_set_style_opa(hint, LV_OPA_30, LV_PART_MAIN);
    lv_obj_add_flag(hint, LV_OBJ_FLAG_FLOATING);  // ignore the page's flex/scroll
    lv_obj_clear_flag(hint, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -frij_screen_min() * 7 / 100);

    if (frij_anim_enabled()) {  // gentle bob (respects reduce-motion)
        lv_anim_t bob;
        lv_anim_init(&bob);
        lv_anim_set_var(&bob, hint);
        lv_anim_set_exec_cb(&bob, frij_anim_exec_translate_y);
        lv_anim_set_values(&bob, 0, -6);
        lv_anim_set_duration(&bob, 700);
        lv_anim_set_playback_duration(&bob, 700);
        lv_anim_set_repeat_count(&bob, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&bob, lv_anim_path_ease_in_out);
        lv_anim_start(&bob);
    }
    return hint;
}

// Keep the slider-card's right-hand value readout in sync as it's dragged.
static void slider_value_cb(lv_event_t* e)
{
    lv_obj_t*   s   = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t*   val = lv_obj_get_child(s, 1);  // [0] = label, [1] = value readout
    const char* unit = (const char*)lv_obj_get_user_data(val);
    lv_label_set_text_fmt(val, "%d%s", lv_slider_get_value(s), unit ? unit : "");
}

lv_obj_t* frij_slider_row(lv_obj_t* parent, const char* label, int min, int max,
                          int value, uint32_t accent, const char* unit)
{
    lv_obj_t* s = lv_slider_create(parent);
    lv_obj_set_width(s, LV_PCT(100));
    lv_obj_set_height(s, FRIJ_ROW_H);  // matches the other rows
    lv_slider_set_range(s, min, max);
    lv_obj_set_style_anim_duration(s, FRIJ_ANIM_MS, LV_PART_MAIN);
    // fill sweeps to its value on build (unless reduce-motion is on)
    lv_slider_set_value(s, value, frij_anim_enabled() ? LV_ANIM_ON : LV_ANIM_OFF);

    // card body (MAIN), accent fill (INDICATOR), no visible knob
    grad_v(s, FRIJ_SURFACE_2, 0x101216);
    lv_obj_set_style_radius(s, FRIJ_RADIUS_M, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s, lv_color_hex(accent), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s, LV_OPA_50, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s, FRIJ_RADIUS_M, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(s, 0, LV_PART_KNOB);

    lv_obj_t* l = lv_label_create(s);  // child [0]: title on the left
    lv_label_set_text(l, label);
    lv_obj_set_style_text_color(l, lv_color_hex(FRIJ_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(l, FRIJ_FONT_BODY, LV_PART_MAIN);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, FRIJ_SP_M, 0);

    lv_obj_t* val = lv_label_create(s);  // child [1]: live value on the right
    lv_obj_set_style_text_color(val, lv_color_hex(FRIJ_TEXT_2), LV_PART_MAIN);
    lv_obj_set_style_text_font(val, FRIJ_FONT_BODY, LV_PART_MAIN);
    lv_obj_align(val, LV_ALIGN_RIGHT_MID, -FRIJ_SP_M, 0);
    lv_obj_set_user_data(val, (void*)unit);  // read back by slider_value_cb
    lv_label_set_text_fmt(val, "%d%s", value, unit ? unit : "");
    lv_obj_add_event_cb(s, slider_value_cb, LV_EVENT_VALUE_CHANGED, NULL);

    frij_haptic_attach(s);
    return s;
}

lv_obj_t* frij_action_row(lv_obj_t* parent, const char* label, lv_event_cb_t on_click)
{
    lv_obj_t* row = frij_surface_row(parent);
    lv_obj_t* l   = frij_label(row, label, FRIJ_FONT_BODY, FRIJ_TEXT);
    lv_obj_set_flex_grow(l, 1);
    lv_obj_t* chev = lv_label_create(row);
    lv_label_set_text(chev, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(chev, FRIJ_FONT_SYMBOL, LV_PART_MAIN);
    lv_obj_set_style_text_color(chev, lv_color_hex(FRIJ_TEXT_2), LV_PART_MAIN);
    lv_obj_add_event_cb(row, on_click, LV_EVENT_CLICKED, NULL);
    return row;
}

lv_obj_t* frij_section_label(lv_obj_t* parent, const char* text)
{
    // UPPERCASE the heading for an iOS-like grouped-list look (labels are short).
    char up[32];
    size_t i = 0;
    for (; text[i] != '\0' && i < sizeof(up) - 1; i++) {
        char ch = text[i];
        up[i]   = (ch >= 'a' && ch <= 'z') ? (char)(ch - 'a' + 'A') : ch;
    }
    up[i] = '\0';

    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, up);
    lv_obj_set_style_text_font(l, FRIJ_FONT_SMALL, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(FRIJ_TEXT_2), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(l, 2, LV_PART_MAIN);  // airier, heading-like
    lv_obj_set_width(l, LV_PCT(100));
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_style_pad_left(l, FRIJ_SP_M, LV_PART_MAIN);
    lv_obj_set_style_pad_top(l, FRIJ_SP_S, LV_PART_MAIN);  // space above the group
    return l;
}

// ---- modals (shared) -------------------------------------------------------

// The currently-open modal (we never stack them), so the Back action can close
// it instead of navigating the launcher. Cleared when the modal is deleted.
static lv_obj_t* s_modal_top = NULL;

static void modal_clear_top_cb(lv_event_t* e)
{
    if (s_modal_top == lv_event_get_target(e)) {
        s_modal_top = NULL;
    }
}

static void modal_gone_cb(lv_anim_t* a)
{
    lv_obj_delete((lv_obj_t*)a->var);
}

// Fade the whole modal (dim + card) out, then delete. Guards double-close.
static void modal_close(lv_obj_t* modal)
{
    if (s_modal_top != modal) {
        return;  // already closing
    }
    s_modal_top = NULL;
    lv_obj_remove_flag(modal, LV_OBJ_FLAG_CLICKABLE);  // ignore taps mid-fade

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, modal);
    lv_anim_set_exec_cb(&a, frij_anim_exec_opa);  // overall opacity cascades to the card
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_duration(&a, FRIJ_ANIM_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&a, modal_gone_cb);
    lv_anim_start(&a);
}

bool frij_modal_close_top(void)
{
    if (s_modal_top) {
        modal_close(s_modal_top);
        return true;
    }
    return false;
}

// Tap on the backdrop itself (not a child) closes the modal.
static void modal_dismiss_cb(lv_event_t* e)
{
    if (lv_event_get_target(e) == lv_event_get_current_target(e)) {
        modal_close((lv_obj_t*)lv_event_get_user_data(e));
    }
}

// A dimmed, tap-to-dismiss backdrop on the active screen (above the launcher).
// The dim fades in; the caller adds a card via modal_card().
static lv_obj_t* modal_backdrop(void)
{
    lv_obj_t* modal = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(modal);
    lv_obj_set_size(modal, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(modal, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(modal, LV_OPA_TRANSP, LV_PART_MAIN);  // fades in below
    lv_obj_clear_flag(modal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(modal, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(modal, modal_dismiss_cb, LV_EVENT_CLICKED, modal);
    s_modal_top = modal;  // Back closes this first (see frij_modal_close_top)
    lv_obj_add_event_cb(modal, modal_clear_top_cb, LV_EVENT_DELETE, NULL);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, modal);
    lv_anim_set_exec_cb(&a, frij_anim_exec_bg_opa);
    lv_anim_set_values(&a, 0, LV_OPA_60);
    lv_anim_set_duration(&a, FRIJ_ANIM_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
    return modal;
}

// A centered Surface-2 card (flex column) that fades + rises in.
static lv_obj_t* modal_card(lv_obj_t* modal)
{
    lv_obj_t* card = lv_obj_create(modal);
    lv_obj_set_width(card, LV_PCT(74));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, lv_color_hex(FRIJ_SURFACE_2), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(card, FRIJ_RADIUS_L, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, FRIJ_SP_L, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, FRIJ_SP_M, LV_PART_MAIN);
    frij_anim_enter(card, 30);  // fade + rise entrance (no-op under reduce-motion)

    // …plus a subtle scale-in pop (pivot at the card's center)
    if (frij_anim_enabled()) {
        lv_obj_set_style_transform_pivot_x(card, lv_pct(50), LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_y(card, lv_pct(50), LV_PART_MAIN);
        lv_obj_set_style_transform_scale(card, 236, LV_PART_MAIN);  // ~0.92
        lv_anim_t pop;
        lv_anim_init(&pop);
        lv_anim_set_var(&pop, card);
        lv_anim_set_exec_cb(&pop, frij_anim_exec_scale);
        lv_anim_set_values(&pop, 236, 256);
        lv_anim_set_duration(&pop, FRIJ_ANIM_MS);
        lv_anim_set_delay(&pop, 30);
        lv_anim_set_path_cb(&pop, lv_anim_path_ease_out);
        lv_anim_start(&pop);
    }
    return card;
}

static lv_obj_t* pill_button(lv_obj_t* parent, const char* text, uint32_t bg, uint32_t fg,
                             lv_event_cb_t cb, void* user)
{
    lv_obj_t* b = lv_button_create(parent);
    lv_obj_set_height(b, 44);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, lv_color_hex(bg), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
    // The default theme grows buttons on press; that pokes past the card's clip.
    // Neutralize it and use a subtle dim instead.
    lv_obj_set_style_transform_width(b, 0, LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(b, 0, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(b, LV_OPA_80, LV_STATE_PRESSED);
    frij_haptic_attach(b);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);

    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, FRIJ_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(fg), LV_PART_MAIN);
    lv_obj_center(l);
    return b;
}

// ---- confirmation dialog ---------------------------------------------------

static void confirm_cancel_cb(lv_event_t* e)
{
    modal_close((lv_obj_t*)lv_event_get_user_data(e));
}

static void confirm_ok_cb(lv_event_t* e)
{
    lv_obj_t*     modal = (lv_obj_t*)lv_event_get_user_data(e);
    lv_event_cb_t cb    = (lv_event_cb_t)lv_obj_get_user_data(modal);  // the caller's handler
    if (cb) {
        cb(e);
    }
    modal_close(modal);
}

void frij_confirm(const char* title, const char* message, const char* confirm_text,
                  uint32_t accent, lv_event_cb_t on_confirm)
{
    lv_obj_t* modal = modal_backdrop();
    lv_obj_set_user_data(modal, (void*)on_confirm);  // read back by confirm_ok_cb

    lv_obj_t* card = modal_card(modal);
    lv_obj_t* t    = frij_label(card, title, FRIJ_FONT_TITLE, FRIJ_TEXT);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    if (message && message[0]) {
        lv_obj_t* m = frij_label(card, message, FRIJ_FONT_BODY, FRIJ_TEXT_2);
        lv_obj_set_width(m, LV_PCT(100));
        lv_label_set_long_mode(m, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(m, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }

    lv_obj_t* row = lv_obj_create(card);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    strip(row);
    lv_obj_set_style_pad_top(row, FRIJ_SP_XS, LV_PART_MAIN);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, FRIJ_SP_M, LV_PART_MAIN);

    pill_button(row, "Cancel", FRIJ_SURFACE_3, FRIJ_TEXT, confirm_cancel_cb, modal);
    pill_button(row, confirm_text, accent, 0xFFFFFF, confirm_ok_cb, modal);
}

// ---- action sheet ----------------------------------------------------------

typedef struct {
    frij_sheet_cb cb;
    void*         user;
} sheet_ctx_t;

static void sheet_free_cb(lv_event_t* e)
{
    lv_free(lv_event_get_user_data(e));  // ctx outlives the build call
}

static void sheet_cancel_cb(lv_event_t* e)
{
    modal_close((lv_obj_t*)lv_event_get_user_data(e));
}

static void sheet_option_cb(lv_event_t* e)
{
    lv_obj_t*     btn   = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t*     modal = (lv_obj_t*)lv_event_get_user_data(e);
    sheet_ctx_t*  c     = (sheet_ctx_t*)lv_obj_get_user_data(modal);
    int           idx   = (int)(intptr_t)lv_obj_get_user_data(btn);
    frij_sheet_cb cb    = c->cb;  // copy out before the modal (and ctx) are freed
    void*         user  = c->user;
    modal_close(modal);  // ctx is freed when the fade-out completes (sheet_free_cb)
    if (cb) {
        cb(idx, user);
    }
}

void frij_action_sheet(const char* title, const char* const* options, int count, uint32_t accent,
                       frij_sheet_cb cb, void* user)
{
    sheet_ctx_t* c = (sheet_ctx_t*)lv_malloc(sizeof(sheet_ctx_t));
    if (c == NULL) {
        return;  // OOM — don't open a sheet we can't track
    }
    c->cb   = cb;
    c->user = user;

    lv_obj_t* modal = modal_backdrop();
    lv_obj_set_user_data(modal, c);
    lv_obj_add_event_cb(modal, sheet_free_cb, LV_EVENT_DELETE, c);

    lv_obj_t* card = modal_card(modal);
    if (title && title[0]) {
        lv_obj_t* t = frij_label(card, title, FRIJ_FONT_TITLE, FRIJ_TEXT);
        lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }

    for (int i = 0; i < count; i++) {
        // first option is the primary (accent); the rest are neutral surfaces
        uint32_t  bg = (i == 0) ? accent : FRIJ_SURFACE_3;
        uint32_t  fg = (i == 0) ? 0xFFFFFF : FRIJ_TEXT;
        lv_obj_t* b  = pill_button(card, options[i], bg, fg, sheet_option_cb, modal);
        lv_obj_set_flex_grow(b, 0);          // stack at natural height in the column
        lv_obj_set_width(b, LV_PCT(100));
        lv_obj_set_user_data(b, (void*)(intptr_t)i);  // option index for the callback
    }

    lv_obj_t* cancel = pill_button(card, "Cancel", FRIJ_SURFACE_2, FRIJ_TEXT_2, sheet_cancel_cb, modal);
    lv_obj_set_flex_grow(cancel, 0);
    lv_obj_set_width(cancel, LV_PCT(100));
}

// ---- keyboard prompt (full-screen text entry) ------------------------------

typedef struct {
    frij_kb_cb cb;
    void*      user;
    lv_obj_t*  ta;
} kb_ctx_t;

static void kb_free_cb(lv_event_t* e)
{
    lv_free(lv_event_get_user_data(e));  // ctx outlives the build call
}

static void kb_event_cb(lv_event_t* e)
{
    lv_event_code_t code    = lv_event_get_code(e);
    lv_obj_t*       overlay = (lv_obj_t*)lv_event_get_user_data(e);
    kb_ctx_t*       c       = (kb_ctx_t*)lv_obj_get_user_data(overlay);
    if (code == LV_EVENT_READY && c->cb) {
        c->cb(lv_textarea_get_text(c->ta), c->user);
    }
    lv_obj_delete(overlay);  // READY or CANCEL both dismiss
}

void frij_keyboard_prompt(const char* title, bool password, frij_kb_cb cb, void* user)
{
    kb_ctx_t* c = (kb_ctx_t*)lv_malloc(sizeof(kb_ctx_t));
    if (c == NULL) {
        return;
    }
    c->cb   = cb;
    c->user = user;

    // full-screen overlay on the active screen (above the launcher + header,
    // like the modals) — drawn last, so it covers everything beneath it
    lv_obj_t* overlay = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay, lv_color_hex(FRIJ_SURFACE_1), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(overlay, c);
    lv_obj_add_event_cb(overlay, kb_free_cb, LV_EVENT_DELETE, c);

    lv_obj_t* t = frij_label(overlay, title, FRIJ_FONT_BODY, FRIJ_TEXT);
    lv_obj_set_width(t, LV_PCT(72));
    lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, frij_screen_min() * 14 / 100);

    lv_obj_t* ta = lv_textarea_create(overlay);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_password_mode(ta, password);
    lv_textarea_set_placeholder_text(ta, password ? "Password" : "Text");
    lv_obj_set_width(ta, LV_PCT(74));
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, frij_screen_min() * 23 / 100);
    c->ta = ta;

    lv_obj_t* kb = lv_keyboard_create(overlay);  // sits at the bottom, full width
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_READY, overlay);
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_CANCEL, overlay);
}

// ---- toast (auto-dismissing snackbar) --------------------------------------

static lv_obj_t* s_toast = NULL;  // at most one on screen at a time

static void toast_gone_cb(lv_anim_t* a)
{
    lv_obj_t* t = (lv_obj_t*)a->var;
    if (s_toast == t) {
        s_toast = NULL;
    }
    lv_obj_delete(t);
}

// Hold, then fade out + delete. Sequenced after the fade-in so the two opacity
// animations don't fight over the same property (which left the toast hidden).
static void toast_in_done_cb(lv_anim_t* a)
{
    lv_anim_t out;
    lv_anim_init(&out);
    lv_anim_set_var(&out, a->var);
    lv_anim_set_exec_cb(&out, frij_anim_exec_opa);
    lv_anim_set_values(&out, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_duration(&out, FRIJ_ANIM_MS);
    lv_anim_set_delay(&out, 1500);  // hold visible before fading
    lv_anim_set_completed_cb(&out, toast_gone_cb);
    lv_anim_start(&out);
}

// Tap a toast to dismiss it early: cancel its in/hold anims and fade out now.
static void toast_tap_cb(lv_event_t* e)
{
    lv_obj_t* t = (lv_obj_t*)lv_event_get_target(e);
    lv_anim_delete(t, NULL);  // drop the pending fade-in / hold / rise
    lv_anim_t out;
    lv_anim_init(&out);
    lv_anim_set_var(&out, t);
    lv_anim_set_exec_cb(&out, frij_anim_exec_opa);
    lv_anim_set_values(&out, lv_obj_get_style_opa(t, LV_PART_MAIN), LV_OPA_TRANSP);
    lv_anim_set_duration(&out, FRIJ_ANIM_MS / 2);
    lv_anim_set_completed_cb(&out, toast_gone_cb);
    lv_anim_start(&out);
}

// Shared builder: a pill with an optional leading status glyph + the message.
static void toast_show(const char* glyph, uint32_t glyph_color, const char* text)
{
    if (s_toast) {
        lv_obj_delete(s_toast);  // also cancels its anims (LVGL drops anims on delete)
    }
    lv_obj_t* t = lv_obj_create(lv_screen_active());
    s_toast     = t;
    lv_obj_remove_style_all(t);
    lv_obj_set_size(t, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(t, lv_color_hex(FRIJ_SURFACE_3), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(t, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_left(t, FRIJ_SP_L, LV_PART_MAIN);
    lv_obj_set_style_pad_right(t, FRIJ_SP_L, LV_PART_MAIN);
    lv_obj_set_style_pad_top(t, FRIJ_SP_S, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(t, FRIJ_SP_S, LV_PART_MAIN);
    lv_obj_clear_flag(t, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);  // tap to dismiss early
    lv_obj_add_event_cb(t, toast_tap_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(t, LV_ALIGN_BOTTOM_MID, 0, -frij_screen_min() * 14 / 100);
    // row: [glyph] text — centered, with a small gap when a glyph is present
    lv_obj_set_flex_flow(t, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(t, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(t, FRIJ_SP_S, LV_PART_MAIN);

    if (glyph && glyph[0]) {
        frij_label(t, glyph, FRIJ_FONT_SYMBOL, glyph_color);
    }
    lv_obj_t* l = frij_label(t, text, FRIJ_FONT_BODY, FRIJ_TEXT);
    // cap the label width so long messages wrap instead of pushing the pill off
    // the round edge; the pill (content-sized) then grows to the wrapped text
    lv_obj_set_style_max_width(l, frij_screen_min() * 64 / 100, LV_PART_MAIN);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    // Fade in; toast_in_done_cb then holds and fades it back out.
    lv_anim_t in;
    lv_anim_init(&in);
    lv_anim_set_var(&in, t);
    lv_anim_set_exec_cb(&in, frij_anim_exec_opa);
    lv_anim_set_values(&in, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&in, FRIJ_ANIM_MS);
    lv_anim_set_path_cb(&in, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&in, toast_in_done_cb);
    lv_anim_start(&in);

    // rise into place (snackbar feel) — skipped under reduce-motion
    if (frij_anim_enabled()) {
        lv_obj_set_style_translate_y(t, 16, LV_PART_MAIN);
        lv_anim_t rise;
        lv_anim_init(&rise);
        lv_anim_set_var(&rise, t);
        lv_anim_set_exec_cb(&rise, frij_anim_exec_translate_y);
        lv_anim_set_values(&rise, 16, 0);
        lv_anim_set_duration(&rise, FRIJ_ANIM_MS);
        lv_anim_set_path_cb(&rise, lv_anim_path_ease_out);
        lv_anim_start(&rise);
    }
}

void frij_toast(const char* text)
{
    toast_show(NULL, 0, text);
}

void frij_toast_status(const char* text, bool ok)
{
    toast_show(ok ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE,
               ok ? FRIJ_SECONDARY : FRIJ_DANGER, text);
}

lv_obj_t* frij_value_row(lv_obj_t* parent, const char* label, const char* value)
{
    lv_obj_t* r = frij_surface_row(parent);
    lv_obj_t* l = frij_label(r, label, FRIJ_FONT_BODY, FRIJ_TEXT);
    lv_obj_set_flex_grow(l, 1);
    frij_label(r, value, FRIJ_FONT_BODY, FRIJ_TEXT_2);
    return r;
}

lv_obj_t* frij_circle_button(lv_obj_t* parent, int diameter, uint32_t bg, const char* symbol,
                             const lv_font_t* font, uint32_t fg, lv_event_cb_t on_click)
{
    static lv_style_prop_t           props[] = {LV_STYLE_TRANSFORM_SCALE_X,
                                                LV_STYLE_TRANSFORM_SCALE_Y, LV_STYLE_PROP_INV};
    static lv_style_transition_dsc_t tr;
    static bool                      tr_ready = false;
    if (!tr_ready) {
        lv_style_transition_dsc_init(&tr, props, lv_anim_path_ease_out, 120, 0, NULL);
        tr_ready = true;
    }

    lv_obj_t* b = lv_button_create(parent);
    lv_obj_set_size(b, diameter, diameter);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, lv_color_hex(bg), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
    // tactile press-pop (shrinks slightly, eases back)
    lv_obj_set_style_transform_pivot_x(b, diameter / 2, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(b, diameter / 2, LV_PART_MAIN);
    lv_obj_set_style_transform_scale(b, 236, LV_STATE_PRESSED);
    lv_obj_set_style_transition(b, &tr, LV_PART_MAIN);
    frij_haptic_attach(b);
    if (on_click) {
        lv_obj_add_event_cb(b, on_click, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, symbol);
    lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(fg), LV_PART_MAIN);
    lv_obj_center(l);
    return b;
}

lv_obj_t* frij_toggle(lv_obj_t* parent, bool on, uint32_t accent)
{
    lv_obj_t* sw = lv_switch_create(parent);
    lv_obj_set_style_bg_color(sw, lv_color_hex(FRIJ_SURFACE_3), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, lv_color_hex(accent), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    if (on) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    frij_haptic_attach(sw);
    return sw;
}

// loose coupling: the launcher provides this; we don't include launcher.h
extern void frij_back(void);

static void on_header_back(lv_event_t* e)
{
    (void)e;
    frij_back();
}

static lv_obj_t* icon_button(lv_obj_t* parent, const char* sym)
{
    // Scale-pop transition for the press feedback (no background — just the icon).
    static lv_style_prop_t props[] = {LV_STYLE_TRANSFORM_SCALE_X, LV_STYLE_TRANSFORM_SCALE_Y,
                                      LV_STYLE_PROP_INV};
    static lv_style_transition_dsc_t tr;
    static bool                      tr_ready = false;
    if (!tr_ready) {
        lv_style_transition_dsc_init(&tr, props, lv_anim_path_ease_out, 120, 0, NULL);
        tr_ready = true;
    }

    lv_obj_t* b = lv_button_create(parent);
    lv_obj_set_size(b, 36, 36);
    lv_obj_set_ext_click_area(b, 10);  // enlarge the touch target past the 36px icon
    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, LV_PART_MAIN);  // no background, icon only
    lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
    // shrink the icon on press, ease back on release; cancel the theme's grow
    lv_obj_set_style_transform_pivot_x(b, 18, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(b, 18, LV_PART_MAIN);
    lv_obj_set_style_transform_scale(b, 224, LV_STATE_PRESSED);  // ~0.88
    lv_obj_set_style_transform_width(b, 0, LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(b, 0, LV_STATE_PRESSED);
    lv_obj_set_style_transition(b, &tr, LV_PART_MAIN);
    frij_haptic_attach(b);

    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, sym);
    lv_obj_set_style_text_font(l, FRIJ_FONT_SYMBOL, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(FRIJ_TEXT), LV_PART_MAIN);
    lv_obj_center(l);
    return b;
}

lv_obj_t* frij_header(lv_obj_t* parent, const char* title, lv_event_cb_t action_cb)
{
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_size(bar, LV_PCT(78), LV_SIZE_CONTENT);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, frij_screen_min() * 15 / 100);  // inside the circle
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar, 0, LV_PART_MAIN);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(bar, FRIJ_SP_S, LV_PART_MAIN);

    lv_obj_t* back = icon_button(bar, LV_SYMBOL_LEFT);
    lv_obj_add_event_cb(back, on_header_back, LV_EVENT_CLICKED, NULL);

    lv_obj_t* t = frij_label(bar, title, FRIJ_FONT_TITLE, FRIJ_TEXT);
    lv_obj_set_flex_grow(t, 1);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);

    // Right action — always present (keeps the title centered), hidden until set.
    lv_obj_t* action = icon_button(bar, "");
    if (action_cb) {
        lv_obj_add_event_cb(action, action_cb, LV_EVENT_CLICKED, NULL);
    }
    lv_obj_set_style_opa(action, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_remove_flag(action, LV_OBJ_FLAG_CLICKABLE);
    return bar;
}

void frij_header_set_action(lv_obj_t* header, const char* symbol)
{
    lv_obj_t* action = lv_obj_get_child(header, lv_obj_get_child_count(header) - 1);
    lv_obj_t* label  = lv_obj_get_child(action, 0);
    if (symbol && symbol[0]) {
        lv_label_set_text(label, symbol);
        lv_obj_set_style_opa(action, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_add_flag(action, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_set_style_opa(action, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_remove_flag(action, LV_OBJ_FLAG_CLICKABLE);
    }
}
