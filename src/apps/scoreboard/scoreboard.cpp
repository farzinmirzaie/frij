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
 *   screen 1 : "who goes first?" — tap to spin a slot reel of the two players
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
    // Let a horizontal drag that starts on the card bubble up to the launcher so
    // it can page between scoreboard screens. Without this the card swallows the
    // gesture (it's a full-height touch zone) and the swipe does nothing. The tap
    // (+1) / long-press (-1) still fire locally.
    lv_obj_add_flag(card, LV_OBJ_FLAG_EVENT_BUBBLE);
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

// ---- "who goes first?" — slot reel (screen 1) ------------------------------
// Game-night helper: tap to spin a vertical reel of the two players' names; it
// scroll-spins for many turns behind a fading window and eases to a stop on a
// random winner. Cheap on the panel — just a column moved by its y offset.

static const char* const FIRST_NAMES[2]    = {NAME_A, NAME_B};
static const uint32_t     FIRST_ACCENTS[2] = {A_ACCENT, B_ACCENT};

#define REEL_H    46          // one slot height (a touch under FRIJ_ROW_H so the
                              // 5-slot window + title + hint all fit on the round)
#define REEL_VIEW 5           // slots visible (centre = the pick; 2 faded each side)
#define REEL_ROWS 34          // names in the strip (alternating A/B), long enough
                              // for a 10+ turn spin without running off the end
#define REEL_MIN_TRAVEL 20    // rows the reel scrolls per spin (>= 10 A/B cycles)

typedef struct {
    lv_obj_t* inner;   // the scrolling strip (moved by its y)
    lv_obj_t* result;
    int       center;  // strip row currently in the centre slot
    int       win;     // landed winner (0/1)
    bool      busy;
} reel_ctx_t;

static reel_ctx_t* s_reel = NULL;

// Strip y so that row `r` sits in the centre slot of the 3-slot window.
static int reel_y_for(int r)
{
    return (REEL_VIEW / 2 - r) * REEL_H;  // row r in the centre slot
}

static void reel_move(void* obj, int32_t y)
{
    lv_obj_set_y((lv_obj_t*)obj, (lv_coord_t)y);
}

static void reel_finish(reel_ctx_t* c)
{
    frij_haptic(FRIJ_HAPTIC_SUCCESS);
    lv_label_set_text_fmt(c->result, "%s goes first!", FIRST_NAMES[c->win]);
    lv_obj_set_style_text_color(c->result, lv_color_hex(FIRST_ACCENTS[c->win]), LV_PART_MAIN);
    c->busy = false;
}

static void reel_settle(lv_anim_t* a)
{
    if (a->user_data) {
        reel_finish((reel_ctx_t*)a->user_data);
    }
}

static void reel_spin(lv_event_t* e)
{
    reel_ctx_t* c = (reel_ctx_t*)lv_event_get_user_data(e);
    if (c->busy) {
        return;
    }
    c->busy = true;

    // Snap back to a low row that still has neighbours above it and shows the SAME
    // name as now (no visible jump — the strip repeats every 2 rows), so each spin
    // starts near the top with a full multi-turn run ahead.
    int p    = c->center & 1;
    int base = REEL_VIEW / 2;
    if ((base & 1) != p) {
        base++;  // keep the same colour under the centre
    }
    int from = reel_y_for(base);
    lv_obj_set_y(c->inner, from);
    c->center = base;

    int win  = (int)lv_rand(0, 1);
    int land = base + REEL_MIN_TRAVEL + (int)lv_rand(0, 4);  // >= 20 rows of travel
    if ((land & 1) != win) {
        land++;  // land on the winner's colour
    }
    c->center = land;
    c->win    = win;
    int to    = reel_y_for(land);

    if (!frij_anim_enabled()) {
        lv_obj_set_y(c->inner, to);  // reduce-motion: jump straight there
        reel_finish(c);
        return;
    }
    lv_label_set_text(c->result, "Spinning...");
    lv_obj_set_style_text_color(c->result, lv_color_hex(FRIJ_TEXT_2), LV_PART_MAIN);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, c->inner);
    lv_anim_set_exec_cb(&a, reel_move);
    lv_anim_set_values(&a, from, to);  // known reset pos, not lv_obj_get_y (stale until refresh)
    lv_anim_set_duration(&a, 2000);  // long enough that 10+ turns read as a spin
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&a, reel_settle);
    a.user_data = c;
    lv_anim_start(&a);
    frij_haptic(FRIJ_HAPTIC_SELECT);
}

static void reel_delete(lv_event_t* e)
{
    reel_ctx_t* c = (reel_ctx_t*)lv_event_get_user_data(e);
    lv_anim_delete(c->inner, reel_move);
    if (c == s_reel) {
        s_reel = NULL;
    }
    lv_free(c);
}

static void reel_grad_free(lv_event_t* e)
{
    lv_free(lv_event_get_user_data(e));
}

// A vertical fade of the page colour, opaque at one edge → clear at the other.
// Stacked over the window's top/bottom slots so the off-centre names dissolve into
// the background (same trick as the header), instead of a hard box border.
static lv_obj_t* reel_fade(lv_obj_t* parent, int y, int h, bool opaque_top)
{
    lv_obj_t* g = lv_obj_create(parent);
    lv_obj_remove_style_all(g);
    lv_obj_set_size(g, LV_PCT(100), h);
    lv_obj_set_pos(g, 0, y);
    lv_obj_add_flag(g, LV_OBJ_FLAG_FLOATING);
    lv_obj_clear_flag(g, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g, LV_OBJ_FLAG_SCROLLABLE);

    lv_grad_dsc_t* d = (lv_grad_dsc_t*)lv_malloc(sizeof(lv_grad_dsc_t));
    if (d == NULL) {
        return g;  // OOM — skip the fade
    }
    lv_memzero(d, sizeof(*d));
    d->dir            = LV_GRAD_DIR_VER;
    d->stops_count    = 3;
    d->stops[0].color = lv_color_hex(FRIJ_SURFACE_1);
    d->stops[1].color = lv_color_hex(FRIJ_SURFACE_1);
    d->stops[2].color = lv_color_hex(FRIJ_SURFACE_1);
    // Stay fully opaque across most of the span, clearing only in the third
    // nearest the centre — so the neighbour names are buried and the centre name
    // is the clear standout (not just the outermost row faded).
    if (opaque_top) {  // outer edge = top
        d->stops[0].frac = 0;   d->stops[0].opa = LV_OPA_COVER;
        d->stops[1].frac = 170; d->stops[1].opa = LV_OPA_COVER;
        d->stops[2].frac = 255; d->stops[2].opa = LV_OPA_TRANSP;
    } else {  // outer edge = bottom
        d->stops[0].frac = 0;   d->stops[0].opa = LV_OPA_TRANSP;
        d->stops[1].frac = 85;  d->stops[1].opa = LV_OPA_COVER;
        d->stops[2].frac = 255; d->stops[2].opa = LV_OPA_COVER;
    }
    lv_obj_set_style_bg_grad(g, d, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_event_cb(g, reel_grad_free, LV_EVENT_DELETE, d);
    return g;
}

static void reel_screen(lv_obj_t* parent)
{
    reel_ctx_t* c = (reel_ctx_t*)lv_malloc(sizeof(reel_ctx_t));
    if (c == NULL) {
        return;
    }
    lv_memzero(c, sizeof(*c));

    lv_obj_t* col = frij_page(parent);
    lv_obj_set_style_pad_row(col, FRIJ_SP_M, LV_PART_MAIN);
    frij_label(col, "Who goes first?", FRIJ_FONT_BODY, FRIJ_TEXT_2);

    // No box/border: a 5-slot-tall window that just clips the strip; the centre
    // slot is the pick and the 2 slots above + 2 below fade into the background.
    lv_obj_t* window = lv_obj_create(col);
    lv_obj_remove_style_all(window);
    lv_obj_set_size(window, LV_PCT(80), REEL_H * REEL_VIEW);
    lv_obj_clear_flag(window, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(window, LV_OBJ_FLAG_CLICKABLE);

    c->inner = lv_obj_create(window);
    lv_obj_remove_style_all(c->inner);
    lv_obj_set_width(c->inner, LV_PCT(100));
    lv_obj_set_height(c->inner, REEL_H * REEL_ROWS);
    lv_obj_set_pos(c->inner, 0, reel_y_for(REEL_VIEW / 2));  // centre slot populated
    lv_obj_clear_flag(c->inner, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(c->inner, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < REEL_ROWS; i++) {
        lv_obj_t* row = lv_label_create(c->inner);
        lv_label_set_text(row, FIRST_NAMES[i & 1]);
        lv_obj_set_style_text_font(row, FRIJ_FONT_DISPLAY, LV_PART_MAIN);
        lv_obj_set_style_text_color(row, lv_color_hex(FIRST_ACCENTS[i & 1]), LV_PART_MAIN);
        lv_obj_set_size(row, LV_PCT(100), REEL_H);
        lv_obj_set_pos(row, 0, i * REEL_H);
        lv_obj_set_style_text_align(row, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_pad_top(row, (REEL_H - 34) / 2, LV_PART_MAIN);  // ~center the 34px line
    }

    // Fades sit above the strip (added last): cover the 2 slots above the centre
    // and the 2 below, so only the middle name is fully visible.
    reel_fade(window, 0, REEL_H * (REEL_VIEW / 2), true);
    reel_fade(window, REEL_H * (REEL_VIEW / 2 + 1), REEL_H * (REEL_VIEW / 2), false);

    c->center = REEL_VIEW / 2;
    c->result = frij_label(col, "Tap to spin", FRIJ_FONT_BODY, FRIJ_TEXT_2);
    s_reel    = c;

    lv_obj_add_flag(col, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(col, reel_spin, LV_EVENT_CLICKED, c);
    lv_obj_add_event_cb(col, reel_delete, LV_EVENT_DELETE, c);
    frij_stagger_in(col, 60);
}

static void screen(lv_obj_t* parent, int index)
{
    if (index == 1) {
        reel_screen(parent);  // "who goes first?" — slot reel
        return;
    }
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
    // Edge strips + a center gap so a horizontal swipe has clear margin to start
    // in (the cards also bubble the gesture — see make_card — so a swipe works
    // from anywhere, but the inset makes it forgiving). The tap-to-score zones
    // stay large.
    lv_obj_set_style_pad_hor(root, FRIJ_SP_L * 2, LV_PART_MAIN);
    lv_obj_set_style_pad_column(root, FRIJ_SP_M, LV_PART_MAIN);
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
    static const frij_app_t app = {"Scoreboard", ACCENT, glance, 2, screen, sb_action, sb_on_action};
    return &app;
}
