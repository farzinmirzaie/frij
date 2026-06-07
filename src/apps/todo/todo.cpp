#include "todo.h"

#include <stdint.h>
#include <string.h>

#include <ArduinoJson.h>

#include "store/store.h"

/*
 * Todo — a checklist backed by the shared store (so it syncs to Supabase).
 *
 * Data: a JSON array under the key "todo": [{"t":"Milk","d":false}, ...].
 *   glance   : "<done>/<total> done"
 *   screen 0 : the checklist; toggling a box saves (cache + cloud)
 *   screen 1 : add-item placeholder (on-device editing is a later step)
 *   screen 2 : stats
 *
 * Items come from the cloud (edit them from a web app later); the device
 * shows them and syncs the done state.
 */

#define MAX_ITEMS 16
#define TEXT_LEN  40
#define STORE_KEY "todo"

static char s_text[MAX_ITEMS][TEXT_LEN];
static bool s_done[MAX_ITEMS];
static int  s_n = 0;

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

// ---- UI helpers -----------------------------------------------------------

static void center_column(lv_obj_t* parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
}

static lv_obj_t* tinted_label(lv_obj_t* parent, const char* text)
{
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_color(l, lv_color_hex(0xBFD8C9), LV_PART_MAIN);
    return l;
}

static void on_toggle(lv_event_t* e)
{
    lv_obj_t* cb = (lv_obj_t*)lv_event_get_target(e);
    int       i  = (int)(intptr_t)lv_event_get_user_data(e);
    if (i >= 0 && i < s_n) {
        s_done[i] = lv_obj_has_state(cb, LV_STATE_CHECKED);
        save_todo();
    }
}

// ---- app contract ---------------------------------------------------------

static void glance(lv_obj_t* parent)
{
    load_todo();
    center_column(parent);

    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "Todo");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

    lv_obj_t* sub = lv_label_create(parent);
    lv_label_set_text_fmt(sub, "%d/%d done", done_count(), s_n);
    lv_obj_set_style_text_color(sub, lv_color_hex(0xBFD8C9), LV_PART_MAIN);
}

static void screen(lv_obj_t* parent, int index)
{
    center_column(parent);

    if (index == 0) {
        frij_store_pull(STORE_KEY);  // fetch latest from the cloud
        load_todo();
        for (int i = 0; i < s_n; i++) {
            lv_obj_t* item = lv_checkbox_create(parent);
            lv_checkbox_set_text(item, s_text[i]);
            lv_obj_set_style_text_color(item, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            if (s_done[i]) {
                lv_obj_add_state(item, LV_STATE_CHECKED);
            }
            lv_obj_add_event_cb(item, on_toggle, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        }
    } else if (index == 1) {
        lv_obj_t* t = lv_label_create(parent);
        lv_label_set_text(t, LV_SYMBOL_PLUS "  Add item");
        lv_obj_set_style_text_color(t, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        tinted_label(parent, "(from the web app)");
    } else {
        load_todo();
        lv_obj_t* t = lv_label_create(parent);
        lv_label_set_text(t, "Stats");
        lv_obj_set_style_text_color(t, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_t* s = tinted_label(parent, "");
        lv_label_set_text_fmt(s, "%d items\n%d done", s_n, done_count());
        lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }
}

const frij_app_t* todo_app(void)
{
    static const frij_app_t app = {"Todo", 0x14512F, glance, 3, screen};
    return &app;
}
