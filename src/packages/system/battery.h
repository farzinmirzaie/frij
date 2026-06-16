#ifndef FRIJ_BATTERY_H
#define FRIJ_BATTERY_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

/*
 * Battery — a board service. The emulator returns a believable mock; the device
 * reads the M5PM1 PMIC via M5.Power.
 *
 * Level/charging are also published as LVGL "subjects" (the observer pattern),
 * so any widget bound to them updates live everywhere — no need to be on a
 * specific screen or to rebuild. A timer refreshes them periodically.
 */
void    frij_battery_init(void);      // init the subjects + start the refresh timer
void    frij_battery_poll(void);      // force an immediate sample (e.g. on About open)
uint8_t frij_battery_pct(void);       // 0–100
bool    frij_battery_charging(void);  // true while on external power

// Reactive sources: level is an int (0–100), charging is an int (0/1). Bind
// widgets/observers to these to track the battery live.
lv_subject_t* frij_battery_level_subject(void);
lv_subject_t* frij_battery_charging_subject(void);

#endif  // FRIJ_BATTERY_H
