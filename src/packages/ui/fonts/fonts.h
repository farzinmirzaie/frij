#ifndef FRIJ_FONTS_H
#define FRIJ_FONTS_H

#include "lvgl.h"

/*
 * SF Rounded, converted to LVGL fonts with lv_font_conv (ASCII range unless
 * noted). Sources aren't committed; regenerate with the commands in this
 * folder's README if you change sizes. Symbols (✓ ± +) are not in these fonts —
 * symbol labels use Montserrat explicitly.
 */
LV_FONT_DECLARE(frij_sf_body)     // 18 px, Regular
LV_FONT_DECLARE(frij_sf_header)   // 22 px, Semibold (app header title)
LV_FONT_DECLARE(frij_sf_title)    // 26 px, Semibold
LV_FONT_DECLARE(frij_sf_display)  // 34 px, Semibold
LV_FONT_DECLARE(frij_sf_logo)     // 56 px, Semibold ("Frij" glyphs only)
LV_FONT_DECLARE(frij_sf_clock)    // 64 px, Semibold (digits + colon only)

#endif  // FRIJ_FONTS_H
