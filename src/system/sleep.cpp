#include "sleep.h"

#include "lvgl.h"

#include "brightness.h"
#include "display.h"
#include "store/store.h"
#include "ui/anim.h"

/*
 * Idle-sleep manager. App-agnostic: it watches LVGL's input-inactivity timer and
 * turns the panel off once it exceeds the user's "Sleep" setting, then back on
 * the moment any input arrives (a touch resets the inactivity timer; so does a
 * wrist raise, which system/motion signals via lv_display_trigger_activity()).
 *
 * Two grace touches before the hard cut:
 *   - the last DIM_MS of the countdown drop the brightness ("about to sleep" —
 *     any touch restores it and resets the timer)
 *   - the shade fades to black instead of snapping; the panel powers off when
 *     the fade lands
 *
 * While asleep, a full-screen CLICKABLE black shade sits on the top layer. It
 * matters on BOTH targets: the touch controller stays live while the panel is
 * off, so without it the waking tap would click whatever happens to be under
 * the finger. The tap still resets LVGL's inactivity timer, which is the wake
 * signal — the shade just keeps it from reaching the UI.
 */

#define DIM_MS  10000  // pre-sleep warning window
#define DIM_PCT 15     // warning brightness

static bool      s_asleep  = false;
static bool      s_dimmed  = false;
static bool      s_inhibit = false;
static lv_obj_t* s_shade   = NULL;

void frij_sleep_inhibit(bool on)
{
    s_inhibit = on;
}

static void shade_opa_exec(void* o, int32_t v)
{
    lv_obj_set_style_bg_opa((lv_obj_t*)o, (lv_opa_t)v, LV_PART_MAIN);
}

static void shade_faded_off(lv_anim_t* a)
{
    (void)a;
    frij_display_set_on(false);  // cut the panel only after the fade lands
}

static void shade_set(bool sleeping)
{
    if (s_shade == NULL) {
        s_shade = lv_obj_create(lv_layer_top());
        lv_obj_remove_style_all(s_shade);
        lv_obj_set_size(s_shade, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(s_shade, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_shade, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_add_flag(s_shade, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_clear_flag(s_shade, LV_OBJ_FLAG_SCROLLABLE);
    }
    lv_anim_delete(s_shade, shade_opa_exec);
    if (sleeping) {
        lv_obj_add_flag(s_shade, LV_OBJ_FLAG_CLICKABLE);  // absorb the waking tap
        if (frij_anim_enabled()) {  // ease to black, then power the panel off
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, s_shade);
            lv_anim_set_exec_cb(&a, shade_opa_exec);
            lv_anim_set_values(&a, lv_obj_get_style_bg_opa(s_shade, LV_PART_MAIN), LV_OPA_COVER);
            lv_anim_set_duration(&a, 300);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
            lv_anim_set_completed_cb(&a, shade_faded_off);
            lv_anim_start(&a);
        } else {
            lv_obj_set_style_bg_opa(s_shade, LV_OPA_COVER, LV_PART_MAIN);
            frij_display_set_on(false);
        }
    } else {  // waking must be instant — no fade on the way back
        lv_obj_set_style_bg_opa(s_shade, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_clear_flag(s_shade, LV_OBJ_FLAG_CLICKABLE);
        frij_display_set_on(true);
    }
}

static void sleep_tick(lv_timer_t* t)
{
    (void)t;
    if (s_inhibit) {  // something (e.g. a running stopwatch) needs the screen
        lv_display_trigger_activity(NULL);
    }
    // The setting lives in the file-backed store — don't hit the filesystem
    // twice a second; re-read every ~2s (plenty responsive for a minutes value).
    static int     mins  = 0;
    static uint8_t skips = 0;
    if (mins == 0 || ++skips >= 4) {
        skips = 0;
        mins  = frij_store_load_int("sleep", 5);  // Settings ▸ Display ▸ Sleep
    }
    if (mins < 1) {
        mins = 1;
    }
    uint32_t threshold = (uint32_t)mins * 60u * 1000u;
    uint32_t idle      = lv_display_get_inactive_time(NULL);

    // Pre-sleep warning: dim for the last stretch; any activity restores.
    if (!s_asleep) {
        bool warn = idle >= threshold - DIM_MS && idle < threshold;
        if (warn && !s_dimmed) {
            s_dimmed = true;
            frij_set_brightness(DIM_PCT);
        } else if (!warn && s_dimmed) {
            s_dimmed = false;
            frij_set_brightness((uint8_t)frij_store_load_int("brightness", 80));
        }
    }

    if (!s_asleep && idle >= threshold) {
        shade_set(true);
        s_asleep = true;
    } else if (s_asleep && idle < threshold) {
        shade_set(false);  // input (or a raise) woke us — instant
        s_asleep = false;
        if (s_dimmed) {  // wake also undoes the pre-sleep dim
            s_dimmed = false;
            frij_set_brightness((uint8_t)frij_store_load_int("brightness", 80));
        }
    }
}

void frij_sleep_init(void)
{
    lv_timer_create(sleep_tick, 500, NULL);  // 0.5s cadence is plenty for minutes
}
