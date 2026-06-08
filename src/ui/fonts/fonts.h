#ifndef FRIJ_FONTS_H
#define FRIJ_FONTS_H

#include "lvgl.h"

/*
 * SF Pro Rounded, converted to LVGL fonts with lv_font_conv (ASCII range).
 * Source OTFs are not committed; regenerate with the commands in this folder's
 * README if you change sizes. Symbols (✓ ± +) are not in these fonts — symbol
 * labels use Montserrat explicitly.
 */
LV_FONT_DECLARE(frij_sf_body)     // 14 px, Regular
LV_FONT_DECLARE(frij_sf_title)    // 20 px, Semibold
LV_FONT_DECLARE(frij_sf_display)  // 26 px, Semibold

#endif  // FRIJ_FONTS_H
