#ifndef FRIJ_COMPONENTS_H
#define FRIJ_COMPONENTS_H

#include "lvgl.h"

/*
 * Reusable, app-agnostic widgets styled with the Frij theme (see theme.h).
 * Each takes a parent (and plain data) — no app specifics.
 */

// Shorter side of the active display, in px. Use to size things responsively.
int frij_screen_min(void);

// Height reserved at the top for the shared app header (the area content sits
// below). One source of truth for the launcher and the snapshot harness.
int frij_header_zone(void);

// Give any object a light haptic tap when pressed (used by the components;
// apps can call it on their own custom widgets).
void frij_haptic_attach(lv_obj_t* obj);

// Apply the standard page background (pure black — AMOLED-friendly) to an
// object's MAIN part.
void frij_apply_bg(lv_obj_t* obj);

// A top-layer ring in `color` whose hole is the round panel: hides the square
// window's corners above ALL content (overlays too), so the emulator always
// shows the true round display. Emulator/snapshot only — pass lv_layer_sys()
// (live) or the screen (snapshot, added last). Never created on the device.
lv_obj_t* frij_round_mask(lv_obj_t* parent, uint32_t color);

// Add a soft accent glow (centered halo). It's a FLOATING background (doesn't
// scroll or affect the parent's scroll area). Returns the object so the caller
// can reposition it (e.g. behind a header). `accent` is 0xRRGGBB.
lv_obj_t* frij_glow(lv_obj_t* parent, uint32_t accent);

// A horizontal strip fading the background color to transparent downward. Place
// it just below a header (full width, `top_px` from the top): rows scrolling up
// fade into the black header zone instead of clipping at a hard line.
lv_obj_t* frij_header_fade(lv_obj_t* parent, int top_px);

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

// Like frij_page_settle, but for IN-PLACE rebuilds: restores `scroll_y`
// (captured with lv_obj_get_scroll_y before the rebuild) instead of jumping
// back to the top — refreshes shouldn't lose the user's place.
void frij_page_settle_at(lv_obj_t* page, int32_t scroll_y);

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

// A text-only "nothing here" placeholder: `title` in primary white over a
// fainter `subtitle` hint (NULL to omit) saying how the screen gets filled.
lv_obj_t* frij_empty_state(lv_obj_t* parent, const char* title, const char* subtitle);

// A faint, gently-bobbing up-chevron near the bottom of a glance — hints that
// the card can be opened by swiping up. FLOATING (doesn't affect layout).
lv_obj_t* frij_swipe_hint(lv_obj_t* parent);

// The Frij logo: a three-circle clover (pink/purple/violet), `size` px tall,
// plus the "Frij" wordmark when `with_name`. Use anywhere branding is wanted.
lv_obj_t* frij_logo(lv_obj_t* parent, int size, bool with_name);

// A small padlock glyph (drawn, not a font symbol — the symbol font has none).
// Muted color; for "secured" markers like Wi-Fi rows.
lv_obj_t* frij_lock_icon(lv_obj_t* parent);

// A full-screen result overlay: a big ring with a check (ok) or cross, a title,
// an optional subtitle, and a button that dismisses it. The Back action also
// closes it. Use to conclude a flow (e.g. Wi-Fi joined: "Connected · Done").
void frij_result_screen(bool ok, const char* title, const char* subtitle, const char* button_text);

// A shared app header near the top: a back button (left, returns to the
// launcher), a centered `title`, and a right action button (hidden until set).
// Title + icons take the app's `accent` color; the background stays the dark
// base. `action_cb` fires when the action is tapped. Round-screen-safe.
lv_obj_t* frij_header(lv_obj_t* parent, const char* title, uint32_t accent,
                      lv_event_cb_t action_cb);

// Show/hide + set the icon of the header's action button (NULL/"" hides it).
void frij_header_set_action(lv_obj_t* header, const char* symbol);

// A slider styled as a full-width card: the whole card is draggable left/right
// and an accent fill shows the amount; `label` sits on top. Returns the slider
// (an lv_slider) — attach LV_EVENT_VALUE_CHANGED and read lv_slider_get_value.
lv_obj_t* frij_slider_row(lv_obj_t* parent, const char* label, int min, int max,
                          int value, uint32_t accent, const char* unit);

// A themed on/off switch. Attach your own LV_EVENT_VALUE_CHANGED handler.
lv_obj_t* frij_toggle(lv_obj_t* parent, bool on, uint32_t accent);

// A full row with a label + switch where tapping ANYWHERE on the row flips it.
// Returns the switch — attach your LV_EVENT_VALUE_CHANGED handler to that.
lv_obj_t* frij_toggle_row(lv_obj_t* parent, const char* label, bool on, uint32_t accent);

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

// A full-screen prompt in the result-screen style: a big `primary_color` ring
// with `symbol`, a title, an optional message, and one or two pill actions.
// `primary_color` is usually the app accent — or FRIJ_DANGER for destructive
// flows. With `cancel_text` NULL it's a single-action notice; otherwise the
// secondary (and Back) dismiss without firing `on_primary`. Closes itself.
void frij_prompt_screen(const char* symbol, uint32_t primary_color, const char* title,
                        const char* message, const char* primary_text, const char* cancel_text,
                        lv_event_cb_t on_primary);

// Close the topmost open modal (confirm/sheet), if any. Returns true if one was
// closed — the launcher's Back action calls this before navigating.
bool frij_modal_close_top(void);

// Register a custom full-screen overlay with the modal system: Back closes it
// (fade-out + delete) before navigating. For app-built overlays that aren't
// one of the canned prompt/sheet/numpad shapes.
void frij_modal_register(lv_obj_t* overlay);

// An equalizer-style "voice" indicator: four rounded bars bobbing at staggered
// rhythms (static staircase under reduce-motion). `h` is the tallest bar.
lv_obj_t* frij_sound_bars(lv_obj_t* parent, int h, uint32_t color);

// A soft halo hugging the screen edge (transparent center -> faint `color` at
// the rim). Static; animate its opacity for a breathing effect. FLOATING.
lv_obj_t* frij_edge_glow(lv_obj_t* parent, uint32_t color);

// A "live" indicator: concentric rings that ripple outward from the center,
// `size` px, in `color`. Put your icon/content inside (it's a plain container)
// and lv_obj_center it. Static single ring under reduce-motion.
lv_obj_t* frij_pulse_ring(lv_obj_t* parent, int size, uint32_t color);

// A full-screen numeric keypad (our own — themed + round-screen friendly): a
// title (wraps to two lines), a masked dots display, and a 3×4 grid of round
// keys (1–9, ⌫ backspace with hold-to-repeat, 0, ✓ confirm). `cb` fires with
// the entered digits on ✓; the Back action cancels without calling `cb`.
// Closes itself either way. Used for the Wi-Fi (numeric) password.
typedef void (*frij_kb_cb)(const char* text, void* user);
void frij_numpad_prompt(const char* title, frij_kb_cb cb, void* user);

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
