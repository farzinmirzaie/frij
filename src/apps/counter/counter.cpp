#include "counter.h"

/*
 * Counter — a tiny demo app: a number with - and + buttons.
 *
 * Only includes "app.h"; knows nothing about the launcher.
 *
 * State is kept in file-static variables. That's fine here because only one
 * counter is ever on screen at a time, and open() resets it each time the app
 * is opened. (A real app would store state per-instance.)
 */

static int       s_count = 0;
static lv_obj_t* s_value = NULL;  // the big number label

static void refresh(void)
{
    char buf[16];
    lv_snprintf(buf, sizeof(buf), "%d", s_count);
    lv_label_set_text(s_value, buf);
}

static void on_minus(lv_event_t* e)
{
    (void)e;
    s_count--;
    refresh();
}

static void on_plus(lv_event_t* e)
{
    (void)e;
    s_count++;
    refresh();
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

static void open(lv_obj_t* parent)
{
    s_count = 0;

    // The number, centered above the buttons.
    s_value = lv_label_create(parent);
    lv_obj_set_style_text_color(s_value, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_value, &lv_font_montserrat_26, LV_PART_MAIN);
    lv_obj_align(s_value, LV_ALIGN_CENTER, 0, -10);
    refresh();

    // A row with - and + buttons below the number.
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
    static const frij_app_t app = {
        "Counter",  // name
        0x1565C0,   // blue tile
        open,
    };
    return &app;
}
