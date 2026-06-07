#include "todo.h"

/*
 * Todo — a simple checklist.
 *
 * Includes only "app.h"; knows nothing about the launcher.
 * open() fills the parent container it is handed.
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
        // A checkbox toggles itself when tapped.
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
