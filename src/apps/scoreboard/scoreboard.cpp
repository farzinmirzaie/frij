#include "scoreboard.h"

#include "store/store.h"
#include "system/haptics.h"
#include "ui/anim.h"
#include "ui/components.h"
#include "ui/theme.h"

/*
 * Scoreboard — a two-player score keeper for board/card game nights. Each player
 * gets a big tappable card: tap to score +1, hold to take one back. The leader's
 * card lights up. Scores persist in the shared store (so they sync across the
 * cloud like the other apps) and survive leaving the app mid-game.
 *
 *   glance   : "Farzin  3 – 2  Farah" + who's leading
 *   screen 0 : full-screen left/right touch halves — tap a half +1, hold it -1;
 *              the header has a reset (confirm) action
 */

static const uint32_t ACCENT    = FRIJ_ACCENT;    // app glow/tint (blue)
static const uint32_t A_ACCENT  = FRIJ_ACCENT;    // player 1 (blue)
static const uint32_t B_ACCENT  = FRIJ_YELLOW;    // player 2 (amber)
static const char*    KEY_A     = "sb_a";
static const char*    KEY_B     = "sb_b";
static const char*    NAME_A    = "Farzin";
static const char*    NAME_B    = "Farah";

static int s_a = 0;
static int s_b = 0;

static void load_scores(void)
{
    s_a = frij_store_load_int(KEY_A, 0);
    s_b = frij_store_load_int(KEY_B, 0);
}

// ---- screen ----------------------------------------------------------------

typedef struct {
    lv_obj_t* card_a;
    lv_obj_t* card_b;
    lv_obj_t* val_a;
    lv_obj_t* val_b;
} sb_ctx_t;

static sb_ctx_t* s_active = NULL;  // the open screen's ctx (for the reset action)

static void pop(lv_obj_t* label)
{
    if (!label || !frij_anim_enabled()) {
        return;
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, label);
    lv_anim_set_exec_cb(&a, frij_anim_exec_scale);
    lv_anim_set_values(&a, 232, 256);  // 0.9 -> 1.0
    lv_anim_set_duration(&a, 160);
    lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
    lv_anim_start(&a);
}

static void refresh(sb_ctx_t* c)
{
    if (c->val_a) {
        lv_label_set_text_fmt(c->val_a, "%d", s_a);
    }
    if (c->val_b) {
        lv_label_set_text_fmt(c->val_b, "%d", s_b);
    }
}

static void on_card_tap(lv_event_t* e)
{
    sb_ctx_t* c    = (sb_ctx_t*)lv_event_get_user_data(e);
    lv_obj_t* card = (lv_obj_t*)lv_event_get_current_target(e);
    bool      is_a = (card == c->card_a);
    if (is_a) {
        s_a++;
    } else {
        s_b++;
    }
    refresh(c);
    pop(is_a ? c->val_a : c->val_b);
    frij_haptic(FRIJ_HAPTIC_SELECT);
    frij_store_save_int(is_a ? KEY_A : KEY_B, is_a ? s_a : s_b);
}

static void on_card_hold(lv_event_t* e)
{
    sb_ctx_t* c    = (sb_ctx_t*)lv_event_get_user_data(e);
    lv_obj_t* card = (lv_obj_t*)lv_event_get_current_target(e);
    bool      is_a = (card == c->card_a);
    int*      v    = is_a ? &s_a : &s_b;
    if (*v <= 0) {
        return;
    }
    (*v)--;
    refresh(c);
    pop(is_a ? c->val_a : c->val_b);
    frij_haptic(FRIJ_HAPTIC_TAP);
    frij_store_save_int(is_a ? KEY_A : KEY_B, *v);
}

static void on_delete(lv_event_t* e)
{
    sb_ctx_t* c = (sb_ctx_t*)lv_event_get_user_data(e);
    if (s_active == c) {
        s_active = NULL;  // the reset action can no longer touch freed halves
    }
    lv_free(c);
}

// A transparent, full-height touch half: name (top) + big score, color-coded.
static lv_obj_t* make_card(lv_obj_t* parent, const char* name, uint32_t accent, lv_obj_t** out_val,
                           lv_event_cb_t tap, lv_event_cb_t hold, void* user)
{
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_flex_grow(card, 1);         // share the width (left | right)
    lv_obj_set_height(card, LV_PCT(100));  // fill the height
    lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, LV_PART_MAIN);  // no panel — just a touch zone
    lv_obj_set_style_radius(card, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, FRIJ_SP_S, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    // press flash only (no permanent panel)
    lv_obj_set_style_bg_color(card, lv_color_hex(FRIJ_SURFACE_2), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_STATE_PRESSED);
    frij_haptic_attach(card);
    // +1 on a short tap (SHORT_CLICKED is suppressed after a long press, so the
    // hold's -1 isn't immediately undone by a +1 on release); -1 on a long press.
    lv_obj_add_event_cb(card, tap, LV_EVENT_SHORT_CLICKED, user);
    lv_obj_add_event_cb(card, hold, LV_EVENT_LONG_PRESSED, user);

    frij_label(card, name, FRIJ_FONT_BODY, FRIJ_TEXT_2);
    lv_obj_t* val = frij_label(card, "0", FRIJ_FONT_CLOCK, accent);
    lv_obj_set_style_transform_pivot_x(val, lv_pct(50), LV_PART_MAIN);  // pop from center
    lv_obj_set_style_transform_pivot_y(val, lv_pct(50), LV_PART_MAIN);
    *out_val = val;
    return card;
}

static void screen(lv_obj_t* parent, int index)
{
    (void)index;
    frij_store_pull_async(KEY_A);  // best-effort cloud refresh
    frij_store_pull_async(KEY_B);
    load_scores();

    sb_ctx_t* c = (sb_ctx_t*)lv_malloc(sizeof(sb_ctx_t));
    if (c == NULL) {
        return;
    }
    lv_memzero(c, sizeof(*c));

    // Full-bleed split: the page (the whole area below the header) becomes a row
    // of two transparent touch halves — no panels, no safe-area inset — with a
    // thin center divider + "VS" on top. frij_page_full_bleed tells the launcher
    // to skip its usual padding/centering so the halves fill edge to edge.
    lv_obj_t* root = parent;
    frij_page_full_bleed(root);
    lv_obj_set_style_pad_all(root, 0, LV_PART_MAIN);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    c->card_a = make_card(root, NAME_A, A_ACCENT, &c->val_a, on_card_tap, on_card_hold, c);
    c->card_b = make_card(root, NAME_B, B_ACCENT, &c->val_b, on_card_tap, on_card_hold, c);

    // center divider (overlay, ignores the flex row)
    lv_obj_t* divider = lv_obj_create(root);
    lv_obj_remove_style_all(divider);
    lv_obj_set_size(divider, 2, LV_PCT(60));
    lv_obj_add_flag(divider, LV_OBJ_FLAG_FLOATING);
    lv_obj_clear_flag(divider, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(divider, lv_color_hex(FRIJ_BORDER), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(divider, LV_OPA_60, LV_PART_MAIN);
    lv_obj_center(divider);

    // "VS" chip in the middle (masks the divider behind it; non-clickable so taps
    // fall through to whichever half is underneath)
    lv_obj_t* vs = lv_label_create(root);
    lv_label_set_text(vs, "VS");
    lv_obj_set_style_text_font(vs, FRIJ_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_style_text_color(vs, lv_color_hex(FRIJ_TEXT_2), LV_PART_MAIN);
    lv_obj_set_style_bg_color(vs, lv_color_hex(FRIJ_SURFACE_1), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(vs, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(vs, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(vs, FRIJ_SP_S, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(vs, FRIJ_SP_XS, LV_PART_MAIN);
    lv_obj_add_flag(vs, LV_OBJ_FLAG_FLOATING);
    lv_obj_clear_flag(vs, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(vs);

    s_active = c;
    refresh(c);
    frij_stagger_in(root, 60);
    lv_obj_add_event_cb(root, on_delete, LV_EVENT_DELETE, c);
}

// ---- glance ----------------------------------------------------------------

static void glance(lv_obj_t* parent)
{
    load_scores();
    lv_obj_t* col = frij_page(parent);

    frij_label(col, "Scoreboard", FRIJ_FONT_BODY, FRIJ_TEXT_2);

    // "3 – 2" with each number in its player color
    lv_obj_t* row = frij_col(col, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, FRIJ_SP_M, LV_PART_MAIN);
    lv_obj_t* a = frij_label(row, "", FRIJ_FONT_DISPLAY, A_ACCENT);
    lv_label_set_text_fmt(a, "%d", s_a);
    frij_label(row, "-", FRIJ_FONT_DISPLAY, FRIJ_TEXT_2);
    lv_obj_t* b = frij_label(row, "", FRIJ_FONT_DISPLAY, B_ACCENT);
    lv_label_set_text_fmt(b, "%d", s_b);

    char status[24];
    if (s_a == 0 && s_b == 0) {
        lv_snprintf(status, sizeof(status), "No scores yet");
    } else if (s_a > s_b) {
        lv_snprintf(status, sizeof(status), "%s leads", NAME_A);
    } else if (s_b > s_a) {
        lv_snprintf(status, sizeof(status), "%s leads", NAME_B);
    } else {
        lv_snprintf(status, sizeof(status), "Tied");
    }
    frij_label(col, status, FRIJ_FONT_BODY, FRIJ_TEXT_2);
}

// ---- reset (header action) -------------------------------------------------

static void do_reset(lv_event_t* e)
{
    (void)e;
    s_a = 0;
    s_b = 0;
    frij_store_save_int(KEY_A, 0);
    frij_store_save_int(KEY_B, 0);
    if (s_active) {
        refresh(s_active);  // update the visible halves in place
    }
    frij_haptic(FRIJ_HAPTIC_SUCCESS);
    frij_toast_status("Scores reset", true);
}

static const char* sb_action(int index)
{
    return index == 0 ? LV_SYMBOL_REFRESH : NULL;
}

static void sb_on_action(int index)
{
    if (index != 0) {
        return;
    }
    frij_prompt_screen(LV_SYMBOL_REFRESH, FRIJ_DANGER, "Reset scores?",
                       "Set both players back to zero.", "Reset", "Cancel", do_reset);
}

const frij_app_t* scoreboard_app(void)
{
    static const frij_app_t app = {"Scoreboard", ACCENT, glance, 1, screen, sb_action, sb_on_action};
    return &app;
}
