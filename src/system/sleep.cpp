#include "sleep.h"

#include "lvgl.h"

#include "display.h"
#include "store/store.h"

/*
 * Idle-sleep manager. App-agnostic: it watches LVGL's input-inactivity timer and
 * turns the panel off once it exceeds the user's "Sleep" setting, then back on
 * the moment any input arrives (a touch resets the inactivity timer; so does a
 * wrist raise, which system/motion signals via lv_display_trig_activity()).
 */

static bool s_asleep = false;

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
        frij_display_set_on(false);
        s_asleep = true;
    } else if (s_asleep && idle < threshold) {
        frij_display_set_on(true);  // input (or a raise) woke us
        s_asleep = false;
    }
}

void frij_sleep_init(void)
{
    lv_timer_create(sleep_tick, 500, NULL);  // 0.5s cadence is plenty for minutes
}
