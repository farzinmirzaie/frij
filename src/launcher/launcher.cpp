#include "launcher.h"

#include "lvgl.h"
#include "lvgl_port_m5stack.hpp"
#include "registry.h"

/*
 * Layout note: the screen is round, so we keep tiles in a centered, wrapping
 * grid and avoid the corners.
 *
 * Locking note: frij_launcher_start() runs OUTSIDE the LVGL task, so it locks.
 * The event callbacks below run INSIDE the LVGL task, so they must NOT lock
 * (that would deadlock).
 */

static const uint32_t COLOR_BG = 0x101418;

// ---- Opening / closing an app -------------------------------------------

// Back button: delete the whole app page; the home screen underneath returns.
static void on_back_clicked(lv_event_t* e)
{
    lv_obj_t* page = (lv_obj_t*)lv_event_get_user_data(e);
    lv_obj_delete(page);
}

static void open_app(int index)
{
    const frij_app_t* app = frij_registry_get(index);
    if (app == NULL || app->open == NULL) {
        return;
    }

    lv_obj_t* screen = lv_screen_active();

    // Full-screen opaque page that sits on top of the home screen.
    lv_obj_t* page = lv_obj_create(screen);
    lv_obj_set_size(page, LV_PCT(100), LV_PCT(100));
    lv_obj_center(page);
    lv_obj_set_style_bg_color(page, lv_color_hex(COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_border_width(page, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(page, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(page, 0, LV_PART_MAIN);

    // Launcher-owned "Back" button, pinned near the top.
    lv_obj_t* back = lv_button_create(page);
    lv_obj_align(back, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_add_event_cb(back, on_back_clicked, LV_EVENT_CLICKED, page);
    lv_obj_t* back_label = lv_label_create(back);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
    lv_obj_center(back_label);

    // The app draws into this content area; it never sees the back button.
    lv_obj_t* content = lv_obj_create(page);
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(72));
    lv_obj_align(content, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(content, 0, LV_PART_MAIN);

    app->open(content);
}

// ---- Home screen ---------------------------------------------------------

static void on_tile_clicked(lv_event_t* e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    open_app(index);
}

static void build_home(void)
{
    lv_obj_t* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BG), LV_PART_MAIN);

    // Centered, wrapping row of tiles.
    lv_obj_t* grid = lv_obj_create(screen);
    lv_obj_set_size(grid, LV_PCT(96), LV_PCT(96));
    lv_obj_center(grid);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(grid, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < frij_registry_count(); i++) {
        const frij_app_t* app = frij_registry_get(i);

        lv_obj_t* tile = lv_button_create(grid);
        lv_obj_set_size(tile, 90, 90);
        lv_obj_set_style_bg_color(tile, lv_color_hex(app->color), LV_PART_MAIN);
        lv_obj_set_style_radius(tile, 16, LV_PART_MAIN);
        lv_obj_add_event_cb(tile, on_tile_clicked, LV_EVENT_CLICKED,
                            (void*)(intptr_t)i);

        lv_obj_t* label = lv_label_create(tile);
        lv_label_set_text(label, app->name);
        lv_obj_center(label);
    }
}

void frij_launcher_start(void)
{
    if (!lvgl_port_lock()) {
        return;
    }
    build_home();
    lvgl_port_unlock();
}
