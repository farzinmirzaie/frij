#ifndef FRIJ_MOTION_H
#define FRIJ_MOTION_H

// Motion / raise-to-wake (BMI270 IMU). Board-specific: real on device, no-op on
// the emulator (no IMU). When the "raisewake" setting is on and the wrist is
// raised to face the user, this signals input activity so the idle sleep
// manager wakes the display.
void frij_motion_init(void);    // called once at startup
void frij_motion_update(void);  // call frequently from the main loop (device)

#endif  // FRIJ_MOTION_H
