#ifndef FRIJ_THEME_H
#define FRIJ_THEME_H

#include "lvgl.h"

#include "fonts/fonts.h"

/*
 * Frij design tokens — colors, type, spacing, radius, motion.
 *
 * Derived from the Frij design system. Use these instead of hardcoding values
 * so the look stays consistent and is easy to retune in one place.
 */

// ---- Colors (0xRRGGBB) ----------------------------------------------------
#define FRIJ_PRIMARY    0xA855F7  // brand purple
#define FRIJ_SECONDARY  0x22C55E  // green
#define FRIJ_ACCENT     0x38BDF8  // blue
#define FRIJ_WARNING    0xFB8C00  // orange
#define FRIJ_INFO       0x7C3AED  // violet
#define FRIJ_YELLOW     0xFACC15  // amber

// Per-app accent palette. Each app picks ONE of these as its `color` and uses
// it for its checks/rings/highlights; the page background stays Surface-1, so
// every app shares the look but has its own scheme (e.g. Todo amber, Counter
// blue, Home purple).

#define FRIJ_SURFACE_1  0x0D0D10  // app background
#define FRIJ_SURFACE_2  0x16161B  // cards / rows
#define FRIJ_SURFACE_3  0x1F1F26  // pressed / elevated
#define FRIJ_BORDER     0x2A2A33
#define FRIJ_TEXT       0xF2F2F7  // primary text
#define FRIJ_TEXT_2     0xA1A1AA  // secondary / muted

// ---- Type (SF Pro Rounded, converted via lv_font_conv) --------------------
#define FRIJ_FONT_DISPLAY (&frij_sf_display)  // 26 Semibold
#define FRIJ_FONT_TITLE   (&frij_sf_title)    // 20 Semibold
#define FRIJ_FONT_BODY    (&frij_sf_body)     // 14 Regular
// Symbols (✓ ± +) aren't in the SF subset — symbol labels use this.
#define FRIJ_FONT_SYMBOL  (&lv_font_montserrat_14)

// ---- Spacing (4pt grid) ---------------------------------------------------
#define FRIJ_SP_XS  4
#define FRIJ_SP_S   8
#define FRIJ_SP_M   12
#define FRIJ_SP_L   16
#define FRIJ_SP_XL  20
#define FRIJ_SP_XXL 24

// ---- Radius ---------------------------------------------------------------
#define FRIJ_RADIUS_S    12
#define FRIJ_RADIUS_M    16
#define FRIJ_RADIUS_L    20
#define FRIJ_RADIUS_FULL LV_RADIUS_CIRCLE

// ---- Motion ---------------------------------------------------------------
#define FRIJ_ANIM_MS 220  // standard transition (200–250ms, ease-out)

#endif  // FRIJ_THEME_H
