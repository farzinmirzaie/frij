#include "todo.h"

#include <stdint.h>
#include <string.h>
#include <time.h>

#include <ArduinoJson.h>

#include "core/datetime.h"
#include "store/store.h"
#include "ui/anim.h"
#include "ui/components.h"
#include "ui/theme.h"

/*
 * Todo — a checklist backed by the shared store (key "todo"). The list is fed
 * from a shared Google Keep list via the off-device bridge (see bridge/), so
 * the device can only toggle; add/remove happen in Keep.
 *
 * Data: a JSON array under the key "todo": [{"t":"Milk","d":false}, ...].
 *   glance   : a random unchecked item, big, + count remaining
 *   screen 0 : the checklist (tap a row to toggle; "Updated Xm ago" footer)
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
        frij_empty_state(col, "Nothing yet", "Add items in Google Keep");
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

    // tiny "Updated Xm ago" footer (when the list last pulled from the cloud),
    // matching the Events list.
    int synced = frij_store_load_int("todo_synced", 0);
    if (synced > 0) {
        char ago[24], upd[40];
        frij_format_relative(ago, sizeof(ago), (time_t)synced);
        lv_snprintf(upd, sizeof(upd), "Updated %s", ago);
        frij_label(col, upd, FRIJ_FONT_SMALL, FRIJ_TEXT_3);
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

// Glance: one RANDOM unchecked item, big, plus how many remain — a different
// pick each time the glance rebuilds, so other todos get face time without
// opening the app.
static void glance(lv_obj_t* parent)
{
    load_todo();
    lv_obj_t* col = frij_page(parent);

    int open[MAX_ITEMS];
    int open_n = 0;
    for (int i = 0; i < s_n; i++) {
        if (!s_done[i]) {
            open[open_n++] = i;
        }
    }
    const char* next = open_n > 0 ? s_text[open[lv_rand(0, (uint32_t)open_n - 1)]] : NULL;

    if (next) {
        frij_label(col, "On the list", FRIJ_FONT_BODY, FRIJ_TEXT_2);
        // full text (not the list's trim), wrapped, with extra side padding
        lv_obj_t* item = frij_label(col, next, FRIJ_FONT_TITLE, FRIJ_TEXT);
        lv_obj_set_width(item, LV_PCT(100));
        lv_obj_set_style_pad_left(item, FRIJ_SP_L, LV_PART_MAIN);
        lv_obj_set_style_pad_right(item, FRIJ_SP_L, LV_PART_MAIN);
        lv_label_set_long_mode(item, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(item, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_t* count = frij_label(col, "", FRIJ_FONT_BODY, FRIJ_TEXT_2);
        lv_label_set_text_fmt(count, "%d left", s_n - done_count());
    } else if (s_n == 0) {  // title + subtitle, same pattern as the Events glance
        frij_label(col, "No todos", FRIJ_FONT_TITLE, FRIJ_TEXT);
        frij_label(col, "Add via Google Keep", FRIJ_FONT_BODY, FRIJ_TEXT_2);
    } else {
        frij_label(col, "All done", FRIJ_FONT_TITLE, FRIJ_TEXT);
        frij_label(col, "Nothing left", FRIJ_FONT_BODY, FRIJ_TEXT_2);
    }
}

static void screen(lv_obj_t* parent, int index)
{
    (void)index;  // single screen: the checklist
    frij_store_pull_async(STORE_KEY);  // "todo_synced" is stamped by the sync, not here
    load_todo();
    build_list(parent);
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
    frij_store_save_int("todo_synced", (int)time(NULL));
    frij_toast("Syncing...");
    lv_timer_t* t = lv_timer_create(td_refresh_cb, 1500, NULL);
    lv_timer_set_repeat_count(t, 1);  // auto-deletes after firing
}

const frij_app_t* todo_app(void)
{
    static const frij_app_t app = {"Todo", ACCENT, glance, 1, screen, td_action, td_on_action};
    return &app;
}
