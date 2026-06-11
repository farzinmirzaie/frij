#include "sleep.h"

#include "lvgl.h"

#include "display.h"
#include "store/store.h"

/*
 * Idle-sleep manager. App-agnostic: it watches LVGL's input-inactivity timer and
 * turns the panel off once it exceeds the user's "Sleep" setting, then back on
 * the moment any input arrives (a touch resets the inactivity timer; so does a
 * wrist raise, which system/motion signals via lv_display_trigger_activity()).
 *
 * While asleep, a full-screen CLICKABLE black shade sits on the top layer. It
 * matters on BOTH targets: the touch controller stays live while the panel is
 * off, so without it the waking tap would click whatever happens to be under
 * the finger. The tap still resets LVGL's inactivity timer, which is the wake
 * signal — the shade just keeps it from reaching the UI.
 */

static bool      s_asleep = false;
static lv_obj_t* s_shade  = NULL;

static void shade_set(bool sleeping)
{
    if (s_shade == NULL) {
        s_shade = lv_obj_create(lv_layer_top());
        lv_obj_remove_style_all(s_shade);
        lv_obj_set_size(s_shade, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(s_shade, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_add_flag(s_shade, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_clear_flag(s_shade, LV_OBJ_FLAG_SCROLLABLE);
    }
    lv_obj_set_style_bg_opa(s_shade, sleeping ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
    if (sleeping) {
        lv_obj_add_flag(s_shade, LV_OBJ_FLAG_CLICKABLE);  // absorb the waking tap
    } else {
        lv_obj_clear_flag(s_shade, LV_OBJ_FLAG_CLICKABLE);
    }
}

static void sleep_tick(lv_timer_t* t)
{
    (void)t;
    int mins = frij_store_load_int("sleep", 5);  // Settings ▸ Display ▸ Sleep
    if (mins < 1) {
        mins = 1;
    }
    uint32_t threshold = (uint32_t)mins * 60u * 1000u;
    uint32_t idle      = lv_display_get_inactive_time(NULL);

    if (!s_asleep && idle >= threshold) {
        shade_set(true);
        frij_display_set_on(false);
        s_asleep = true;
    } else if (s_asleep && idle < threshold) {
        frij_display_set_on(true);  // input (or a raise) woke us
        shade_set(false);
        s_asleep = false;
    }
}

void frij_sleep_init(void)
{
    lv_timer_create(sleep_tick, 500, NULL);  // 0.5s cadence is plenty for minutes
}
