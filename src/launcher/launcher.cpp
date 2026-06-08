#include "launcher.h"

#include <stdlib.h>  // abs

#include "lvgl.h"
#include "lvgl_port_m5stack.hpp"
#include "ui/carousel.h"
#include "ui/theme.h"
#include "registry.h"

/*
 * Navigation model — a vertical stack with the home in the middle:
 *
 *        [ SETTINGS ]   (swipe down from home)
 *        [  HOME    ]   (app glances)
 *        [  APP     ]   (swipe up from home)
 *
 * Each layer is a horizontal carousel (its own screens). One input handler on
 * the root decides axis: horizontal drags drive the active layer's carousel,
 * vertical drags slide whole layers (both follow the finger, snap on release).
 * Back returns to home from an app/settings layer.
 *
 * Locking: start() runs outside the LVGL task (locks). Everything else runs in
 * LVGL callbacks (no locking).
 */

typedef enum { HOME, APP, SETTINGS } layer_t;

static const uint32_t COLOR_BG      = FRIJ_SURFACE_1;
static const int      VSNAP_PERCENT = 30;

static lv_obj_t*       s_root     = NULL;
static lv_obj_t*       s_home     = NULL;  // persistent
static lv_obj_t*       s_app      = NULL;  // transient
static lv_obj_t*       s_settings = NULL;  // transient
static frij_carousel_t s_chome, s_capp, s_cset;
static frij_carousel_t* s_active = NULL;
static layer_t          s_cur    = HOME;

// input / animation state
static lv_point_t s_start;
static int        s_axis = 0;       // 0 undecided, 1 horizontal, 2 vertical
static bool       s_anim = false;   // a vertical transition is animating

static int height(void)
{
    return lv_obj_get_height(s_root);
}

// ---- page builders --------------------------------------------------------

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
    paint_bg(page, FRIJ_SURFACE_1);  // uniform dark; apps use their accent inside
    if (app->build_glance) {
        app->build_glance(page);
    }
}

static void app_screen_builder(lv_obj_t* page, int index, void* user)
{
    const frij_app_t* app = (const frij_app_t*)user;
    if (!app) {
        return;
    }
    paint_bg(page, FRIJ_SURFACE_1);
    if (app->build_screen) {
        app->build_screen(page, index);
    }
}

// ---- layers ---------------------------------------------------------------

static lv_obj_t* make_layer(void)
{
    lv_obj_t* o = lv_obj_create(s_root);
    lv_obj_set_size(o, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(o, 0, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(o, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(o, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(o, LV_OBJ_FLAG_EVENT_BUBBLE);
    return o;
}

static void ensure_app_layer(void)
{
    if (s_app) {
        return;
    }
    const frij_app_t* app = frij_registry_get(frij_carousel_index(&s_chome));
    if (!app) {
        return;
    }
    s_app = make_layer();
    lv_obj_set_y(s_app, height());  // starts below the home
    int n = app->screen_count > 0 ? app->screen_count : 1;
    frij_carousel_init(&s_capp, s_app, n, app_screen_builder, (void*)app);
}

static void ensure_settings_layer(void)
{
    if (s_settings) {
        return;
    }
    const frij_app_t* app = frij_registry_settings();
    if (!app) {
        return;
    }
    s_settings = make_layer();
    lv_obj_set_y(s_settings, -height());  // starts above the home
    int n = app->screen_count > 0 ? app->screen_count : 1;
    frij_carousel_init(&s_cset, s_settings, n, app_screen_builder, (void*)app);
}

// ---- vertical-transition completions --------------------------------------

static void anim_y(void* o, int32_t v)
{
    lv_obj_set_y((lv_obj_t*)o, (lv_coord_t)v);
}

static void slide_y(lv_obj_t* o, int to, lv_anim_completed_cb_t done)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_values(&a, lv_obj_get_y(o), to);
    lv_anim_set_duration(&a, 160);
    lv_anim_set_exec_cb(&a, anim_y);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    if (done) {
        lv_anim_set_completed_cb(&a, done);
    }
    lv_anim_start(&a);
}

static void done_enter_app(lv_anim_t* a)      { (void)a; s_cur = APP;      s_active = &s_capp; s_anim = false; }
static void done_enter_settings(lv_anim_t* a) { (void)a; s_cur = SETTINGS; s_active = &s_cset; s_anim = false; }

static void done_home_from_app(lv_anim_t* a)
{
    (void)a;
    if (s_app) { lv_obj_delete(s_app); s_app = NULL; }
    s_cur = HOME; s_active = &s_chome; s_anim = false;
}

static void done_home_from_settings(lv_anim_t* a)
{
    (void)a;
    if (s_settings) { lv_obj_delete(s_settings); s_settings = NULL; }
    s_cur = HOME; s_active = &s_chome; s_anim = false;
}

static void done_revert_to_home(lv_anim_t* a)
{
    (void)a;  // a partial open from home was cancelled: drop the transient layer
    if (s_app)      { lv_obj_delete(s_app);      s_app = NULL; }
    if (s_settings) { lv_obj_delete(s_settings); s_settings = NULL; }
    s_anim = false;
}

static void done_revert_simple(lv_anim_t* a) { (void)a; s_anim = false; }

// ---- vertical drag --------------------------------------------------------

static void nav_vdrag(int dy)
{
    int h = height();
    if (s_cur == HOME) {
        if (dy < 0) {  // swipe up -> reveal app from below
            ensure_app_layer();
            if (!s_app) {
                return;
            }
            lv_obj_set_y(s_home, dy);
            lv_obj_set_y(s_app, h + dy);
        } else if (dy > 0) {  // swipe down -> reveal settings from above
            ensure_settings_layer();
            if (!s_settings) {
                return;
            }
            lv_obj_set_y(s_home, dy);
            lv_obj_set_y(s_settings, -h + dy);
        }
    } else if (s_cur == APP) {
        if (dy > 0) {  // swipe down -> back to home
            lv_obj_set_y(s_app, dy);
            lv_obj_set_y(s_home, -h + dy);
        }
    } else {  // SETTINGS
        if (dy < 0) {  // swipe up -> back to home
            lv_obj_set_y(s_settings, dy);
            lv_obj_set_y(s_home, h + dy);
        }
    }
}

static void nav_vend(int dy)
{
    int h   = height();
    int thr = h * VSNAP_PERCENT / 100;
    s_anim  = true;

    if (s_cur == HOME && dy < 0 && s_app) {
        if (-dy > thr) { slide_y(s_home, -h, NULL); slide_y(s_app, 0, done_enter_app); }
        else           { slide_y(s_app, h, NULL);   slide_y(s_home, 0, done_revert_to_home); }
    } else if (s_cur == HOME && dy > 0 && s_settings) {
        if (dy > thr)  { slide_y(s_home, h, NULL);  slide_y(s_settings, 0, done_enter_settings); }
        else           { slide_y(s_settings, -h, NULL); slide_y(s_home, 0, done_revert_to_home); }
    } else if (s_cur == APP && dy > 0) {
        if (dy > thr)  { slide_y(s_app, h, NULL);   slide_y(s_home, 0, done_home_from_app); }
        else           { slide_y(s_app, 0, NULL);   slide_y(s_home, -h, done_revert_simple); }
    } else if (s_cur == SETTINGS && dy < 0) {
        if (-dy > thr) { slide_y(s_settings, -h, NULL); slide_y(s_home, 0, done_home_from_settings); }
        else           { slide_y(s_settings, 0, NULL);  slide_y(s_home, h, done_revert_simple); }
    } else {
        s_anim = false;  // no valid vertical move
    }
}

// ---- input + back ----------------------------------------------------------

static void on_input(lv_event_t* e)
{
    if (s_anim) {
        return;
    }
    lv_event_code_t code  = lv_event_get_code(e);
    lv_indev_t*     indev = lv_indev_active();
    if (indev == NULL) {
        return;
    }

    if (code == LV_EVENT_PRESSED) {
        lv_indev_get_point(indev, &s_start);
        s_axis = 0;
        return;
    }
    if (code == LV_EVENT_PRESSING) {
        lv_point_t p;
        lv_indev_get_point(indev, &p);
        int dx = p.x - s_start.x;
        int dy = p.y - s_start.y;
        if (s_axis == 0 && (abs(dx) > 8 || abs(dy) > 8)) {
            s_axis = (abs(dx) >= abs(dy)) ? 1 : 2;
        }
        if (s_axis == 1) {
            frij_carousel_drag(s_active, dx);
        } else if (s_axis == 2) {
            nav_vdrag(dy);
        }
        return;
    }
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        lv_point_t p;
        lv_indev_get_point(indev, &p);
        if (s_axis == 1) {
            frij_carousel_end(s_active, p.x - s_start.x);
        } else if (s_axis == 2) {
            nav_vend(p.y - s_start.y);
        }
        s_axis = 0;
    }
}

void frij_back(void)
{
    if (s_anim || s_cur == HOME) {
        return;
    }
    int h  = height();
    s_anim = true;
    if (s_cur == APP) {
        slide_y(s_app, h, NULL);
        slide_y(s_home, 0, done_home_from_app);
    } else {
        slide_y(s_settings, -h, NULL);
        slide_y(s_home, 0, done_home_from_settings);
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

    s_root = lv_obj_create(screen);
    lv_obj_set_size(s_root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_style_bg_color(s_root, lv_color_hex(COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_root, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_root, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_root, on_input, LV_EVENT_ALL, NULL);

    s_home = make_layer();
    lv_obj_set_y(s_home, 0);
    frij_carousel_init(&s_chome, s_home, frij_registry_count(), glance_builder, NULL);

    s_active = &s_chome;
    s_cur    = HOME;
    s_anim   = false;
    s_axis   = 0;

    lvgl_port_unlock();
}
