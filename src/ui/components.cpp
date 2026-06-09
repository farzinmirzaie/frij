#include "components.h"

#include <stdint.h>

#include "system/haptics.h"
#include "theme.h"

static void on_press_haptic(lv_event_t* e)
{
    (void)e;
    frij_haptic(FRIJ_HAPTIC_TAP);
}

// ---- animation exec callbacks ---------------------------------------------

static void set_opa_cb(void* o, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t*)o, (lv_opa_t)v, LV_PART_MAIN);
}

static void set_ty_cb(void* o, int32_t v)
{
    lv_obj_set_style_translate_y((lv_obj_t*)o, v, LV_PART_MAIN);
}

static void set_scale_cb(void* o, int32_t v)
{
    lv_obj_set_style_transform_scale_x((lv_obj_t*)o, v, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_y((lv_obj_t*)o, v, LV_PART_MAIN);
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

void frij_glow(lv_obj_t* parent, uint32_t accent)
{
    int       sz = frij_screen_min() * 80 / 100;
    lv_obj_t* g  = lv_obj_create(parent);
    lv_obj_remove_style_all(g);
    lv_obj_set_size(g, sz, sz);
    lv_obj_center(g);
    lv_obj_add_flag(g, LV_OBJ_FLAG_IGNORE_LAYOUT);  // background, not a flex item
    lv_obj_clear_flag(g, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g, LV_OBJ_FLAG_SCROLLABLE);

    // true radial gradient: accent at the center fading to transparent at the edge
    lv_grad_dsc_t* d = (lv_grad_dsc_t*)lv_malloc(sizeof(lv_grad_dsc_t));
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
    lv_obj_set_height(r, LV_SIZE_CONTENT);
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
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, check);
        lv_anim_set_exec_cb(&a, set_scale_cb);
        lv_anim_set_values(&a, 210, 256);  // 256 = 100%
        lv_anim_set_duration(&a, 180);
        lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
        lv_anim_start(&a);
        frij_haptic(FRIJ_HAPTIC_SUCCESS);
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
    lv_label_set_text(icon, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_font(icon, FRIJ_FONT_SYMBOL, LV_PART_MAIN);
    lv_obj_set_style_text_color(icon, lv_color_hex(FRIJ_PRIMARY), LV_PART_MAIN);
    lv_obj_center(icon);

    frij_label(box, text, FRIJ_FONT_BODY, FRIJ_TEXT_2);
    return box;
}

lv_obj_t* frij_slider_row(lv_obj_t* parent, const char* label, int min, int max,
                          int value, uint32_t accent)
{
    lv_obj_t* s = lv_slider_create(parent);
    lv_obj_set_width(s, LV_PCT(100));
    lv_obj_set_height(s, 52);  // card-sized
    lv_slider_set_range(s, min, max);
    lv_slider_set_value(s, value, LV_ANIM_OFF);

    // card body (MAIN), accent fill (INDICATOR), no visible knob
    grad_v(s, FRIJ_SURFACE_2, 0x101216);
    lv_obj_set_style_radius(s, FRIJ_RADIUS_M, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s, lv_color_hex(accent), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s, LV_OPA_50, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s, FRIJ_RADIUS_M, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(s, 0, LV_PART_KNOB);

    lv_obj_t* l = lv_label_create(s);
    lv_label_set_text(l, label);
    lv_obj_set_style_text_color(l, lv_color_hex(FRIJ_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(l, FRIJ_FONT_BODY, LV_PART_MAIN);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, FRIJ_SP_M, 0);

    frij_haptic_attach(s);
    return s;
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
    lv_obj_t* b = lv_button_create(parent);
    lv_obj_set_size(b, 36, 36);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, lv_color_hex(FRIJ_SURFACE_2), LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, lv_color_hex(FRIJ_SURFACE_3), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
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

void frij_anim_enter(lv_obj_t* obj, uint32_t delay_ms)
{
    lv_obj_set_style_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_translate_y(obj, 14, LV_PART_MAIN);

    lv_anim_t fade;
    lv_anim_init(&fade);
    lv_anim_set_var(&fade, obj);
    lv_anim_set_exec_cb(&fade, set_opa_cb);
    lv_anim_set_values(&fade, 0, LV_OPA_COVER);
    lv_anim_set_duration(&fade, FRIJ_ANIM_MS);
    lv_anim_set_delay(&fade, delay_ms);
    lv_anim_set_path_cb(&fade, lv_anim_path_ease_out);
    lv_anim_start(&fade);

    lv_anim_t rise;
    lv_anim_init(&rise);
    lv_anim_set_var(&rise, obj);
    lv_anim_set_exec_cb(&rise, set_ty_cb);
    lv_anim_set_values(&rise, 14, 0);
    lv_anim_set_duration(&rise, FRIJ_ANIM_MS + 40);
    lv_anim_set_delay(&rise, delay_ms);
    lv_anim_set_path_cb(&rise, lv_anim_path_ease_out);
    lv_anim_start(&rise);
}
