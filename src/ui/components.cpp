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

int frij_header_zone(void)
{
    return frij_screen_min() * 19 / 100;
}

void frij_apply_bg(lv_obj_t* obj)
{
    // pure black, no gradient: pixels switch fully off on the AMOLED panel and
    // launcher pages can't show a gray seam between each other
    lv_obj_set_style_bg_color(obj, lv_color_hex(FRIJ_SURFACE_1), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
}

lv_obj_t* frij_round_mask(lv_obj_t* parent, uint32_t color)
{
    // A thick circular ring whose hole is exactly the round panel: parented to
    // a top layer it hides the square window's corners ABOVE everything —
    // overlays included — so the emulator always shows the true round display.
    // (The real panel has no corners; nothing creates this on the device.)
    int       m = frij_screen_min();
    const int b = (m * 30) / 100;  // thick enough to reach past the corners
    lv_obj_t* ring = lv_obj_create(parent);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, m + 2 * b, m + 2 * b);
    lv_obj_center(ring);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(ring, b, LV_PART_MAIN);
    lv_obj_set_style_border_color(ring, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_border_opa(ring, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, LV_PART_MAIN);
    // FLOATING: the oversized ring must not grow the parent's scroll area or
    // shift its layout — it's pure chrome pinned over the center.
    lv_obj_add_flag(ring, LV_OBJ_FLAG_FLOATING);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
    return ring;
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

lv_obj_t* frij_header_fade(lv_obj_t* parent, int top_px)
{
    // The base color fading to transparent over ~28px: rows scrolling up under
    // the header dissolve into the dark zone instead of clipping at a hard line.
    lv_obj_t* g = lv_obj_create(parent);
    lv_obj_remove_style_all(g);
    lv_obj_set_size(g, LV_PCT(100), 28);
    lv_obj_set_pos(g, 0, top_px);
    lv_obj_clear_flag(g, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g, LV_OBJ_FLAG_SCROLLABLE);

    lv_grad_dsc_t* d = (lv_grad_dsc_t*)lv_malloc(sizeof(lv_grad_dsc_t));
    if (d == NULL) {
        return g;  // OOM — skip the fade
    }
    lv_memzero(d, sizeof(*d));
    d->dir            = LV_GRAD_DIR_VER;
    d->stops_count    = 2;
    d->stops[0].color = lv_color_hex(FRIJ_SURFACE_1);
    d->stops[0].opa   = LV_OPA_COVER;
    d->stops[0].frac  = 0;
    d->stops[1].color = lv_color_hex(FRIJ_SURFACE_1);
    d->stops[1].opa   = 0;
    d->stops[1].frac  = 255;
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

void frij_page_settle_at(lv_obj_t* page, int32_t scroll_y)
{
    frij_page_settle(page);
    lv_obj_scroll_to_y(page, scroll_y, LV_ANIM_OFF);  // clamped if content shrank
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

lv_obj_t* frij_empty_state(lv_obj_t* parent, const char* title, const char* subtitle)
{
    // Text-only (no icon): a clear title and a fainter hint on what fills the
    // screen ("Add events in Google Calendar"). Centered, multi-line-safe.
    lv_obj_t* box = frij_col(parent, FRIJ_SP_S);

    lv_obj_t* t = frij_label(box, title, FRIJ_FONT_BODY, FRIJ_TEXT);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    if (subtitle && subtitle[0]) {
        lv_obj_t* s = frij_label(box, subtitle, FRIJ_FONT_SMALL, FRIJ_TEXT_3);
        lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }
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

lv_obj_t* frij_logo(lv_obj_t* parent, int size, bool with_name)
{
    // row: [clover] [Frij]
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, FRIJ_SP_S, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);

    // The halo is the ROW's own background: the row gets `inset` padding all
    // around and a radial gradient centered (in px) on the clover. Nothing is
    // oversized relative to its parent, so the glow structurally cannot clip.
    int inset = size * 2 / 5;
    lv_obj_set_style_pad_all(row, inset, LV_PART_MAIN);
    lv_grad_dsc_t* gd = (lv_grad_dsc_t*)lv_malloc(sizeof(lv_grad_dsc_t));
    if (gd != NULL) {
        int cx = inset + size / 2;  // clover center within the padded row
        lv_memzero(gd, sizeof(*gd));
        lv_grad_radial_init(gd, cx, cx, cx + inset + size / 2, cx, LV_GRAD_EXTEND_PAD);
        gd->stops_count    = 3;
        gd->stops[0].color = lv_color_hex(FRIJ_PRIMARY);
        gd->stops[0].opa   = 90;
        gd->stops[0].frac  = 0;
        gd->stops[1].color = lv_color_hex(FRIJ_PRIMARY);
        gd->stops[1].opa   = 0;
        gd->stops[1].frac  = 215;
        gd->stops[2].color = lv_color_hex(FRIJ_PRIMARY);
        gd->stops[2].opa   = 0;
        gd->stops[2].frac  = 255;
        lv_obj_set_style_bg_grad(row, gd, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_add_event_cb(row, glow_free_cb, LV_EVENT_DELETE, gd);
    }

    lv_obj_t* clover = lv_obj_create(row);
    lv_obj_remove_style_all(clover);
    lv_obj_set_size(clover, size, size);
    lv_obj_clear_flag(clover, LV_OBJ_FLAG_SCROLLABLE);

    static const uint32_t colors[] = {FRIJ_PINK, FRIJ_PRIMARY, FRIJ_INFO};
    int d = size * 65 / 100;  // circle diameter; ~35% overlap toward the center
    const int px[] = {(size - d) / 2, 0, size - d};  // top-mid, bottom-left, bottom-right
    const int py[] = {0, size - d, size - d};
    for (int i = 0; i < 3; i++) {
        lv_obj_t* c = lv_obj_create(clover);
        lv_obj_remove_style_all(c);
        lv_obj_set_size(c, d, d);
        lv_obj_set_pos(c, px[i], py[i]);
        lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        // vertical gradient (lighter top → deeper bottom) gives the orbs depth
        lv_obj_set_style_bg_color(c, lv_color_lighten(lv_color_hex(colors[i]), 40), LV_PART_MAIN);
        lv_obj_set_style_bg_grad_color(c, lv_color_darken(lv_color_hex(colors[i]), 60),
                                       LV_PART_MAIN);
        lv_obj_set_style_bg_grad_dir(c, LV_GRAD_DIR_VER, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(c, 215, LV_PART_MAIN);  // slight blend where they overlap
    }

    if (with_name) {
        // Big heroes use the dedicated 56px wordmark cut (crisp, no scaling);
        // small inline logos fall back to the title font.
        frij_label(row, "Frij", size >= 40 ? FRIJ_FONT_LOGO : FRIJ_FONT_TITLE, FRIJ_TEXT);
    }
    return row;
}

lv_obj_t* frij_lock_icon(lv_obj_t* parent)
{
    // No padlock in the symbol font, so draw a tiny one: a ring shackle on top
    // of a filled rounded body.
    lv_obj_t* lock = lv_obj_create(parent);
    lv_obj_remove_style_all(lock);
    lv_obj_set_size(lock, 14, 16);
    lv_obj_clear_flag(lock, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(lock, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* shackle = lv_obj_create(lock);
    lv_obj_remove_style_all(shackle);
    lv_obj_set_size(shackle, 10, 10);
    lv_obj_align(shackle, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_radius(shackle, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(shackle, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(shackle, lv_color_hex(FRIJ_TEXT_2), LV_PART_MAIN);

    lv_obj_t* body = lv_obj_create(lock);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, 14, 9);
    lv_obj_align(body, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(body, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_color(body, lv_color_hex(FRIJ_TEXT_2), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(body, LV_OPA_COVER, LV_PART_MAIN);
    return lock;
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


lv_obj_t* frij_value_row(lv_obj_t* parent, const char* label, const char* value)
{
    lv_obj_t* r = frij_surface_row(parent);
    // read-only: no press darken / haptic — tapping it does nothing, so it
    // shouldn't feel tappable like an action row
    lv_obj_clear_flag(r, LV_OBJ_FLAG_CLICKABLE);
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
    lv_obj_set_style_anim_duration(sw, FRIJ_ANIM_MS, LV_PART_MAIN);  // knob slide = house motion
    lv_obj_set_style_bg_color(sw, lv_color_hex(FRIJ_SURFACE_3), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, lv_color_hex(accent), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    if (on) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    frij_haptic_attach(sw);
    return sw;
}

// The whole row toggles (not just the switch), so tapping anywhere flips it.
static void toggle_row_click_cb(lv_event_t* e)
{
    lv_obj_t* row = (lv_obj_t*)lv_event_get_current_target(e);
    lv_obj_t* sw  = lv_obj_get_child(row, lv_obj_get_child_count(row) - 1);
    if (lv_obj_has_state(sw, LV_STATE_CHECKED)) {
        lv_obj_remove_state(sw, LV_STATE_CHECKED);
    } else {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_send_event(sw, LV_EVENT_VALUE_CHANGED, NULL);  // run the caller's handler
}

lv_obj_t* frij_toggle_row(lv_obj_t* parent, const char* label, bool on, uint32_t accent)
{
    lv_obj_t* row = frij_surface_row(parent);
    lv_obj_t* l   = frij_label(row, label, FRIJ_FONT_BODY, FRIJ_TEXT);
    lv_obj_set_flex_grow(l, 1);
    lv_obj_t* sw = frij_toggle(row, on, accent);
    lv_obj_remove_flag(sw, LV_OBJ_FLAG_CLICKABLE);  // the row drives it
    lv_obj_add_event_cb(row, toggle_row_click_cb, LV_EVENT_CLICKED, NULL);
    return sw;
}

// loose coupling: the launcher provides this; we don't include launcher.h
extern void frij_back(void);

static void on_header_back(lv_event_t* e)
{
    (void)e;
    frij_back();
}

static lv_obj_t* icon_button(lv_obj_t* parent, const char* sym, uint32_t color)
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
    lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_center(l);
    return b;
}

lv_obj_t* frij_header(lv_obj_t* parent, const char* title, uint32_t accent,
                      lv_event_cb_t action_cb)
{
    // Slim bar: narrow enough to sit inside the circle's chord at this height.
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_size(bar, LV_PCT(62), LV_SIZE_CONTENT);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, frij_screen_min() * 11 / 100);  // inside the circle
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar, 0, LV_PART_MAIN);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(bar, FRIJ_SP_S, LV_PART_MAIN);

    // title + icons carry the app's accent; the background stays the dark base
    lv_obj_t* back = icon_button(bar, LV_SYMBOL_LEFT, accent);
    lv_obj_add_event_cb(back, on_header_back, LV_EVENT_CLICKED, NULL);

    lv_obj_t* t = frij_label(bar, title, FRIJ_FONT_HEADER, accent);  // real 22px cut, crisp
    lv_obj_set_flex_grow(t, 1);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);

    // Right action — always present (keeps the title centered), hidden until set.
    lv_obj_t* action = icon_button(bar, "", accent);
    if (action_cb) {
        lv_obj_add_event_cb(action, action_cb, LV_EVENT_CLICKED, NULL);
    }
    lv_obj_set_style_opa(action, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_remove_flag(action, LV_OBJ_FLAG_CLICKABLE);

    frij_anim_enter(bar, 60);  // settle in just after the layer slides up
    return bar;
}

// Fade the header action icon to `to` opacity (cancels any in-flight fade).
static void header_action_fade(lv_obj_t* action, lv_opa_t to)
{
    lv_anim_delete(action, frij_anim_exec_opa);  // don't fight a previous fade
    lv_opa_t from = lv_obj_get_style_opa(action, LV_PART_MAIN);
    if (!frij_anim_enabled() || from == to) {
        lv_obj_set_style_opa(action, to, LV_PART_MAIN);
        return;
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, action);
    lv_anim_set_exec_cb(&a, frij_anim_exec_opa);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, FRIJ_ANIM_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

void frij_header_set_action(lv_obj_t* header, const char* symbol)
{
    lv_obj_t* action = lv_obj_get_child(header, lv_obj_get_child_count(header) - 1);
    lv_obj_t* label  = lv_obj_get_child(action, 0);
    if (symbol && symbol[0]) {
        lv_label_set_text(label, symbol);
        header_action_fade(action, LV_OPA_COVER);
        lv_obj_add_flag(action, LV_OBJ_FLAG_CLICKABLE);
    } else {
        header_action_fade(action, LV_OPA_TRANSP);  // fade out, don't pop out
        lv_obj_remove_flag(action, LV_OBJ_FLAG_CLICKABLE);  // untappable right away
    }
}
