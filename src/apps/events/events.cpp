#include "events.h"

#include "data/events.h"
#include "ui/anim.h"
#include "ui/components.h"
#include "ui/theme.h"

/*
 * Events — countdowns to upcoming events across one or more calendars. PURE UI:
 * it asks the data layer (data/events.h) for display-ready view structs and
 * lays them out. It knows nothing about the store, the cloud, or the JSON shape.
 *
 *   glance   : the nearest upcoming event + how soon
 *   screen 0 : the list — Today / This week / Later sections, a unit-scaled
 *              badge per event in its calendar's color, "Updated Xm ago" footer
 *   screen 1 : big-number countdown to the next upcoming event
 *   screen 2 : Calendars — toggle each calendar on/off (hides it everywhere)
 *
 * The per-calendar color is used ONLY on the list badges; everywhere else
 * (glance, countdown, the Calendars switches) uses the app accent.
 */

static const uint32_t ACCENT = FRIJ_PINK;  // Events' app accent

// Readable text on a colored badge: dark on light colors, white on dark ones.
static uint32_t on_color(uint32_t bg)
{
    uint32_t r = (bg >> 16) & 0xFF, g = (bg >> 8) & 0xFF, b = bg & 0xFF;
    return (r * 299 + g * 587 + b * 114) / 1000 > 150 ? 0x101216 : FRIJ_TEXT;
}

// ---- the list screen ----------------------------------------------------------

static lv_obj_t* s_list_col = NULL;  // the list page, for in-place refresh

static void on_list_deleted(lv_event_t* e)
{
    if (s_list_col == lv_event_get_target(e)) {
        s_list_col = NULL;
    }
}

static void add_event_row(lv_obj_t* col, const frij_event_view_t* v)
{
    bool has_loc = v->loc[0] != '\0';

    lv_obj_t* row = frij_surface_row(col);
    lv_obj_set_height(row, has_loc ? 88 : 72);  // 2 lines + badge, 3 with a location

    // each event's badge carries its calendar's color (gray for a holidays feed
    // is just that calendar's own color — no special case)
    lv_obj_t* dot = frij_circle_button(row, 44, v->color, v->badge, FRIJ_FONT_SMALL,
                                       on_color(v->color), NULL);
    if (v->days == 0 && frij_anim_enabled()) {  // today: a gentle breathing pulse
        lv_obj_set_style_transform_pivot_x(dot, lv_pct(50), LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_y(dot, lv_pct(50), LV_PART_MAIN);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, dot);
        lv_anim_set_exec_cb(&a, frij_anim_exec_scale);
        lv_anim_set_values(&a, 256, 280);
        lv_anim_set_duration(&a, 900);
        lv_anim_set_playback_duration(&a, 900);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        lv_anim_start(&a);
    }

    lv_obj_t* texts = frij_col(row, 2);
    lv_obj_set_flex_grow(texts, 1);
    lv_obj_set_style_flex_cross_place(texts, LV_FLEX_ALIGN_START, LV_PART_MAIN);
    lv_obj_t* title = frij_label(texts, v->title, FRIJ_FONT_BODY, FRIJ_TEXT);
    lv_obj_set_width(title, LV_PCT(100));
    lv_obj_set_height(title, lv_font_get_line_height(FRIJ_FONT_BODY));
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);

    frij_label(texts, v->when, FRIJ_FONT_SMALL, FRIJ_TEXT_2);

    if (has_loc) {
        lv_obj_t* loc = frij_label(texts, v->loc, FRIJ_FONT_SMALL, FRIJ_TEXT_2);
        lv_obj_set_width(loc, LV_PCT(100));
        lv_obj_set_height(loc, lv_font_get_line_height(FRIJ_FONT_SMALL));
        lv_label_set_long_mode(loc, LV_LABEL_LONG_DOT);
    }
}

static void populate_list(lv_obj_t* col)
{
    static const char* SECTIONS[] = {"Today", "This week", "Later"};

    static frij_event_view_t views[FRIJ_EVENTS_MAX];  // static: 50 views is too big for the stack
    int                      n = frij_events_load(views, FRIJ_EVENTS_MAX);

    if (n == 0) {
        frij_empty_state(col, "No upcoming events", "Add events in the family\nGoogle Calendar");
        return;
    }

    int last_sec = -1;
    for (int i = 0; i < n; i++) {
        int sec = views[i].days == 0 ? 0 : (views[i].days <= 7 ? 1 : 2);
        if (sec != last_sec) {  // views arrive sorted, so sections are runs
            frij_section_label(col, SECTIONS[sec]);
            last_sec = sec;
        }
        add_event_row(col, &views[i]);
    }

    char upd[40];
    if (frij_events_synced_ago(upd, sizeof(upd))) {  // tiny freshness footer
        frij_label(col, upd, FRIJ_FONT_SMALL, FRIJ_TEXT_3);
    }
    frij_stagger_in(col, 45);
}

static void build_list(lv_obj_t* parent)
{
    s_list_col = frij_page(parent);
    lv_obj_add_event_cb(s_list_col, on_list_deleted, LV_EVENT_DELETE, NULL);
    populate_list(s_list_col);
}

// ---- app contract -------------------------------------------------------------

// Glance: the nearest upcoming event + how soon it is.
static void glance(lv_obj_t* parent)
{
    lv_obj_t*         col = frij_page(parent);
    frij_event_view_t v[1];
    if (frij_events_load(v, 1) == 0) {
        frij_label(col, "No events", FRIJ_FONT_TITLE, FRIJ_TEXT);
        frij_label(col, "Add via Google Calendar", FRIJ_FONT_BODY, FRIJ_TEXT_2);
        return;
    }

    frij_label(col, "Coming up", FRIJ_FONT_BODY, FRIJ_TEXT_2);
    lv_obj_t* title = frij_label(col, v[0].title, FRIJ_FONT_TITLE, FRIJ_TEXT);
    lv_obj_set_width(title, LV_PCT(100));
    lv_obj_set_style_pad_left(title, FRIJ_SP_L, LV_PART_MAIN);
    lv_obj_set_style_pad_right(title, FRIJ_SP_L, LV_PART_MAIN);
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    frij_label(col, v[0].rel, FRIJ_FONT_BODY, ACCENT);

    if (v[0].loc[0]) {
        lv_obj_t* loc = frij_label(col, v[0].loc, FRIJ_FONT_SMALL, FRIJ_TEXT_2);
        lv_obj_set_width(loc, LV_PCT(80));
        lv_obj_set_height(loc, lv_font_get_line_height(FRIJ_FONT_SMALL));
        lv_label_set_long_mode(loc, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(loc, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }
}

// Countdown screen: the next upcoming event as a big number.
static void build_countdown(lv_obj_t* parent)
{
    lv_obj_t*         col = frij_page(parent);
    frij_event_view_t v;
    if (!frij_events_next(&v)) {
        frij_empty_state(col, "No countdown", "Upcoming events appear here");
        return;
    }

    if (v.days == 0) {  // the clock font is digits-only, so words use Display
        frij_label(col, "Today", FRIJ_FONT_DISPLAY, FRIJ_TEXT);
    } else if (v.days == 1) {
        frij_label(col, "Tomorrow", FRIJ_FONT_DISPLAY, FRIJ_TEXT);
    } else {
        char num[8];
        lv_snprintf(num, sizeof(num), "%d", v.days);
        frij_label(col, num, FRIJ_FONT_CLOCK, FRIJ_TEXT);
        frij_label(col, "days", FRIJ_FONT_BODY, FRIJ_TEXT_2);
    }

    lv_obj_t* title = frij_label(col, v.title, FRIJ_FONT_TITLE, ACCENT);
    lv_obj_set_width(title, LV_PCT(100));
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    frij_label(col, v.when, FRIJ_FONT_SMALL, FRIJ_TEXT_2);

    if (v.loc[0]) {  // where, if the event has a location
        lv_obj_t* loc = frij_label(col, v.loc, FRIJ_FONT_SMALL, FRIJ_TEXT_3);
        lv_obj_set_width(loc, LV_PCT(90));
        lv_label_set_long_mode(loc, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(loc, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }
}

// ---- the Calendars screen ------------------------------------------------------

// Names backing each toggle's user_data (kept alive for the screen's lifetime;
// at most FRIJ_CAL_MAX rows). The switch passes its name to the data layer.
static char s_cal_names[FRIJ_CAL_MAX][FRIJ_CAL_NAME];

static void on_cal_toggle(lv_event_t* e)
{
    const char* name = (const char*)lv_event_get_user_data(e);
    lv_obj_t*   sw   = (lv_obj_t*)lv_event_get_target(e);
    bool        on   = lv_obj_has_state(sw, LV_STATE_CHECKED);
    frij_events_set_calendar(name, on);  // hides/shows it in list + glance + countdown
}

static void build_calendars(lv_obj_t* parent)
{
    lv_obj_t*       col = frij_page(parent);
    frij_calendar_t cals[FRIJ_CAL_MAX];
    int             n = frij_events_calendars(cals, FRIJ_CAL_MAX);

    if (n == 0) {
        frij_empty_state(col, "No calendars", "Add calendars off-device\n(see bridge)");
        return;
    }

    frij_section_label(col, "Show calendars");
    for (int i = 0; i < n; i++) {
        lv_snprintf(s_cal_names[i], FRIJ_CAL_NAME, "%s", cals[i].name);
        // leading dot in the calendar's color; the switch stays the app accent
        lv_obj_t* sw = frij_toggle_row_dot(col, cals[i].color, cals[i].name, cals[i].enabled, ACCENT);
        lv_obj_add_event_cb(sw, on_cal_toggle, LV_EVENT_VALUE_CHANGED, s_cal_names[i]);
    }
    frij_stagger_in(col, 45);
}

static void screen(lv_obj_t* parent, int index)
{
    if (index == 0) {
        frij_events_sync();  // kick a background refresh; the list reads the cache
        build_list(parent);
    } else if (index == 1) {
        build_countdown(parent);
    } else {
        build_calendars(parent);
    }
}

// Header action: refresh — pull the latest and rebuild. Non-blocking.
static const char* ev_action(int index)
{
    return index == 0 ? LV_SYMBOL_REFRESH : NULL;
}

static void ev_refresh_cb(lv_timer_t* t)
{
    (void)t;
    if (!s_list_col) {
        return;  // the page was closed in the meantime
    }
    int32_t y = lv_obj_get_scroll_y(s_list_col);
    lv_obj_clean(s_list_col);
    populate_list(s_list_col);
    frij_page_settle_at(s_list_col, y);
}

static void ev_on_action(int index)
{
    if (index != 0 || !s_list_col) {
        return;
    }
    frij_events_sync();
    frij_toast("Syncing...");
    lv_timer_t* t = lv_timer_create(ev_refresh_cb, 1500, NULL);
    lv_timer_set_repeat_count(t, 1);
}

const frij_app_t* events_app(void)
{
    static const frij_app_t app = {"Events", ACCENT, glance, 3, screen, ev_action, ev_on_action};
    return &app;
}
