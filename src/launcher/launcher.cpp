#include "launcher.h"

#include "lvgl.h"
#include "lvgl_port_m5stack.hpp"
#include "carousel.h"
#include "registry.h"

/*
 * Three layers. The carousel owns horizontal drags (paging) and reports
 * vertical swipes via a callback; the launcher turns those into open/settings.
 *
 * Locking: frij_launcher_start() runs outside the LVGL task (locks). The
 * carousel callbacks and frij_back() run inside it (no locking).
 */

typedef enum { LAYER_LAUNCHER, LAYER_APP, LAYER_SETTINGS } layer_t;

static const uint32_t COLOR_BG = 0x101418;

static layer_t         s_layer = LAYER_LAUNCHER;
static frij_carousel_t s_apps;            // glance carousel (home)
static frij_carousel_t s_screens;         // open app's screen carousel
static lv_obj_t*       s_overlay = NULL;  // app/settings layer over the home

// ---- carousel page builders ----------------------------------------------

static void paint_bg(lv_obj_t* page, uint32_t color)
{
    lv_obj_set_style_bg_color(page, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, LV_PART_MAIN);
}

static void glance_builder(lv_obj_t* page, int index, void* user)
{
    (void)user;
    const frij_app_t* app = frij_registry_get(index);
    if (!app) {
        return;
    }
    paint_bg(page, app->color);
    if (app->build_glance) {
        app->build_glance(page);
    }
}

static void screen_builder(lv_obj_t* page, int index, void* user)
{
    const frij_app_t* app = (const frij_app_t*)user;
    if (!app) {
        return;
    }
    paint_bg(page, app->color);
    if (app->build_screen) {
        app->build_screen(page, index);
    }
}

// ---- layer transitions -----------------------------------------------------

static lv_obj_t* make_layer(void)
{
    lv_obj_t* o = lv_obj_create(lv_screen_active());
    lv_obj_set_size(o, LV_PCT(100), LV_PCT(100));
    lv_obj_center(o);
    lv_obj_set_style_bg_color(o, lv_color_hex(COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_border_width(o, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(o, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static void enter_app(int index)
{
    const frij_app_t* app = frij_registry_get(index);
    if (app == NULL) {
        return;
    }
    int screens = app->screen_count > 0 ? app->screen_count : 1;
    s_overlay = make_layer();
    frij_carousel_init(&s_screens, s_overlay, screens, screen_builder, NULL, (void*)app);
    s_layer = LAYER_APP;
}

static void open_settings(void)
{
    s_overlay = make_layer();
    lv_obj_t* label = lv_label_create(s_overlay);
    lv_label_set_text(label, "Settings\n(soon)");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(label);
    s_layer = LAYER_SETTINGS;
}

void frij_back(void)
{
    if (s_layer == LAYER_LAUNCHER) {
        return;
    }
    if (s_overlay) {
        lv_obj_delete(s_overlay);
        s_overlay = NULL;
    }
    s_layer = LAYER_LAUNCHER;
}

// ---- vertical swipe on the home carousel -----------------------------------

static void on_vswipe(lv_dir_t dir, void* user)
{
    (void)user;
    if (s_layer != LAYER_LAUNCHER) {
        return;
    }
    if (dir == LV_DIR_TOP) {
        enter_app(frij_carousel_index(&s_apps));
    } else if (dir == LV_DIR_BOTTOM) {
        open_settings();
    }
}

void frij_launcher_start(void)
{
    if (!lvgl_port_lock()) {
        return;
    }

    lv_obj_t* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BG), LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    frij_carousel_init(&s_apps, screen, frij_registry_count(), glance_builder, on_vswipe, NULL);
    s_layer = LAYER_LAUNCHER;

    lvgl_port_unlock();
}
