#ifndef FRIJ_COMPONENTS_H
#define FRIJ_COMPONENTS_H

#include "lvgl.h"

/*
 * Reusable, app-agnostic widgets styled with the Frij theme (see theme.h).
 * Each takes a parent (and plain data) — no app specifics.
 */

// Shorter side of the active display, in px. Use to size things responsively.
int frij_screen_min(void);

// Give any object a light haptic tap when pressed (used by the components;
// apps can call it on their own custom widgets).
void frij_haptic_attach(lv_obj_t* obj);

// Apply the standard subtle page-background gradient to an object's MAIN part.
void frij_apply_bg(lv_obj_t* obj);

// Add a soft accent glow (centered halo). It's a FLOATING background (doesn't
// scroll or affect the parent's scroll area). Returns the object so the caller
// can reposition it (e.g. behind a header). `accent` is 0xRRGGBB.
lv_obj_t* frij_glow(lv_obj_t* parent, uint32_t accent);

// A vertical, centered flex column with `gap` px between children.
lv_obj_t* frij_col(lv_obj_t* parent, int gap);

// A centered column at 86% width — the standard page body for a round screen.
lv_obj_t* frij_page(lv_obj_t* parent);

// A label using a theme font + color (0xRRGGBB).
lv_obj_t* frij_label(lv_obj_t* parent, const char* text, const lv_font_t* font, uint32_t color);

// A rounded Surface-2 row (flex row, padded) with press feedback. Make it
// clickable and attach your own LV_EVENT_CLICKED handler.
lv_obj_t* frij_surface_row(lv_obj_t* parent);

// A circular check. `accent` (0xRRGGBB) is the filled color when checked.
lv_obj_t* frij_check(lv_obj_t* parent, bool checked, uint32_t accent);
// Update a check's state; `animate` does a small pop.
void      frij_check_set(lv_obj_t* check, bool checked, bool animate);

// A thin progress ring (lv_arc) at `pct` (0–100), `size` px, `accent` color.
lv_obj_t* frij_progress_ring(lv_obj_t* parent, int size, int pct, uint32_t accent);

// A round "nothing here" placeholder with an icon + text.
lv_obj_t* frij_empty_state(lv_obj_t* parent, const char* text);

// A shared app header near the top: a back button (left, returns to the
// launcher), a centered `title`, and a right action button (hidden until set).
// `action_cb` fires when the action is tapped. Round-screen-safe placement.
lv_obj_t* frij_header(lv_obj_t* parent, const char* title, lv_event_cb_t action_cb);

// Show/hide + set the icon of the header's action button (NULL/"" hides it).
void frij_header_set_action(lv_obj_t* header, const char* symbol);

// A slider styled as a full-width card: the whole card is draggable left/right
// and an accent fill shows the amount; `label` sits on top. Returns the slider
// (an lv_slider) — attach LV_EVENT_VALUE_CHANGED and read lv_slider_get_value.
lv_obj_t* frij_slider_row(lv_obj_t* parent, const char* label, int min, int max,
                          int value, uint32_t accent);

// A themed on/off switch. Attach your own LV_EVENT_VALUE_CHANGED handler.
lv_obj_t* frij_toggle(lv_obj_t* parent, bool on, uint32_t accent);

// A tappable card row: label + right chevron. Attach via the returned row's
// LV_EVENT_CLICKED. Use for actions like "Sync now" / "Reset".
lv_obj_t* frij_action_row(lv_obj_t* parent, const char* label, lv_event_cb_t on_click);

// A modal confirmation dialog: a dimmed backdrop + a centered card with `title`,
// optional `message`, a "Cancel" button and an accent button labelled
// `confirm_text`. `on_confirm` fires when confirmed; the dialog closes itself
// either way (tapping the backdrop cancels). Use for destructive actions.
void frij_confirm(const char* title, const char* message, const char* confirm_text,
                  uint32_t accent, lv_event_cb_t on_confirm);

// Entrance animation: fade in + rise. `delay_ms` lets callers stagger a list.
void frij_anim_enter(lv_obj_t* obj, uint32_t delay_ms);

#endif  // FRIJ_COMPONENTS_H
