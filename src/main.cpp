#include <M5Unified.h>

#include "apps/assistant/assistant.h"
#include "launcher/launcher.h"
#include "lvgl_port_m5stack.hpp"
#include "system/motion.h"

/*
 * Device entry (compiled only for the `device` env — excluded from the emulator
 * and snapshot builds in platformio.ini).
 *
 * We let M5Unified bring the board up (M5.begin): it handles the AMOLED panel
 * reset via the M5IOE1 I/O expander, the CST820B touch, the BMI270 IMU, and the
 * PMIC — the things a bare gfx.init() can't. Then we hand M5Unified's display to
 * our LVGL port and start the app. See docs/HARDWARE.md.
 */

extern void user_app(void);

void setup(void)
{
    auto cfg         = M5.config();
    cfg.internal_imu = true;  // BMI270 — raise-to-wake (system/motion)
    M5.begin(cfg);

    lvgl_port_init(M5.Display);  // our LVGL bridge drives M5Unified's display
    user_app();
}

void loop(void)
{
    M5.update();           // refresh IMU + buttons
    frij_motion_update();  // raise-to-wake (no-op when the setting is off)

    // Key A (G2, yellow) = Back: tap goes back one layer, hold jumps home.
    if (M5.BtnA.wasReleased() && !M5.BtnA.wasHold()) {
        frij_back();
    } else if (M5.BtnA.wasHold()) {
        frij_home();
    }
    // Key B (G1, blue) = push-to-talk for Frij AI: hold to record, release to ask.
    if (M5.BtnB.wasPressed()) {
        frij_assistant_ptt(true);
    } else if (M5.BtnB.wasReleased()) {
        frij_assistant_ptt(false);
    }
    delay(5);
}
