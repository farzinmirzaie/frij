#include "todo.h"

/*
 * Todo — a simple checklist.
 *
 * Includes only "app.h"; knows nothing about the launcher.
 *   glance : a count summary shown in the launcher carousel
 *   screen : the checklist (one screen)
 */

static const char* ITEMS[] = {"Milk", "Eggs", "Bread", "Coffee"};
static const int ITEM_COUNT = sizeof(ITEMS) / sizeof(ITEMS[0]);

static void glance(lv_obj_t* parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "Todo");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

    lv_obj_t* count = lv_label_create(parent);
    lv_label_set_text_fmt(count, "%d items", ITEM_COUNT);
    lv_obj_set_style_text_color(count, lv_color_hex(0x8A93A0), LV_PART_MAIN);
}

static void screen(lv_obj_t* parent, int index)
{
    (void)index;  // single screen

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < ITEM_COUNT; i++) {
        lv_obj_t* item = lv_checkbox_create(parent);
        lv_checkbox_set_text(item, ITEMS[i]);
        lv_obj_set_style_text_color(item, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    }
}

const frij_app_t* todo_app(void)
{
    static const frij_app_t app = {"Todo", glance, 1, screen};
    return &app;
}
