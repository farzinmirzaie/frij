#include "launcher.h"

#include <stdlib.h>  // abs

#include "lvgl.h"
#include "lvgl_port_m5stack.hpp"
#include "store/store.h"
#include "system/haptics.h"
#include "ui/anim.h"
#include "ui/carousel.h"
#include "ui/components.h"
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

static bool s_show_hint = true;  // swipe-up affordance; stops nagging after a few boots

static lv_obj_t*       s_root     = NULL;
static lv_obj_t*       s_home     = NULL;  // persistent
static lv_obj_t*       s_app      = NULL;  // transient
static lv_obj_t*       s_settings = NULL;  // transient
static frij_carousel_t s_chome, s_capp, s_cset;
static frij_carousel_t* s_active = NULL;
static layer_t          s_cur    = HOME;

// persistent header for the open app/settings layer (above its content carousel)
static const frij_app_t* s_layer_app    = NULL;
static lv_obj_t*         s_layer_header = NULL;

// input / animation state
static lv_point_t s_start;
static int        s_axis    = 0;     // 0 undecided, 1 horizontal, 2 vertical
static bool       s_anim    = false; // a vertical transition is animating
static bool       s_outside = false; // press began outside the round area
static bool       s_v_decided    = false;  // this vertical gesture's mode is set
static bool       s_v_transition = false;  // true = layer transition; false = content scroll
static uint32_t   s_press_tick   = 0;      // for fling detection (fast short swipes)

static int height(void)
{
    return lv_obj_get_height(s_root);
}

static int header_zone(void);  // height reserved at the top for the header

// ---- page builders --------------------------------------------------------

static void glance_builder(lv_obj_t* page, int index, void* user)
{
    (void)user;
    const frij_app_t* app = frij_registry_get(index);
    if (!app) {
        return;
    }
    frij_apply_bg(page);  // pure-black page base
    frij_glow(page, app->color);  // soft per-app accent halo behind the content
    if (app->build_glance) {
        app->build_glance(page);
    }
    // openable apps get a faint "swipe up to open" affordance (first boots only)
    if (s_show_hint && app->build_screen && app->screen_count >= 1) {
        frij_swipe_hint(page);
    }
    frij_page_settle(page);  // center if it fits, else top-align
}

static void app_screen_builder(lv_obj_t* page, int index, void* user)
{
    const frij_app_t* app = (const frij_app_t*)user;
    if (!app) {
        return;
    }
    frij_apply_bg(page);  // glow is on the layer (behind the header), not here
    if (app->build_screen) {
        app->build_screen(page, index);
    }
    frij_page_under_header(page, header_zone());  // breathing room + screen-centered
    frij_page_settle(page);                       // center if it fits, else top-align
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
    lv_obj_set_style_transform_pivot_x(o, lv_pct(50), LV_PART_MAIN);  // zoom from center
    lv_obj_set_style_transform_pivot_y(o, lv_pct(50), LV_PART_MAIN);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(o, LV_OBJ_FLAG_EVENT_BUBBLE);
    return o;
}

// Layer transition FX hook (mirrors the carousel's page_fx): `p` = outness,
// 0..256. The zoom/fade is disabled (see layer_fx) — too slow on the GPU-less
// panel — but the hook + `p` plumbing stay so the slide machinery is unchanged.

static void layer_fx(lv_obj_t* layer, int32_t p)
{
    // Layer zoom/fade is disabled: transform_scale + opa on a full-screen layer
    // make the GPU-less software renderer rasterize + rescale it every frame, so
    // the home<->settings<->app transition would crawl. The plain vertical slide
    // stays smooth. (No-op kept so the call sites/anims need no changes.)
    (void)layer;
    (void)p;
}

static void layer_fx_exec(void* layer, int32_t p)
{
    layer_fx((lv_obj_t*)layer, p);
}

static void layer_fx_to(lv_obj_t* layer, int32_t from, int32_t to)
{
    if (layer == NULL || !frij_anim_enabled()) {
        return;
    }
    lv_anim_delete(layer, layer_fx_exec);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, layer);
    lv_anim_set_exec_cb(&a, layer_fx_exec);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, 160);  // matches slide_y
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

// The persistent header (above the content carousel) calls this on tap; it
// dispatches to the app's action handler for the current screen.
static void header_action_clicked(lv_event_t* e)
{
    (void)e;
    frij_header_spin_action(s_layer_header);  // spin the refresh-style icon once
    if (s_layer_app && s_layer_app->on_action && s_active) {
        s_layer_app->on_action(frij_carousel_index(s_active));
    }
}

// Update the header's action icon when the content screen changes.
static void layer_change_cb(int index, void* user)
{
    (void)user;
    if (!s_layer_header) {
        return;
    }
    const char* sym = (s_layer_app && s_layer_app->action_symbol)
                          ? s_layer_app->action_symbol(index)
                          : NULL;
    frij_header_set_action(s_layer_header, sym);
}

// Re-query the current screen's action symbol — for when an app's action
// availability changes without a screen change (e.g. Wi-Fi toggled off hides
// the rescan button). The iso carousel launcher provides its own definition
// (see user_app.cpp), so skip this one in that build.
#ifndef FRIJ_NEW_LAUNCHER
void frij_launcher_refresh_action(void)
{
    if (s_active) {
        layer_change_cb(frij_carousel_index(s_active), NULL);
    }
}
#endif

// Add the shared header above a layer's content carousel. The header zone stays
// the dark base color; a small fade strip below it dissolves scrolling rows
// instead of hard-clipping them; title + icons take the app accent.
static void add_layer_header(lv_obj_t* layer, const frij_app_t* app, frij_carousel_t* car)
{
    frij_header_fade(layer, header_zone());  // above the content (added after it)

    s_layer_app = app;
    frij_carousel_set_change_cb(car, layer_change_cb, NULL);
    s_layer_header = frij_header(layer, app->name, app->color, header_action_clicked);
    layer_change_cb(frij_carousel_index(car), NULL);
}

// Push a layer's content carousel below the persistent header.
static int header_zone(void)
{
    return frij_header_zone();  // one source of truth (ui/components)
}

static void drop_content_below_header(frij_carousel_t* car)
{
    lv_obj_set_y(car->viewport, header_zone());
    lv_obj_set_height(car->viewport, frij_screen_min() - header_zone());
}

static void ensure_app_layer(void)
{
    if (s_app) {
        return;
    }
    const frij_app_t* app = frij_registry_get(frij_carousel_index(&s_chome));
    // glance-only apps (e.g. the Home watch face) can't be opened
    if (!app || app->build_screen == NULL || app->screen_count < 1) {
        return;
    }
    s_app = make_layer();
    lv_obj_set_y(s_app, height());  // starts below the home
    frij_carousel_init(&s_capp, s_app, app->screen_count, app_screen_builder, (void*)app, app->color);
    drop_content_below_header(&s_capp);
    add_layer_header(s_app, app, &s_capp);  // header on top, persists across screens
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
    frij_carousel_init(&s_cset, s_settings, n, app_screen_builder, (void*)app, app->color);
    drop_content_below_header(&s_cset);
    add_layer_header(s_settings, app, &s_cset);
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

static void done_enter_app(lv_anim_t* a)      { (void)a; s_cur = APP;      s_active = &s_capp; s_anim = false; frij_haptic(FRIJ_HAPTIC_SELECT); }
static void done_enter_settings(lv_anim_t* a) { (void)a; s_cur = SETTINGS; s_active = &s_cset; s_anim = false; frij_haptic(FRIJ_HAPTIC_SELECT); }

// We're back on home (a close finished, or a partial open was cancelled): drop
// whichever transient layer exists — only one can at a time.
static bool s_home_index0 = false;  // hold-Back: land on glance 0 after the close

static void done_back_home(lv_anim_t* a)
{
    (void)a;
    bool closed = s_cur != HOME;  // a real close, not a cancelled partial open
    if (s_app)      { lv_obj_delete(s_app);      s_app = NULL; }
    if (s_settings) { lv_obj_delete(s_settings); s_settings = NULL; }
    s_layer_app = NULL; s_layer_header = NULL;
    s_cur = HOME; s_active = &s_chome; s_anim = false;
    layer_fx(s_home, 0);  // safety: home settles at native scale/opacity
    if (closed) {
        frij_haptic(FRIJ_HAPTIC_SELECT);
        if (s_home_index0 && frij_carousel_index(&s_chome) != 0) {
            // hold-Back: continue to the watch face (goto builds it fresh)
            s_home_index0 = false;
            frij_carousel_goto(&s_chome, 0);
            return;
        }
        s_home_index0 = false;
        // Returning from an app: rebuild the visible glance so it shows fresh
        // data (random todo pick, new events, …) — glances otherwise live from
        // boot and would only refresh when swiped away and back.
        frij_carousel_refresh(&s_chome);
    }
}

static void done_revert_simple(lv_anim_t* a) { (void)a; s_anim = false; }

// ---- vertical drag --------------------------------------------------------

static void nav_vdrag(int dy)
{
    int h = height();
    int p = 256 * (dy < 0 ? -dy : dy) / (h > 0 ? h : 1);  // drag progress as outness
    if (s_cur == HOME) {
        if (dy < 0) {  // swipe up -> reveal app from below
            ensure_app_layer();
            if (!s_app) {
                return;
            }
            lv_obj_set_y(s_home, dy);
            lv_obj_set_y(s_app, h + dy);
            layer_fx(s_home, p);  // home recedes, the app grows in
            layer_fx(s_app, 256 - p);
        } else if (dy > 0) {  // swipe down -> reveal settings from above
            ensure_settings_layer();
            if (!s_settings) {
                return;
            }
            lv_obj_set_y(s_home, dy);
            lv_obj_set_y(s_settings, -h + dy);
            layer_fx(s_home, p);
            layer_fx(s_settings, 256 - p);
        }
    } else if (s_cur == APP) {
        if (dy > 0) {  // swipe down -> back to home
            lv_obj_set_y(s_app, dy);
            lv_obj_set_y(s_home, -h + dy);
            layer_fx(s_app, p);
            layer_fx(s_home, 256 - p);
        }
    } else {  // SETTINGS
        if (dy < 0) {  // swipe up -> back to home
            lv_obj_set_y(s_settings, dy);
            lv_obj_set_y(s_home, h + dy);
            layer_fx(s_settings, p);
            layer_fx(s_home, 256 - p);
        }
    }
}

static void nav_vend(int dy)
{
    int h   = height();
    int thr = h * VSNAP_PERCENT / 100;
    // A fast, short flick should commit too — waiting for the 30% threshold
    // makes quick swipes feel ignored. Direction safety comes from the per-
    // branch dy sign checks below.
    if (abs(dy) > 40 && lv_tick_elaps(s_press_tick) < 260) {
        thr = 40;
    }
    s_anim = true;

    int p = 256 * (dy < 0 ? -dy : dy) / (h > 0 ? h : 1);
    if (p > 256) p = 256;

    if (s_cur == HOME && dy < 0 && s_app) {
        if (-dy > thr) { slide_y(s_home, -h, NULL); slide_y(s_app, 0, done_enter_app);
                         layer_fx_to(s_home, p, 256); layer_fx_to(s_app, 256 - p, 0); }
        else           { slide_y(s_app, h, NULL);   slide_y(s_home, 0, done_back_home);
                         layer_fx_to(s_home, p, 0);  layer_fx_to(s_app, 256 - p, 256); }
    } else if (s_cur == HOME && dy > 0 && s_settings) {
        if (dy > thr)  { slide_y(s_home, h, NULL);  slide_y(s_settings, 0, done_enter_settings);
                         layer_fx_to(s_home, p, 256); layer_fx_to(s_settings, 256 - p, 0); }
        else           { slide_y(s_settings, -h, NULL); slide_y(s_home, 0, done_back_home);
                         layer_fx_to(s_home, p, 0);  layer_fx_to(s_settings, 256 - p, 256); }
    } else if (s_cur == APP && dy > 0) {
        if (dy > thr)  { slide_y(s_app, h, NULL);   slide_y(s_home, 0, done_back_home);
                         layer_fx_to(s_app, p, 256); layer_fx_to(s_home, 256 - p, 0); }
        else           { slide_y(s_app, 0, NULL);   slide_y(s_home, -h, done_revert_simple);
                         layer_fx_to(s_app, p, 0);  layer_fx_to(s_home, 256 - p, 256); }
    } else if (s_cur == SETTINGS && dy < 0) {
        if (-dy > thr) { slide_y(s_settings, -h, NULL); slide_y(s_home, 0, done_back_home);
                         layer_fx_to(s_settings, p, 256); layer_fx_to(s_home, 256 - p, 0); }
        else           { slide_y(s_settings, 0, NULL);  slide_y(s_home, h, done_revert_simple);
                         layer_fx_to(s_settings, p, 0); layer_fx_to(s_home, 256 - p, 256); }
    } else {
        s_anim = false;  // no valid vertical move
    }
}

// ---- input + back ----------------------------------------------------------

// A vertical drag becomes a layer transition (Back/open) only when the content
// can't scroll further in that direction; otherwise it scrolls natively.
static bool should_transition(int dy)
{
    if (s_cur == HOME) {
        return true;  // glances don't scroll; up = open, down = settings
    }
    lv_obj_t* content = s_active ? s_active->cur : NULL;
    if (!content) {
        return true;
    }
    if (s_cur == APP) {
        return dy > 0 && lv_obj_get_scroll_top(content) <= 0;     // swipe down at the top
    }
    return dy < 0 && lv_obj_get_scroll_bottom(content) <= 0;      // SETTINGS: swipe up at the bottom
}

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
        s_axis       = 0;
        s_v_decided  = false;
        s_press_tick = lv_tick_get();  // for fling detection
        // ignore touches outside the round panel
        int w  = lv_obj_get_width(s_root);
        int h  = lv_obj_get_height(s_root);
        int r  = (w < h ? w : h) / 2;
        int dx = s_start.x - w / 2;
        int dy = s_start.y - h / 2;
        s_outside = (dx * dx + dy * dy) > (r * r);
        return;
    }
    if (s_outside) {
        if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
            s_outside = false;
        }
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
            if (!s_v_decided) {
                s_v_decided    = true;
                s_v_transition = should_transition(dy);
            }
            if (s_v_transition) {
                nav_vdrag(dy);
            }
            // else: let LVGL scroll the content natively
        }
        return;
    }
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        lv_point_t p;
        lv_indev_get_point(indev, &p);
        if (s_axis == 1) {
            frij_carousel_end(s_active, p.x - s_start.x);
        } else if (s_axis == 2 && s_v_transition) {
            nav_vend(p.y - s_start.y);
        }
        s_axis = 0;
    }
}

// Close the open app/settings layer back to home (no screen-0 step).
static void close_layer(void)
{
    int h  = height();
    s_anim = true;
    if (s_cur == APP) {
        slide_y(s_app, h, NULL);
        slide_y(s_home, 0, done_back_home);
        layer_fx_to(s_app, 0, 256);  // app recedes, home grows back in
        layer_fx_to(s_home, 256, 0);
    } else {
        slide_y(s_settings, -h, NULL);
        slide_y(s_home, 0, done_back_home);
        layer_fx_to(s_settings, 0, 256);
        layer_fx_to(s_home, 256, 0);
    }
}

void frij_back(void)
{
    if (frij_modal_close_top()) {
        return;  // a dialog/sheet is open: Back closes it instead of navigating
    }
    if (s_anim) {
        return;
    }
    if (s_cur == HOME) {
        // already home: jump to the default screen (the clock, index 0)
        if (frij_carousel_index(&s_chome) != 0) {
            frij_carousel_goto(&s_chome, 0);
        }
        return;
    }
    // Inside an app/settings: step back to its MAIN screen first (the user
    // swiped sideways); only close the layer once already on screen 0.
    if (s_active && frij_carousel_index(s_active) != 0) {
        frij_carousel_goto(s_active, 0);
        return;
    }
    close_layer();
}

void frij_home(void)
{
    while (frij_modal_close_top()) {}  // shed every open dialog/sheet first
    if (s_anim) {
        return;
    }
    if (s_cur == HOME) {
        if (frij_carousel_index(&s_chome) != 0) {
            frij_carousel_goto(&s_chome, 0);
        }
        return;
    }
    s_home_index0 = true;   // done_back_home continues to glance 0
    close_layer();          // skip Back's screen-0 step — hold means ALL the way
}

void frij_launcher_start(void)
{
    // Show the swipe-up hint for the first few boots, then retire it — by then
    // the gesture is learned and the bobbing chevron is just noise.
    int seen    = frij_store_load_int("hint_seen", 0);
    s_show_hint = seen < 5;
    if (s_show_hint) {
        frij_store_save_int("hint_seen", seen + 1);
    }

    if (!lvgl_port_lock()) {
        return;
    }

    // The panel is round: clip everything to a circle and show a neutral
    // "outside" color in the corners (only visible in the square emulator
    // window; the real panel has no corners).
    lv_obj_t* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(FRIJ_OUTSIDE), LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    s_root = lv_obj_create(screen);
    lv_obj_set_size(s_root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_style_bg_color(s_root, lv_color_hex(COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_root, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_root, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(s_root, true, LV_PART_MAIN);  // clip children to the circle
    lv_obj_set_style_pad_all(s_root, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_root, on_input, LV_EVENT_ALL, NULL);

    s_home = make_layer();
    lv_obj_set_y(s_home, 0);
    frij_carousel_init(&s_chome, s_home, frij_registry_count(), glance_builder, NULL, FRIJ_TEXT);

    s_active = &s_chome;
    s_cur    = HOME;
    s_anim   = false;
    s_axis   = 0;

    lvgl_port_unlock();
}
