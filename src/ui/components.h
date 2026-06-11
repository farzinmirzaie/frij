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

// A restrained accent wash along the top edge that fades to nothing — a subtle
// per-app color cue behind a header. FLOATING (doesn't scroll or grow bounds).
lv_obj_t* frij_top_tint(lv_obj_t* parent, uint32_t accent);

// A vertical, centered flex column with `gap` px between children.
lv_obj_t* frij_col(lv_obj_t* parent, int gap);

// A centered column at 86% width — the standard page body for a round screen.
// Call frij_page_settle() after adding children to finalize the layout.
lv_obj_t* frij_page(lv_obj_t* parent);

// Lay a page out beneath a top bar `header_px` tall: adds breathing room under
// the bar and a matching bottom inset so centered content still lands at the
// screen's true center. A reusable "safe area" for header screens.
void frij_page_under_header(lv_obj_t* page, int header_px);

// Finalize a page once its children exist: center the content if it fits, or
// top-align it if it overflows (so the first row never hides above the scroll).
void frij_page_settle(lv_obj_t* page);

// Opt a page out of auto-centering: it stays top-aligned regardless of how much
// content it has (use for lists whose first row should never jump to center).
void frij_page_pin_top(lv_obj_t* page);

// Opt a page out of the header safe-area inset AND auto-centering entirely: the
// content owns the whole area below the header (use for full-bleed layouts like
// the Scoreboard's left/right split). Call before adding children.
void frij_page_full_bleed(lv_obj_t* page);

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

// A faint, gently-bobbing up-chevron near the bottom of a glance — hints that
// the card can be opened by swiping up. FLOATING (doesn't affect layout).
lv_obj_t* frij_swipe_hint(lv_obj_t* parent);

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
                          int value, uint32_t accent, const char* unit);

// A themed on/off switch. Attach your own LV_EVENT_VALUE_CHANGED handler.
lv_obj_t* frij_toggle(lv_obj_t* parent, bool on, uint32_t accent);

// A tappable card row: label + right chevron. Attach via the returned row's
// LV_EVENT_CLICKED. Use for actions like "Sync now" / "Reset".
lv_obj_t* frij_action_row(lv_obj_t* parent, const char* label, lv_event_cb_t on_click);

// A small muted, left-aligned heading for grouping rows on a page.
lv_obj_t* frij_section_label(lv_obj_t* parent, const char* text);

// A read-only row: `label` (left) + muted `value` (right). For info like
// Battery / Last sync.
lv_obj_t* frij_value_row(lv_obj_t* parent, const char* label, const char* value);

// A circular icon button with a press-pop + haptic. `bg`/`fg` are 0xRRGGBB,
// `symbol` is drawn in `font`. Pass NULL `on_click` for a static badge.
lv_obj_t* frij_circle_button(lv_obj_t* parent, int diameter, uint32_t bg, const char* symbol,
                             const lv_font_t* font, uint32_t fg, lv_event_cb_t on_click);

// A brief auto-dismissing message pill near the bottom of the screen (fades in,
// holds, fades out, removes itself). Use for transient confirmations.
void frij_toast(const char* text);

// Like frij_toast, but with a leading status glyph: a green check when `ok`,
// a red cross otherwise. Use for action results (sync done / failed).
void frij_toast_status(const char* text, bool ok);

// A modal confirmation dialog: a dimmed backdrop + a centered card with `title`,
// optional `message`, a "Cancel" button and an accent button labelled
// `confirm_text`. `on_confirm` fires when confirmed; the dialog closes itself
// either way (tapping the backdrop cancels). Use for destructive actions.
void frij_confirm(const char* title, const char* message, const char* confirm_text,
                  uint32_t accent, lv_event_cb_t on_confirm);

// Close the topmost open modal (confirm/sheet), if any. Returns true if one was
// closed — the launcher's Back action calls this before navigating.
bool frij_modal_close_top(void);

// Full-screen text-entry overlay: a title, a one-line textarea, and an on-screen
// keyboard. `password` masks the input; `numeric` shows a number pad instead of
// the full QWERTY (fewer, bigger keys — good for the round screen + PIN-style
// passwords). `cb` fires with the entered text when the user confirms (keyboard
// ✓); cancelling (✕) closes without calling `cb`. Closes itself either way.
typedef void (*frij_kb_cb)(const char* text, void* user);
void frij_keyboard_prompt(const char* title, bool password, bool numeric, frij_kb_cb cb, void* user);

// Callback for frij_action_sheet: `option` is the tapped option's index (0..n-1).
typedef void (*frij_sheet_cb)(int option, void* user);

// A modal action sheet: a dimmed backdrop + a centered card with `title` and a
// stacked list of `options` (the first is the accent/primary action) plus a
// "Cancel". `cb` fires with the chosen index; the sheet closes itself (tapping
// the backdrop or Cancel dismisses without calling `cb`).
void frij_action_sheet(const char* title, const char* const* options, int count, uint32_t accent,
                       frij_sheet_cb cb, void* user);

// Entrance/stagger animations now live in ui/anim.h.

#endif  // FRIJ_COMPONENTS_H
