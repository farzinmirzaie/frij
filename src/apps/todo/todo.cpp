#include "todo.h"

#include <stdint.h>
#include <string.h>

#include <ArduinoJson.h>

#include "store/store.h"
#include "system/haptics.h"
#include "ui/anim.h"
#include "ui/components.h"
#include "ui/theme.h"

/*
 * Todo — a checklist backed by the shared store (key "todo"). The list can be
 * fed from a shared Google Keep list via the off-device bridge (see bridge/),
 * so today the device is effectively read-only; editing happens in Keep.
 *
 * Data: a JSON array under the key "todo": [{"t":"Milk","d":false}, ...].
 *   glance   : "up next" — the next unchecked item, big, + count remaining
 *   screen 0 : the checklist (tap a row to toggle; animated)
 *   screen 1 : progress (big ring + %)
 *   screen 2 : add by voice (placeholder UI — no STT yet)
 */

#define MAX_ITEMS 16
#define TEXT_LEN  64  // full-ish item text; the list ellipsizes, the glance wraps it
#define STORE_KEY "todo"

static const uint32_t ACCENT = FRIJ_YELLOW;  // Todo's color scheme

static char s_text[MAX_ITEMS][TEXT_LEN];
static bool s_done[MAX_ITEMS];
static int  s_n = 0;

// ---- data -----------------------------------------------------------------

static void save_todo(void)
{
    JsonDocument doc;
    JsonArray    arr = doc.to<JsonArray>();
    for (int i = 0; i < s_n; i++) {
        JsonObject o = arr.add<JsonObject>();
        o["t"]       = s_text[i];
        o["d"]       = s_done[i];
    }
    char out[2048];
    serializeJson(doc, out, sizeof(out));
    frij_store_save(STORE_KEY, out);
}

static void load_todo(void)
{
    // No seeded defaults — an absent/empty/corrupt store is simply an empty
    // list (the UI shows its empty states; the Keep bridge fills the real one).
    s_n = 0;
    char buf[2048];
    if (!frij_store_load(STORE_KEY, buf, sizeof(buf))) {
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, buf) != DeserializationError::Ok || !doc.is<JsonArray>()) {
        return;
    }
    for (JsonObject o : doc.as<JsonArray>()) {
        if (s_n >= MAX_ITEMS) {
            break;
        }
        const char* t = o["t"] | "";
        strncpy(s_text[s_n], t, TEXT_LEN - 1);
        s_text[s_n][TEXT_LEN - 1] = '\0';
        s_done[s_n]               = o["d"] | false;
        s_n++;
    }
}

static int done_count(void)
{
    int c = 0;
    for (int i = 0; i < s_n; i++) {
        c += s_done[i] ? 1 : 0;
    }
    return c;
}

static int done_pct(void)
{
    return s_n > 0 ? ((done_count() * 100 + s_n / 2) / s_n) : 0;  // rounded, not truncated
}

// ---- the checklist screen --------------------------------------------------

static void on_toggle(lv_event_t* e)
{
    lv_obj_t* row = (lv_obj_t*)lv_event_get_current_target(e);
    int       i   = (int)(intptr_t)lv_obj_get_user_data(row);
    if (i < 0 || i >= s_n) {
        return;
    }
    s_done[i]         = !s_done[i];
    lv_obj_t* check   = lv_obj_get_child(row, 0);
    lv_obj_t* label   = lv_obj_get_child(row, 1);
    frij_check_set(check, s_done[i], true);
    lv_obj_set_style_text_color(label, lv_color_hex(s_done[i] ? FRIJ_TEXT_2 : FRIJ_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_decor(label, s_done[i] ? LV_TEXT_DECOR_STRIKETHROUGH : LV_TEXT_DECOR_NONE,
                                LV_PART_MAIN);
    save_todo();
}

static lv_obj_t* s_list_col = NULL;  // the checklist page, for in-place refresh

// Clear the cached page pointer when its page is destroyed (layer closed), so a
// later refresh can't act on freed memory.
static void on_list_deleted(lv_event_t* e)
{
    if (s_list_col == lv_event_get_target(e)) {
        s_list_col = NULL;
    }
}

// Fill the page with rows from the current s_text/s_done.
static void populate_list(lv_obj_t* col)
{
    if (s_n == 0) {
        frij_empty_state(col, "Nothing yet");
        return;
    }
    for (int i = 0; i < s_n; i++) {
        lv_obj_t* row = frij_surface_row(col);
        lv_obj_set_user_data(row, (void*)(intptr_t)i);
        lv_obj_add_event_cb(row, on_toggle, LV_EVENT_CLICKED, NULL);

        frij_check(row, s_done[i], ACCENT);
        lv_obj_t* label = frij_label(row, s_text[i], FRIJ_FONT_BODY,
                                     s_done[i] ? FRIJ_TEXT_2 : FRIJ_TEXT);
        lv_obj_set_flex_grow(label, 1);
        // one line, ellipsized — pin the height so LONG_DOT dots instead of wrapping
        lv_obj_set_height(label, lv_font_get_line_height(FRIJ_FONT_BODY));
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        if (s_done[i]) {
            lv_obj_set_style_text_decor(label, LV_TEXT_DECOR_STRIKETHROUGH, LV_PART_MAIN);
        }
    }
    frij_stagger_in(col, 45);  // staggered fade + rise (shared helper)
}

static void build_list(lv_obj_t* parent)
{
    // The shared header (back + "Todo" + refresh) is provided by the launcher;
    // this is the scrollable content (sits in the area below the header).
    s_list_col = frij_page(parent);
    lv_obj_add_event_cb(s_list_col, on_list_deleted, LV_EVENT_DELETE, NULL);
    populate_list(s_list_col);
}

// ---- app contract ---------------------------------------------------------

// Glance: "up next" — the next unchecked item, big, plus how many remain.
static void glance(lv_obj_t* parent)
{
    load_todo();
    lv_obj_t* col = frij_page(parent);

    const char* next = NULL;
    for (int i = 0; i < s_n; i++) {
        if (!s_done[i]) {
            next = s_text[i];
            break;
        }
    }

    if (next) {
        frij_label(col, "Up next", FRIJ_FONT_BODY, FRIJ_TEXT_2);
        // full text (not the list's trim), wrapped, with extra side padding
        lv_obj_t* item = frij_label(col, next, FRIJ_FONT_TITLE, FRIJ_TEXT);
        lv_obj_set_width(item, LV_PCT(100));
        lv_obj_set_style_pad_left(item, FRIJ_SP_L, LV_PART_MAIN);
        lv_obj_set_style_pad_right(item, FRIJ_SP_L, LV_PART_MAIN);
        lv_label_set_long_mode(item, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(item, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_t* count = frij_label(col, "", FRIJ_FONT_BODY, FRIJ_TEXT_2);
        lv_label_set_text_fmt(count, "%d left", s_n - done_count());
    } else {
        frij_label(col, s_n == 0 ? "No todos" : "All done", FRIJ_FONT_TITLE, FRIJ_TEXT);
        if (s_n > 0) {
            frij_label(col, "Nothing left", FRIJ_FONT_BODY, FRIJ_TEXT_2);
        }
    }
}

// anim exec: sweep the ring's value.
static void arc_set_value_cb(void* arc, int32_t v)
{
    lv_arc_set_value((lv_obj_t*)arc, (int32_t)v);
}

// Progress screen: a large ring with the % in the middle.
static void build_progress(lv_obj_t* parent)
{
    load_todo();
    lv_obj_t* col = frij_page(parent);

    int       sz  = frij_screen_min() * 60 / 100;  // big, on-brand
    lv_obj_t* arc = frij_progress_ring(col, sz, done_pct(), ACCENT);
    lv_obj_set_style_arc_width(arc, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 12, LV_PART_INDICATOR);

    // sweep the fill from 0 to the value on open (skipped under reduce-motion;
    // the ring is already drawn at done_pct() by frij_progress_ring)
    if (frij_anim_enabled()) {
        lv_anim_t sweep;
        lv_anim_init(&sweep);
        lv_anim_set_var(&sweep, arc);
        lv_anim_set_exec_cb(&sweep, arc_set_value_cb);
        lv_anim_set_values(&sweep, 0, done_pct());
        lv_anim_set_duration(&sweep, FRIJ_ANIM_MS * 2);
        lv_anim_set_path_cb(&sweep, lv_anim_path_ease_out);
        lv_anim_start(&sweep);
    }

    lv_obj_t* inner = frij_col(arc, 2);  // stacked, centered inside the ring
    lv_obj_center(inner);
    frij_anim_enter(inner, FRIJ_ANIM_MS / 2);  // fade/rise in as the ring sweeps
    lv_obj_t* pct = frij_label(inner, "", FRIJ_FONT_DISPLAY, FRIJ_TEXT);
    lv_obj_t* sub = frij_label(inner, "", FRIJ_FONT_BODY, FRIJ_TEXT_2);

    if (s_n == 0) {
        lv_label_set_text(pct, "—");
        lv_label_set_text(sub, "No todos");
    } else if (done_count() == s_n) {
        lv_label_set_text(pct, "100%");
        lv_label_set_text(sub, "All done!");
        // celebrate: a gentle one-shot pulse of the ring + a success buzz
        if (frij_anim_enabled()) {
            lv_obj_set_style_transform_pivot_x(arc, lv_pct(50), LV_PART_MAIN);
            lv_obj_set_style_transform_pivot_y(arc, lv_pct(50), LV_PART_MAIN);
            lv_anim_t pulse;
            lv_anim_init(&pulse);
            lv_anim_set_var(&pulse, arc);
            lv_anim_set_exec_cb(&pulse, frij_anim_exec_scale);
            lv_anim_set_values(&pulse, 256, 280);
            lv_anim_set_duration(&pulse, 260);
            lv_anim_set_playback_duration(&pulse, 260);
            lv_anim_set_delay(&pulse, FRIJ_ANIM_MS);  // after the fill sweep
            lv_anim_set_path_cb(&pulse, lv_anim_path_ease_in_out);
            lv_anim_start(&pulse);
        }
        frij_haptic(FRIJ_HAPTIC_SUCCESS);
    } else {
        lv_label_set_text_fmt(pct, "%d%%", done_pct());
        lv_label_set_text_fmt(sub, "%d of %d done", done_count(), s_n);
    }
}

// Add-by-voice screen: a big mic-style button. Placeholder — no STT yet.
static void on_add_voice(lv_event_t* e)
{
    (void)e;
    frij_toast("Voice add — coming soon");
}

static void build_add(lv_obj_t* parent)
{
    lv_obj_t* col = frij_page(parent);

    // big accent circle with a dark "+" (amber accent reads better with dark text)
    lv_obj_t* mic = frij_circle_button(col, 104, ACCENT, "+", FRIJ_FONT_DISPLAY, 0x101216,
                                       on_add_voice);
    if (frij_anim_enabled()) {  // gentle breathing — invites the tap
        lv_anim_t breathe;
        lv_anim_init(&breathe);
        lv_anim_set_var(&breathe, mic);
        lv_anim_set_exec_cb(&breathe, frij_anim_exec_scale);
        lv_anim_set_values(&breathe, 250, 262);
        lv_anim_set_duration(&breathe, 900);
        lv_anim_set_playback_duration(&breathe, 900);
        lv_anim_set_repeat_count(&breathe, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&breathe, lv_anim_path_ease_in_out);
        lv_anim_start(&breathe);
    }

    frij_label(col, "Add by voice", FRIJ_FONT_TITLE, FRIJ_TEXT);
    frij_label(col, "Tap to speak", FRIJ_FONT_BODY, FRIJ_TEXT_2);
}

static void screen(lv_obj_t* parent, int index)
{
    if (index == 0) {
        frij_store_pull_async(STORE_KEY);
        load_todo();
        build_list(parent);
    } else if (index == 1) {
        build_progress(parent);
    } else {
        build_add(parent);
    }
}

// Header action: a refresh button on the list screen — pulls the latest from the
// cloud (which the Keep→Supabase bridge keeps current) and rebuilds the list.
static const char* td_action(int index)
{
    return index == 0 ? LV_SYMBOL_REFRESH : NULL;
}

// One-shot timer: rebuild the list after the background pull has had time to
// land in the cache (the store has no completion callback yet).
static void td_refresh_cb(lv_timer_t* t)
{
    (void)t;
    if (!s_list_col) {
        return;  // the page was closed in the meantime
    }
    int32_t y = lv_obj_get_scroll_y(s_list_col);  // keep the user's place
    load_todo();
    lv_obj_clean(s_list_col);  // keeps the page's styles/padding, drops the rows
    populate_list(s_list_col);
    frij_page_settle_at(s_list_col, y);
}

static void td_on_action(int index)
{
    if (index != 0 || !s_list_col) {
        return;
    }
    // Non-blocking: pull in the background (a blocking pull froze gestures and
    // animations for the whole fetch), then rebuild once it has likely landed.
    frij_store_pull_async(STORE_KEY);
    frij_toast("Syncing...");
    lv_timer_t* t = lv_timer_create(td_refresh_cb, 1500, NULL);
    lv_timer_set_repeat_count(t, 1);  // auto-deletes after firing
}

const frij_app_t* todo_app(void)
{
    static const frij_app_t app = {"Todo", ACCENT, glance, 3, screen, td_action, td_on_action};
    return &app;
}
