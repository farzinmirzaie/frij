#include "components.h"

#include <stdint.h>

#include "anim.h"
#include "system/haptics.h"
#include "theme.h"

/*
 * Full-screen / floating surfaces that sit ABOVE the apps: the modal stack
 * (action sheet), the prompt/result screens, the numeric keypad, and the toast.
 * Split out of components.cpp purely for navigability — same component library,
 * declared in components.h.
 */

// ---- modals (shared) -------------------------------------------------------

// The currently-open modal (we never stack them), so the Back action can close
// it instead of navigating the launcher. Cleared when the modal is deleted.
static lv_obj_t* s_modal_top = NULL;

static void modal_clear_top_cb(lv_event_t* e)
{
    if (s_modal_top == lv_event_get_target(e)) {
        s_modal_top = NULL;
    }
}

static void modal_gone_cb(lv_anim_t* a)
{
    lv_obj_delete((lv_obj_t*)a->var);
}

// Fade the whole modal (dim + card) out, then delete. Guards double-close.
static void modal_close(lv_obj_t* modal)
{
    if (s_modal_top != modal) {
        return;  // already closing
    }
    s_modal_top = NULL;
    lv_obj_remove_flag(modal, LV_OBJ_FLAG_CLICKABLE);  // ignore taps mid-fade

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, modal);
    lv_anim_set_exec_cb(&a, frij_anim_exec_opa);  // overall opacity cascades to the card
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_duration(&a, FRIJ_ANIM_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&a, modal_gone_cb);
    lv_anim_start(&a);
}

bool frij_modal_close_top(void)
{
    if (s_modal_top) {
        modal_close(s_modal_top);
        return true;
    }
    return false;
}

// Tap on the backdrop itself (not a child) closes the modal.
static void modal_dismiss_cb(lv_event_t* e)
{
    if (lv_event_get_target(e) == lv_event_get_current_target(e)) {
        modal_close((lv_obj_t*)lv_event_get_user_data(e));
    }
}

// A dimmed, tap-to-dismiss backdrop on the active screen (above the launcher).
// The dim fades in; the caller adds a card via modal_card().
static lv_obj_t* modal_backdrop(void)
{
    lv_obj_t* modal = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(modal);
    lv_obj_set_size(modal, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(modal, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(modal, LV_OPA_TRANSP, LV_PART_MAIN);  // fades in below
    lv_obj_clear_flag(modal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(modal, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(modal, modal_dismiss_cb, LV_EVENT_CLICKED, modal);
    s_modal_top = modal;  // Back closes this first (see frij_modal_close_top)
    lv_obj_add_event_cb(modal, modal_clear_top_cb, LV_EVENT_DELETE, NULL);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, modal);
    lv_anim_set_exec_cb(&a, frij_anim_exec_bg_opa);
    lv_anim_set_values(&a, 0, LV_OPA_60);
    lv_anim_set_duration(&a, FRIJ_ANIM_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
    return modal;
}

// A centered Surface-2 card (flex column) that fades + rises in.
static lv_obj_t* modal_card(lv_obj_t* modal)
{
    lv_obj_t* card = lv_obj_create(modal);
    lv_obj_set_width(card, LV_PCT(74));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, lv_color_hex(FRIJ_SURFACE_2), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(card, FRIJ_RADIUS_L, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, FRIJ_SP_L, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, FRIJ_SP_M, LV_PART_MAIN);
    frij_anim_enter(card, 30);  // fade + rise entrance (no-op under reduce-motion)

    // …plus a subtle scale-in pop (pivot at the card's center)
    if (frij_anim_enabled()) {
        lv_obj_set_style_transform_pivot_x(card, lv_pct(50), LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_y(card, lv_pct(50), LV_PART_MAIN);
        lv_obj_set_style_transform_scale(card, 236, LV_PART_MAIN);  // ~0.92
        lv_anim_t pop;
        lv_anim_init(&pop);
        lv_anim_set_var(&pop, card);
        lv_anim_set_exec_cb(&pop, frij_anim_exec_scale);
        lv_anim_set_values(&pop, 236, 256);
        lv_anim_set_duration(&pop, FRIJ_ANIM_MS);
        lv_anim_set_delay(&pop, 30);
        lv_anim_set_path_cb(&pop, lv_anim_path_ease_out);
        lv_anim_start(&pop);
    }
    return card;
}

static lv_obj_t* pill_button(lv_obj_t* parent, const char* text, uint32_t bg, uint32_t fg,
                             lv_event_cb_t cb, void* user)
{
    lv_obj_t* b = lv_button_create(parent);
    lv_obj_set_height(b, 44);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, lv_color_hex(bg), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
    // The default theme grows buttons on press; that pokes past the card's clip.
    // Neutralize it and use a subtle dim instead.
    lv_obj_set_style_transform_width(b, 0, LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(b, 0, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(b, LV_OPA_80, LV_STATE_PRESSED);
    frij_haptic_attach(b);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);

    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, FRIJ_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(fg), LV_PART_MAIN);
    lv_obj_center(l);
    return b;
}

// Generic "close my overlay" button handler (user_data = the overlay).
static void confirm_cancel_cb(lv_event_t* e)
{
    modal_close((lv_obj_t*)lv_event_get_user_data(e));
}

// ---- action sheet ----------------------------------------------------------

typedef struct {
    frij_sheet_cb cb;
    void*         user;
} sheet_ctx_t;

static void sheet_free_cb(lv_event_t* e)
{
    lv_free(lv_event_get_user_data(e));  // ctx outlives the build call
}

static void sheet_cancel_cb(lv_event_t* e)
{
    modal_close((lv_obj_t*)lv_event_get_user_data(e));
}

static void sheet_option_cb(lv_event_t* e)
{
    lv_obj_t*     btn   = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t*     modal = (lv_obj_t*)lv_event_get_user_data(e);
    sheet_ctx_t*  c     = (sheet_ctx_t*)lv_obj_get_user_data(modal);
    int           idx   = (int)(intptr_t)lv_obj_get_user_data(btn);
    frij_sheet_cb cb    = c->cb;  // copy out before the modal (and ctx) are freed
    void*         user  = c->user;
    modal_close(modal);  // ctx is freed when the fade-out completes (sheet_free_cb)
    if (cb) {
        cb(idx, user);
    }
}

void frij_action_sheet(const char* title, const char* const* options, int count, uint32_t accent,
                       frij_sheet_cb cb, void* user)
{
    sheet_ctx_t* c = (sheet_ctx_t*)lv_malloc(sizeof(sheet_ctx_t));
    if (c == NULL) {
        return;  // OOM — don't open a sheet we can't track
    }
    c->cb   = cb;
    c->user = user;

    lv_obj_t* modal = modal_backdrop();
    lv_obj_set_user_data(modal, c);
    lv_obj_add_event_cb(modal, sheet_free_cb, LV_EVENT_DELETE, c);

    lv_obj_t* card = modal_card(modal);
    if (title && title[0]) {
        lv_obj_t* t = frij_label(card, title, FRIJ_FONT_TITLE, FRIJ_TEXT);
        lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }

    for (int i = 0; i < count; i++) {
        // first option is the primary (accent); the rest are neutral surfaces
        uint32_t  bg = (i == 0) ? accent : FRIJ_SURFACE_3;
        uint32_t  fg = (i == 0) ? 0xFFFFFF : FRIJ_TEXT;
        lv_obj_t* b  = pill_button(card, options[i], bg, fg, sheet_option_cb, modal);
        lv_obj_set_flex_grow(b, 0);          // stack at natural height in the column
        lv_obj_set_width(b, LV_PCT(100));
        lv_obj_set_user_data(b, (void*)(intptr_t)i);  // option index for the callback
    }

    lv_obj_t* cancel = pill_button(card, "Cancel", FRIJ_SURFACE_2, FRIJ_TEXT_2, sheet_cancel_cb, modal);
    lv_obj_set_flex_grow(cancel, 0);
    lv_obj_set_width(cancel, LV_PCT(100));
}

// Fade a full-screen overlay's background in — without this the screen snaps
// to black while the children animate. (Children fade themselves.)
static void overlay_fade_in(lv_obj_t* overlay)
{
    if (!frij_anim_enabled()) {
        return;
    }
    lv_obj_set_style_bg_opa(overlay, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, overlay);
    lv_anim_set_exec_cb(&a, frij_anim_exec_bg_opa);
    lv_anim_set_values(&a, 0, LV_OPA_COVER);
    lv_anim_set_duration(&a, FRIJ_ANIM_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

// ---- numeric keypad (our own, round-screen friendly) -----------------------

#define FRIJ_NUMPAD_MAX 32

typedef struct {
    char       buf[FRIJ_NUMPAD_MAX + 1];
    int        len;
    int        prev_len;  // to know whether a digit was added (pop the new dot)
    bool       repeated;  // a hold-repeat ran; swallow the release's CLICKED
    lv_obj_t*  dots;      // masked-value display (one dot per entered digit)
    frij_kb_cb cb;
    void*      user;
} numpad_ctx_t;

// Special key codes stored in a button's user_data (digits store their char).
#define NUMPAD_BACKSPACE (-1)
#define NUMPAD_OK        (-2)

static void numpad_free_cb(lv_event_t* e)
{
    lv_free(lv_event_get_user_data(e));
}

// Rebuild the row of dots to match the current length (capped so a long code
// can't overflow the row — entry keeps counting past the visual cap).
static void numpad_sync_dots(numpad_ctx_t* c)
{
    int shown = c->len > 14 ? 14 : c->len;
    lv_obj_clean(c->dots);
    for (int i = 0; i < shown; i++) {
        lv_obj_t* d = lv_obj_create(c->dots);
        lv_obj_remove_style_all(d);
        lv_obj_set_size(d, 12, 12);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(d, lv_color_hex(FRIJ_TEXT), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, LV_PART_MAIN);
        // the newly-added dot pops in
        if (i == shown - 1 && c->len > c->prev_len && frij_anim_enabled()) {
            lv_obj_set_style_transform_pivot_x(d, 6, LV_PART_MAIN);
            lv_obj_set_style_transform_pivot_y(d, 6, LV_PART_MAIN);
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, d);
            lv_anim_set_exec_cb(&a, frij_anim_exec_scale);
            lv_anim_set_values(&a, 120, 256);
            lv_anim_set_duration(&a, 140);
            lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
            lv_anim_start(&a);
        }
    }
    c->prev_len = c->len;
}

static void numpad_key_cb(lv_event_t* e)
{
    lv_obj_t*     btn     = (lv_obj_t*)lv_event_get_current_target(e);
    lv_obj_t*     overlay = (lv_obj_t*)lv_event_get_user_data(e);
    numpad_ctx_t* c       = (numpad_ctx_t*)lv_obj_get_user_data(overlay);
    int           code    = (int)(intptr_t)lv_obj_get_user_data(btn);
    bool          repeat  = lv_event_get_code(e) == LV_EVENT_LONG_PRESSED_REPEAT;

    if (code == NUMPAD_OK) {
        if (c->len == 0) {
            frij_haptic(FRIJ_HAPTIC_TAP);  // nothing typed — nudge, don't submit ""
            return;
        }
        char out[FRIJ_NUMPAD_MAX + 1];  // copy out before the close frees ctx
        c->buf[c->len] = '\0';
        lv_memcpy(out, c->buf, (size_t)c->len + 1);
        frij_kb_cb cb = c->cb;
        void*      u  = c->user;
        modal_close(overlay);  // fade out; ctx freed on delete (numpad_free_cb)
        if (cb) {
            cb(out, u);
        }
        return;
    }
    if (code == NUMPAD_BACKSPACE) {
        if (repeat) {
            c->repeated = true;  // swallow the CLICKED that fires on release
        } else if (c->repeated) {
            c->repeated = false;
            return;
        }
        if (c->len > 0) {
            c->buf[--c->len] = '\0';
            numpad_sync_dots(c);
        }
        return;
    }
    // a digit
    if (c->len < FRIJ_NUMPAD_MAX) {
        c->buf[c->len++] = (char)code;
        numpad_sync_dots(c);
    }
}

// One round key built on frij_circle_button; `code` is the digit char or a
// NUMPAD_* sentinel, stashed in user_data for numpad_key_cb.
static lv_obj_t* numpad_key(lv_obj_t* grid, lv_obj_t* overlay, const char* label, int code,
                            uint32_t bg, uint32_t fg, const lv_font_t* font)
{
    lv_obj_t* b = frij_circle_button(grid, 66, bg, label, font, fg, NULL);
    lv_obj_set_user_data(b, (void*)(intptr_t)code);
    lv_obj_add_event_cb(b, numpad_key_cb, LV_EVENT_CLICKED, overlay);
    return b;
}

void frij_numpad_prompt(const char* title, frij_kb_cb cb, void* user)
{
    numpad_ctx_t* c = (numpad_ctx_t*)lv_malloc(sizeof(numpad_ctx_t));
    if (c == NULL) {
        return;
    }
    lv_memzero(c, sizeof(*c));
    c->cb   = cb;
    c->user = user;

    // full-screen overlay (above launcher + header), themed like the rest
    lv_obj_t* overlay = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    frij_apply_bg(overlay);  // the standard page wash
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(overlay, c);
    lv_obj_add_event_cb(overlay, numpad_free_cb, LV_EVENT_DELETE, c);
    overlay_fade_in(overlay);
    // Register with the modal stack so the Back action cancels the keypad.
    s_modal_top = overlay;
    lv_obj_add_event_cb(overlay, modal_clear_top_cb, LV_EVENT_DELETE, NULL);

    // vertical stack: title · dots · keypad — centered, so it stays in the circle
    lv_obj_t* col = frij_col(overlay, FRIJ_SP_M);
    lv_obj_center(col);
    lv_obj_set_height(col, LV_SIZE_CONTENT);

    lv_obj_t* t = frij_label(col, title && title[0] ? title : "Enter code", FRIJ_FONT_BODY,
                             FRIJ_TEXT_2);
    lv_obj_set_width(t, LV_PCT(80));
    lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);  // e.g. "Enter password for\n<ssid>"
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    // masked value (dots). Keep a fixed min height so the layout doesn't jump.
    c->dots = lv_obj_create(col);
    lv_obj_remove_style_all(c->dots);
    lv_obj_set_size(c->dots, LV_PCT(80), 16);
    lv_obj_set_flex_flow(c->dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(c->dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(c->dots, FRIJ_SP_S, LV_PART_MAIN);
    lv_obj_clear_flag(c->dots, LV_OBJ_FLAG_SCROLLABLE);

    // 3×4 grid of round keys (flex-wrap at 3 across)
    lv_obj_t* grid = lv_obj_create(col);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, 66 * 3 + FRIJ_SP_S * 2, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(grid, FRIJ_SP_S, LV_PART_MAIN);
    lv_obj_set_style_pad_column(grid, FRIJ_SP_S, LV_PART_MAIN);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    for (int d = 1; d <= 9; d++) {
        char lbl[2] = {(char)('0' + d), '\0'};
        numpad_key(grid, overlay, lbl, '0' + d, FRIJ_SURFACE_2, FRIJ_TEXT, FRIJ_FONT_TITLE);
    }
    lv_obj_t* bs = numpad_key(grid, overlay, LV_SYMBOL_BACKSPACE, NUMPAD_BACKSPACE, FRIJ_SURFACE_2,
                              FRIJ_TEXT_2, FRIJ_FONT_SYMBOL);
    lv_obj_add_event_cb(bs, numpad_key_cb, LV_EVENT_LONG_PRESSED_REPEAT, overlay);  // hold to clear
    numpad_key(grid, overlay, "0", '0', FRIJ_SURFACE_2, FRIJ_TEXT, FRIJ_FONT_TITLE);
    numpad_key(grid, overlay, LV_SYMBOL_OK, NUMPAD_OK, FRIJ_SECONDARY, 0xFFFFFF, FRIJ_FONT_SYMBOL);
}

// ---- full-screen prompt / result (ring symbol + message + actions) ----------

// The primary button fires the caller's handler (stored on the overlay), then
// the screen closes.
static void prompt_primary_cb(lv_event_t* e)
{
    lv_obj_t*     overlay = (lv_obj_t*)lv_event_get_user_data(e);
    lv_event_cb_t cb      = (lv_event_cb_t)lv_obj_get_user_data(overlay);
    modal_close(overlay);
    if (cb) {
        cb(e);
    }
}

// Shared builder for frij_result_screen / frij_prompt_screen: a full-screen
// overlay with a big colored ring (2x-scaled symbol glyph), a title in the same
// color, an optional message, and one or two pill actions. Registered with the
// modal stack, so Back dismisses without firing the primary.
static void prompt_screen(const char* symbol, uint32_t color, const char* title,
                          const char* message, const char* primary_text, const char* cancel_text,
                          lv_event_cb_t on_primary)
{
    lv_obj_t* overlay = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    frij_apply_bg(overlay);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(overlay, (void*)on_primary);  // read by prompt_primary_cb
    overlay_fade_in(overlay);
    s_modal_top = overlay;
    lv_obj_add_event_cb(overlay, modal_clear_top_cb, LV_EVENT_DELETE, NULL);

    lv_obj_t* col = frij_col(overlay, FRIJ_SP_M);
    lv_obj_center(col);
    lv_obj_set_height(col, LV_SIZE_CONTENT);

    lv_obj_t* ring = lv_obj_create(col);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, 88, 88);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(ring, 3, LV_PART_MAIN);
    lv_obj_set_style_border_color(ring, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ring, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ring, LV_OPA_20, LV_PART_MAIN);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
    if (frij_anim_enabled()) {  // the ring pops in with a small overshoot
        lv_obj_set_style_transform_pivot_x(ring, lv_pct(50), LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_y(ring, lv_pct(50), LV_PART_MAIN);
        lv_anim_t pop;
        lv_anim_init(&pop);
        lv_anim_set_var(&pop, ring);
        lv_anim_set_exec_cb(&pop, frij_anim_exec_scale);
        lv_anim_set_values(&pop, 190, 256);
        lv_anim_set_duration(&pop, 320);
        lv_anim_set_path_cb(&pop, lv_anim_path_overshoot);
        lv_anim_start(&pop);
    }

    lv_obj_t* glyph = lv_label_create(ring);
    lv_label_set_text(glyph, symbol);
    lv_obj_set_style_text_font(glyph, FRIJ_FONT_SYMBOL_L, LV_PART_MAIN);  // native 40px, crisp
    lv_obj_set_style_text_color(glyph, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_center(glyph);

    lv_obj_t* t = frij_label(col, title, FRIJ_FONT_TITLE, color);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    if (message && message[0]) {
        lv_obj_t* m = frij_label(col, message, FRIJ_FONT_BODY, FRIJ_TEXT_2);
        lv_obj_set_width(m, LV_PCT(70));
        lv_label_set_long_mode(m, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(m, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }

    if (cancel_text && cancel_text[0]) {  // two actions: round ✕ (cancel) | ✓ (primary)
        lv_obj_t* btns = lv_obj_create(col);
        lv_obj_remove_style_all(btns);
        lv_obj_set_size(btns, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(btns, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btns, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(btns, FRIJ_SP_XXL, LV_PART_MAIN);
        lv_obj_set_style_margin_top(btns, FRIJ_SP_M, LV_PART_MAIN);
        lv_obj_clear_flag(btns, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* no = frij_circle_button(btns, 60, FRIJ_SURFACE_2, LV_SYMBOL_CLOSE,
                                          FRIJ_FONT_SYMBOL, FRIJ_TEXT_2, NULL);
        lv_obj_add_event_cb(no, confirm_cancel_cb, LV_EVENT_CLICKED, overlay);
        lv_obj_t* yes = frij_circle_button(btns, 60, color, LV_SYMBOL_OK, FRIJ_FONT_SYMBOL,
                                           0xFFFFFF, NULL);
        lv_obj_add_event_cb(yes, prompt_primary_cb, LV_EVENT_CLICKED, overlay);
    } else {  // single action: just dismiss
        lv_obj_t* btn = pill_button(col, primary_text ? primary_text : "Done", FRIJ_SURFACE_3,
                                    FRIJ_TEXT, prompt_primary_cb, overlay);
        lv_obj_set_flex_grow(btn, 0);
        lv_obj_set_width(btn, 150);
        lv_obj_set_style_margin_top(btn, FRIJ_SP_M, LV_PART_MAIN);
    }

    frij_anim_enter(col, 0);  // fade + rise in (no-op under reduce-motion)
}

void frij_result_screen(bool ok, const char* title, const char* subtitle, const char* button_text)
{
    prompt_screen(ok ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE, ok ? FRIJ_SECONDARY : FRIJ_DANGER, title,
                  subtitle, button_text, NULL, NULL);
}

void frij_prompt_screen(const char* symbol, uint32_t primary_color, const char* title,
                        const char* message, const char* primary_text, const char* cancel_text,
                        lv_event_cb_t on_primary)
{
    prompt_screen(symbol, primary_color, title, message, primary_text, cancel_text, on_primary);
}

// ---- toast (auto-dismissing snackbar) --------------------------------------

static lv_obj_t* s_toast = NULL;  // at most one on screen at a time

static void toast_gone_cb(lv_anim_t* a)
{
    lv_obj_t* t = (lv_obj_t*)a->var;
    if (s_toast == t) {
        s_toast = NULL;
    }
    lv_obj_delete(t);
}

// Hold, then fade out + delete. Sequenced after the fade-in so the two opacity
// animations don't fight over the same property (which left the toast hidden).
static void toast_in_done_cb(lv_anim_t* a)
{
    lv_anim_t out;
    lv_anim_init(&out);
    lv_anim_set_var(&out, a->var);
    lv_anim_set_exec_cb(&out, frij_anim_exec_opa);
    lv_anim_set_values(&out, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_duration(&out, FRIJ_ANIM_MS);
    lv_anim_set_delay(&out, 1500);  // hold visible before fading
    lv_anim_set_completed_cb(&out, toast_gone_cb);
    lv_anim_start(&out);
}

// Tap a toast to dismiss it early: cancel its in/hold anims and fade out now.
static void toast_tap_cb(lv_event_t* e)
{
    lv_obj_t* t = (lv_obj_t*)lv_event_get_target(e);
    lv_anim_delete(t, NULL);  // drop the pending fade-in / hold / rise
    lv_anim_t out;
    lv_anim_init(&out);
    lv_anim_set_var(&out, t);
    lv_anim_set_exec_cb(&out, frij_anim_exec_opa);
    lv_anim_set_values(&out, lv_obj_get_style_opa(t, LV_PART_MAIN), LV_OPA_TRANSP);
    lv_anim_set_duration(&out, FRIJ_ANIM_MS / 2);
    lv_anim_set_completed_cb(&out, toast_gone_cb);
    lv_anim_start(&out);
}

// Shared builder: a pill with an optional leading status glyph + the message.
// `border` adds a hairline tint in the glyph color (used for failures only —
// success reads fine from the green check alone).
static void toast_show(const char* glyph, uint32_t glyph_color, const char* text, bool border)
{
    if (s_toast) {
        lv_obj_delete(s_toast);  // also cancels its anims (LVGL drops anims on delete)
    }
    lv_obj_t* t = lv_obj_create(lv_screen_active());
    s_toast     = t;
    lv_obj_remove_style_all(t);
    lv_obj_set_size(t, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(t, lv_color_hex(FRIJ_SURFACE_3), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(t, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    if (border && glyph && glyph[0]) {  // failure toasts get a hairline tint
        lv_obj_set_style_border_width(t, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(t, lv_color_hex(glyph_color), LV_PART_MAIN);
        lv_obj_set_style_border_opa(t, LV_OPA_50, LV_PART_MAIN);
    }
    lv_obj_set_style_pad_left(t, FRIJ_SP_L, LV_PART_MAIN);
    lv_obj_set_style_pad_right(t, FRIJ_SP_L, LV_PART_MAIN);
    lv_obj_set_style_pad_top(t, FRIJ_SP_S, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(t, FRIJ_SP_S, LV_PART_MAIN);
    lv_obj_clear_flag(t, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);  // tap to dismiss early
    lv_obj_add_event_cb(t, toast_tap_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(t, LV_ALIGN_BOTTOM_MID, 0, -frij_screen_min() * 14 / 100);
    // row: [glyph] text — centered, with a small gap when a glyph is present
    lv_obj_set_flex_flow(t, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(t, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(t, FRIJ_SP_S, LV_PART_MAIN);

    if (glyph && glyph[0]) {
        frij_label(t, glyph, FRIJ_FONT_SYMBOL, glyph_color);
    }
    lv_obj_t* l = frij_label(t, text, FRIJ_FONT_BODY, FRIJ_TEXT);
    // cap the label width so long messages wrap instead of pushing the pill off
    // the round edge; the pill (content-sized) then grows to the wrapped text
    lv_obj_set_style_max_width(l, frij_screen_min() * 64 / 100, LV_PART_MAIN);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    // Fade in; toast_in_done_cb then holds and fades it back out.
    lv_anim_t in;
    lv_anim_init(&in);
    lv_anim_set_var(&in, t);
    lv_anim_set_exec_cb(&in, frij_anim_exec_opa);
    lv_anim_set_values(&in, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&in, FRIJ_ANIM_MS);
    lv_anim_set_path_cb(&in, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&in, toast_in_done_cb);
    lv_anim_start(&in);

    // rise into place (snackbar feel) — skipped under reduce-motion
    if (frij_anim_enabled()) {
        lv_obj_set_style_translate_y(t, 16, LV_PART_MAIN);
        lv_anim_t rise;
        lv_anim_init(&rise);
        lv_anim_set_var(&rise, t);
        lv_anim_set_exec_cb(&rise, frij_anim_exec_translate_y);
        lv_anim_set_values(&rise, 16, 0);
        lv_anim_set_duration(&rise, FRIJ_ANIM_MS);
        lv_anim_set_path_cb(&rise, lv_anim_path_ease_out);
        lv_anim_start(&rise);
    }
}

void frij_toast(const char* text)
{
    toast_show(NULL, 0, text, false);
}

void frij_toast_status(const char* text, bool ok)
{
    toast_show(ok ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE,
               ok ? FRIJ_SECONDARY : FRIJ_DANGER, text, !ok);
}
