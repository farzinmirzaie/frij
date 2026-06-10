#include "battery.h"

/*
 * The readings (pct/charging) are board-specific — emulator mock vs M5.Power on
 * device — behind the usual guard. The reactive layer on top (subjects + refresh
 * timer) is neutral and is defined once, below the guard.
 */
#if defined(__has_include) && (__has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>))

// Emulator: no real cell — report a steady, believable level.
uint8_t frij_battery_pct(void) { return 76; }
bool    frij_battery_charging(void) { return false; }

#else

#include <M5Unified.h>

uint8_t frij_battery_pct(void)
{
    int level = M5.Power.getBatteryLevel();  // -1 if unknown
    if (level < 0) {
        return 0;
    }
    return level > 100 ? 100 : (uint8_t)level;
}

bool frij_battery_charging(void)
{
    return M5.Power.isCharging();
}

#endif

// ---- reactive layer (neutral) ----------------------------------------------

static lv_subject_t s_level;
static lv_subject_t s_charging;

lv_subject_t* frij_battery_level_subject(void) { return &s_level; }
lv_subject_t* frij_battery_charging_subject(void) { return &s_charging; }

// Push the latest readings into the subjects (only notifies observers on change).
static void battery_refresh(lv_timer_t* t)
{
    (void)t;
    lv_subject_set_int(&s_level, frij_battery_pct());
    lv_subject_set_int(&s_charging, frij_battery_charging() ? 1 : 0);
}

void frij_battery_init(void)
{
    static bool inited = false;
    if (inited) {
        return;  // idempotent — safe if called from both a harness and user_app
    }
    inited = true;
    lv_subject_init_int(&s_level, frij_battery_pct());
    lv_subject_init_int(&s_charging, frij_battery_charging() ? 1 : 0);
    lv_timer_create(battery_refresh, 5000, NULL);  // 5s is plenty for a battery
}
