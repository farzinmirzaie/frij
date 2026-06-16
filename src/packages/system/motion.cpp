#include "motion.h"

/*
 * Raise-to-wake. Board-specific, same SDL/device guard as brightness.cpp.
 *
 * Device: the BMI270 is brought up by M5.begin(cfg.internal_imu) in main.cpp;
 * here we just poll the accelerometer each loop and, when the screen normal
 * (the board's Z axis) swings up to face the user, signal LVGL input activity so
 * the idle sleep manager (system/sleep) wakes the panel. The Z-axis threshold is
 * a sensible default — tune RAISE_Z / the axis sign once on the real unit.
 */
#if defined(__has_include) && (__has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>))

void frij_motion_init(void) {}
void frij_motion_update(void) {}

#else

#include <M5Unified.h>

#include "lvgl.h"
#include "store/store.h"

static const float RAISE_Z   = 0.6f;  // Z accel (g) above which the face is "up"
static bool        s_face_up = true;   // start assumed up so we only fire on a rise

void frij_motion_init(void)
{
    // The IMU is started by M5.begin(cfg.internal_imu = true) in main.cpp.
}

void frij_motion_update(void)
{
    // The "raisewake" setting is cached and re-read at most once a second — this
    // runs every ~5ms loop, so reading the file each call would hammer LittleFS.
    static bool     enabled   = true;
    static uint32_t last_read = 0;
    uint32_t        now       = lv_tick_get();
    if (last_read == 0 || now - last_read >= 1000) {
        enabled   = frij_store_load_bool("raisewake", true);
        last_read = now ? now : 1;
    }
    if (!enabled) {
        s_face_up = true;  // disabled: don't fire when re-enabled until next rise
        return;
    }
    float ax, ay, az;
    if (!M5.Imu.getAccel(&ax, &ay, &az)) {
        return;
    }
    bool up = (az > RAISE_Z);  // screen pointing up against gravity
    if (up && !s_face_up) {
        lv_display_trigger_activity(NULL);  // treat a raise as input → wakes the display
    }
    s_face_up = up;
}

#endif
