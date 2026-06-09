#include "todo.h"

#include <stdint.h>
#include <string.h>

#include <ArduinoJson.h>

#include "store/store.h"
#include "ui/components.h"
#include "ui/theme.h"

/*
 * Todo — a checklist backed by the shared store (so it syncs to Supabase),
 * styled with the Frij design system.
 *
 * Data: a JSON array under the key "todo": [{"t":"Milk","d":false}, ...].
 *   glance   : progress ring + "<done> of <total>"
 *   screen 0 : the checklist (tap a row to toggle; animated)
 *   screen 1 : add-item placeholder
 *   screen 2 : stats
 */

#define MAX_ITEMS 16
#define TEXT_LEN  40
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
    char out[1024];
    serializeJson(doc, out, sizeof(out));
    frij_store_save(STORE_KEY, out);
}

static void seed_defaults(void)
{
    static const char* defaults[] = {"Milk", "Eggs", "Bread", "Coffee"};
    s_n = sizeof(defaults) / sizeof(defaults[0]);
    for (int i = 0; i < s_n; i++) {
        strncpy(s_text[i], defaults[i], TEXT_LEN - 1);
        s_text[i][TEXT_LEN - 1] = '\0';
        s_done[i]               = false;
    }
    save_todo();
}

static void load_todo(void)
{
    char buf[1024];
    if (!frij_store_load(STORE_KEY, buf, sizeof(buf))) {
        seed_defaults();
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, buf) != DeserializationError::Ok || !doc.is<JsonArray>()) {
        seed_defaults();
        return;
    }
    s_n = 0;
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
    if (s_n == 0) {
        seed_defaults();
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
    return s_n > 0 ? (done_count() * 100 / s_n) : 0;
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
    save_todo();
}

static void build_list(lv_obj_t* parent)
{
    // The shared header (back + "Todo" + "+") is provided by the launcher; this
    // is the scrollable content (sits in the area below the header).
    lv_obj_t* col = frij_page(parent);

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
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);  // ellipsize long text

        frij_anim_enter(row, i * 45);  // staggered fade + rise
    }
}

// ---- app contract ---------------------------------------------------------

static void glance(lv_obj_t* parent)
{
    load_todo();
    lv_obj_t* col = frij_page(parent);

    lv_obj_t* ring  = frij_progress_ring(col, 72, done_pct(), ACCENT);
    lv_obj_t* ringl = frij_label(ring, "", FRIJ_FONT_BODY, FRIJ_TEXT);
    lv_label_set_text_fmt(ringl, "%d%%", done_pct());
    lv_obj_center(ringl);

    frij_label(col, "Todo", FRIJ_FONT_TITLE, FRIJ_TEXT);
    lv_obj_t* sub = frij_label(col, "", FRIJ_FONT_BODY, FRIJ_TEXT_2);
    lv_label_set_text_fmt(sub, "%d of %d", done_count(), s_n);
}

static void screen(lv_obj_t* parent, int index)
{
    if (index == 0) {
        frij_store_pull_async(STORE_KEY);
        load_todo();
        build_list(parent);
    } else if (index == 1) {
        frij_empty_state(frij_page(parent), "Add from\nthe web app");
    } else {
        load_todo();
        lv_obj_t* col   = frij_page(parent);
        lv_obj_t* ring  = frij_progress_ring(col, 84, done_pct(), ACCENT);
        lv_obj_t* ringl = frij_label(ring, "", FRIJ_FONT_TITLE, FRIJ_TEXT);
        lv_label_set_text_fmt(ringl, "%d%%", done_pct());
        lv_obj_center(ringl);
        frij_label(col, "Stats", FRIJ_FONT_TITLE, FRIJ_TEXT);
        lv_obj_t* s = frij_label(col, "", FRIJ_FONT_BODY, FRIJ_TEXT_2);
        lv_label_set_text_fmt(s, "%d items  -  %d done", s_n, done_count());
    }
}

// Header action: a "+" on the list screen (add item — TODO).
static const char* td_action(int index)
{
    return index == 0 ? LV_SYMBOL_PLUS : NULL;
}

static void td_on_action(int index)
{
    (void)index;  // TODO: add-item flow (on-device keyboard or via the web app)
}

const frij_app_t* todo_app(void)
{
    static const frij_app_t app = {"Todo", ACCENT, glance, 3, screen, td_action, td_on_action};
    return &app;
}
