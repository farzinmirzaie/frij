#include "counter.h"

#include <stdlib.h>  // atoi

#include "store/store.h"
#include "ui/components.h"
#include "ui/theme.h"

/*
 * Counter — a number with - and + buttons. Persists via the store ("counter").
 * Color scheme: blue.
 */

static const char*    STORE_KEY = "counter";
static const uint32_t ACCENT    = FRIJ_ACCENT;  // blue

static int       s_count = 0;
static lv_obj_t* s_value = NULL;

static void load_count(void)
{
    char buf[16];
    if (frij_store_load(STORE_KEY, buf, sizeof(buf))) {
        s_count = atoi(buf);
    }
}

static void save_count(void)
{
    char buf[16];
    lv_snprintf(buf, sizeof(buf), "%d", s_count);
    frij_store_save(STORE_KEY, buf);
}

static void refresh(void)
{
    if (s_value) {
        lv_label_set_text_fmt(s_value, "%d", s_count);
    }
}

static void on_minus(lv_event_t* e)
{
    (void)e;
    s_count--;
    refresh();
    save_count();
}

static void on_plus(lv_event_t* e)
{
    (void)e;
    s_count++;
    refresh();
    save_count();
}

static lv_obj_t* round_button(lv_obj_t* parent, const char* sym, lv_event_cb_t cb)
{
    static lv_style_prop_t           props[] = {LV_STYLE_TRANSFORM_SCALE_X, LV_STYLE_TRANSFORM_SCALE_Y,
                                                LV_STYLE_BG_COLOR, LV_STYLE_PROP_INV};
    static lv_style_transition_dsc_t tr;
    static bool                      tr_ready = false;
    if (!tr_ready) {
        lv_style_transition_dsc_init(&tr, props, lv_anim_path_ease_out, 120, 0, NULL);
        tr_ready = true;
    }

    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_size(btn, 56, 56);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(FRIJ_SURFACE_2), LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(FRIJ_SURFACE_3), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    // tactile press pop
    lv_obj_set_style_transform_pivot_x(btn, 28, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(btn, 28, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_x(btn, 236, LV_STATE_PRESSED);
    lv_obj_set_style_transform_scale_y(btn, 236, LV_STATE_PRESSED);
    lv_obj_set_style_transition(btn, &tr, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, sym);
    lv_obj_set_style_text_font(label, FRIJ_FONT_SYMBOL, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(ACCENT), LV_PART_MAIN);
    lv_obj_center(label);
    return btn;
}

static void glance(lv_obj_t* parent)
{
    load_count();
    lv_obj_t* col = frij_page(parent);

    lv_obj_t* value = frij_label(col, "", FRIJ_FONT_DISPLAY, ACCENT);
    lv_label_set_text_fmt(value, "%d", s_count);
    frij_label(col, "Counter", FRIJ_FONT_BODY, FRIJ_TEXT_2);
}

static void screen(lv_obj_t* parent, int index)
{
    (void)index;
    frij_store_pull_async(STORE_KEY);
    load_count();

    lv_obj_t* col = frij_page(parent);
    s_value       = frij_label(col, "", FRIJ_FONT_DISPLAY, FRIJ_TEXT);
    refresh();

    lv_obj_t* row = lv_obj_create(col);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, FRIJ_SP_XL, LV_PART_MAIN);

    round_button(row, LV_SYMBOL_MINUS, on_minus);
    round_button(row, LV_SYMBOL_PLUS, on_plus);
}

const frij_app_t* counter_app(void)
{
    static const frij_app_t app = {"Counter", ACCENT, glance, 1, screen};
    return &app;
}
