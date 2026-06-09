#include "counter.h"

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
    s_count = frij_store_load_int(STORE_KEY, 0);
}

static void save_count(void)
{
    frij_store_save_int(STORE_KEY, s_count);
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

    frij_circle_button(row, 56, FRIJ_SURFACE_2, LV_SYMBOL_MINUS, FRIJ_FONT_SYMBOL, ACCENT, on_minus);
    frij_circle_button(row, 56, FRIJ_SURFACE_2, LV_SYMBOL_PLUS, FRIJ_FONT_SYMBOL, ACCENT, on_plus);
}

const frij_app_t* counter_app(void)
{
    static const frij_app_t app = {"Counter", ACCENT, glance, 1, screen};
    return &app;
}
