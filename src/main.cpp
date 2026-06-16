#include <M5Unified.h>
#include <lgfx/v1/platforms/esp32/Bus_SPI.hpp>  // enable DMA on the panel bus

#include "lvgl.h"  // lv_display_trigger_activity (button-wake)

#include "apps/assistant/assistant.h"
#include "launcher/launcher.h"
#include "lvgl_port_m5stack.hpp"
#include "system/audio.h"
#include "system/haptics.h"
#include "system/motion.h"
#include "system/sleep.h"

// A button press while the panel is asleep wakes it WITHOUT firing the button's
// action (Back/PTT). This flag latches for the duration of that press so the
// matching release is swallowed too; it clears once both buttons are up.
static bool s_btn_wake = false;

// Wake the screen on any G1/G2 press while asleep; returns true if this press is
// (still) a wake-only press whose action must be suppressed. Touch already wakes
// via LVGL's inactivity timer — this adds the physical buttons.
static bool buttons_wake_only(void)
{
    if ((M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) && frij_sleep_active()) {
        lv_display_trigger_activity(NULL);  // reset inactivity -> sleep_tick wakes
        s_btn_wake = true;
    }
    bool consumed = s_btn_wake;
    if (!M5.BtnA.isPressed() && !M5.BtnB.isPressed()) {
        s_btn_wake = false;  // both up — the next press is a fresh action
    }
    return consumed;
}

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

    // M5.begin() powers the ES8311 speaker amp; idle it hisses (white noise) and
    // nothing plays through it by default (the assistant shows text; UI tones are
    // off unless touch-sounds is on). Power it down — tones re-begin on demand.
    M5.Speaker.end();

    // M5GFX's StopWatch autodetect leaves the QSPI bus with DMA OFF (it sets
    // dma_channel for other boards but not this one), so every writePixels is a
    // CPU byte-bang → full-screen redraws (scrolling) crawl at ~2fps while a tiny
    // redraw (a toggle) is fine. Turn DMA on, matching M5StopWatch-UserDemo.
    if (auto *bus = static_cast<lgfx::Bus_SPI *>(M5.Display.getPanel()->getBus())) {
        auto bc          = bus->config();
        bc.dma_channel   = SPI_DMA_CH_AUTO;
        bus->config(bc);
        bus->init();
    }

    lvgl_port_init(M5.Display);  // our LVGL bridge drives M5Unified's display
    user_app();
}

void loop(void)
{
    M5.update();  // refresh IMU + buttons

    frij_motion_update();      // raise-to-wake (no-op when the setting is off)
    bool waking = buttons_wake_only();  // G1/G2 wake the panel; suppress their action

    // Physical buttons echo the same press feedback as on-screen taps. Haptic on
    // both. The click *tone* only on Back: Key A immediately hands the I2S bus to
    // the mic for recording, so a tone there would be cut off mid-buzz. Both
    // respect their settings (frij_haptic / frij_audio_click are no-ops when off).

    // Key A (G2, yellow) = push-to-talk for Frij AI: hold to record, release to ask.
    if (M5.BtnA.wasPressed() && !waking) {
        frij_haptic(FRIJ_HAPTIC_TAP);
        frij_assistant_ptt(true);
    } else if (M5.BtnA.wasReleased() && !waking) {
        frij_assistant_ptt(false);
    }
    // Key B (G1, blue) = Back: tap goes back one layer, hold jumps home.
    if (M5.BtnB.wasPressed() && !waking) {
        frij_haptic(FRIJ_HAPTIC_TAP);
        frij_audio_click();
    }
    if (M5.BtnB.wasReleased() && !M5.BtnB.wasHold() && !waking) {
        frij_back();
    } else if (M5.BtnB.wasHold() && !waking) {
        frij_home();
    }
    delay(5);
}
