#include "todo.h"

/*
 * Todo — a simple checklist, with extra screens to exercise the app carousel.
 *
 * Includes only "app.h"; knows nothing about the launcher.
 *   glance   : item count
 *   screen 0 : the checklist
 *   screen 1 : an "add item" placeholder
 *   screen 2 : a stats summary
 */

static const char* ITEMS[] = {"Milk", "Eggs", "Bread", "Coffee"};
static const int ITEM_COUNT = sizeof(ITEMS) / sizeof(ITEMS[0]);

static void center_column(lv_obj_t* parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
}

static lv_obj_t* white_label(lv_obj_t* parent, const char* text)
{
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    return l;
}

static void glance(lv_obj_t* parent)
{
    center_column(parent);
    white_label(parent, "Todo");
    lv_obj_t* count = lv_label_create(parent);
    lv_label_set_text_fmt(count, "%d items", ITEM_COUNT);
    lv_obj_set_style_text_color(count, lv_color_hex(0xBFD8C9), LV_PART_MAIN);
}

static void screen(lv_obj_t* parent, int index)
{
    center_column(parent);

    if (index == 0) {
        for (int i = 0; i < ITEM_COUNT; i++) {
            lv_obj_t* item = lv_checkbox_create(parent);
            lv_checkbox_set_text(item, ITEMS[i]);
            lv_obj_set_style_text_color(item, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        }
    } else if (index == 1) {
        white_label(parent, LV_SYMBOL_PLUS "  Add item");
        lv_obj_t* hint = lv_label_create(parent);
        lv_label_set_text(hint, "(soon)");
        lv_obj_set_style_text_color(hint, lv_color_hex(0xBFD8C9), LV_PART_MAIN);
    } else {
        white_label(parent, "Stats");
        lv_obj_t* s = lv_label_create(parent);
        lv_label_set_text_fmt(s, "%d items\n0 done", ITEM_COUNT);
        lv_obj_set_style_text_color(s, lv_color_hex(0xBFD8C9), LV_PART_MAIN);
        lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }
}

const frij_app_t* todo_app(void)
{
    static const frij_app_t app = {"Todo", 0x14512F, glance, 3, screen};
    return &app;
}
