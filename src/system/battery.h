#ifndef FRIJ_BATTERY_H
#define FRIJ_BATTERY_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Battery — a board service. The emulator returns a believable mock so the UI
 * can show a charge indicator; the device reads the M5PM1 PMIC (TODO).
 */
void    frij_battery_init(void);
uint8_t frij_battery_pct(void);       // 0–100
bool    frij_battery_charging(void);  // true while on external power

#endif  // FRIJ_BATTERY_H
