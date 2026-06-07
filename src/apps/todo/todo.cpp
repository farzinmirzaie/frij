#include "todo.h"

/*
 * Todo — a tiny demo app: a list of checkboxes you can tick off.
 *
 * It only includes "app.h". It knows nothing about the launcher.
 * `open()` just fills the parent container it is handed.
 */

static const char* ITEMS[] = {"Milk", "Eggs", "Bread", "Coffee"};
static const int ITEM_COUNT = sizeof(ITEMS) / sizeof(ITEMS[0]);

static void open(lv_obj_t* parent)
{
    // Stack the checkboxes in a centered column.
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < ITEM_COUNT; i++) {
        lv_obj_t* item = lv_checkbox_create(parent);
        lv_checkbox_set_text(item, ITEMS[i]);
        lv_obj_set_style_text_color(item, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        // A checkbox toggles itself on tap — no extra code needed for the demo.
    }
}

const frij_app_t* todo_app(void)
{
    static const frij_app_t app = {
        "Todo",    // name
        0x2E7D32,  // green tile
        open,
    };
    return &app;
}
