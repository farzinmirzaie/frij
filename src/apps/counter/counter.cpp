#include "counter.h"

#include <stdlib.h>  // atoi

#include "store/store.h"

/*
 * Counter — a number with - and + buttons.
 *
 * Includes only "app.h" (+ the shared store); knows nothing about the launcher.
 *   glance : shows the current value
 *   screen : the value plus -/+ buttons (one screen)
 *
 * The value persists via the shared store under the key "counter".
 */

static const char* STORE_KEY = "counter";

static int       s_count = 0;
static lv_obj_t* s_value = NULL;  // the big number label on the app screen

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

static lv_obj_t* make_button(lv_obj_t* parent, const char* text, lv_event_cb_t cb)
{
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_size(btn, 56, 56);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return btn;
}

static void glance(lv_obj_t* parent)
{
    load_count();

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "Counter");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

    lv_obj_t* value = lv_label_create(parent);
    lv_label_set_text_fmt(value, "%d", s_count);
    lv_obj_set_style_text_color(value, lv_color_hex(0x8A93A0), LV_PART_MAIN);
}

static void screen(lv_obj_t* parent, int index)
{
    (void)index;  // single screen
    frij_store_pull(STORE_KEY);
    load_count();

    s_value = lv_label_create(parent);
    lv_obj_set_style_text_color(s_value, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_value, &lv_font_montserrat_26, LV_PART_MAIN);
    lv_obj_align(s_value, LV_ALIGN_CENTER, 0, -10);
    refresh();

    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 16, LV_PART_MAIN);

    make_button(row, LV_SYMBOL_MINUS, on_minus);
    make_button(row, LV_SYMBOL_PLUS, on_plus);
}

const frij_app_t* counter_app(void)
{
    static const frij_app_t app = {"Counter", 0x123A6B, glance, 1, screen};
    return &app;
}
