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

static const uint32_t ACCENT    = FRIJ_PINK;  // Events' app accent
static const uint32_t BADGE_INK = 0x101216;   // near-black text on a light badge

enum { SCREEN_LIST, SCREEN_COUNTDOWN, SCREEN_CALENDARS, SCREEN_COUNT };  // app's own screens

static const int EVENT_ROW_H     = 72;  // badge + title + when
static const int EVENT_ROW_H_LOC = 88;  // ...plus a location line

// Readable text on a colored badge: dark on light colors, white on dark ones.
static uint32_t on_color(uint32_t bg)
{
    uint32_t r = (bg >> 16) & 0xFF, g = (bg >> 8) & 0xFF, b = bg & 0xFF;
    return (r * 299 + g * 587 + b * 114) / 1000 > 150 ? BADGE_INK : FRIJ_TEXT;
}

// A centered, wrapping title (kept off the round edges) — glance + countdown.
static lv_obj_t* add_centered_title(lv_obj_t* col, const char* text, uint32_t color)
{
    lv_obj_t* t = frij_label(col, text, FRIJ_FONT_TITLE, color);
    lv_obj_set_width(t, LV_PCT(100));
    lv_obj_set_style_pad_left(t, FRIJ_SP_L, LV_PART_MAIN);
    lv_obj_set_style_pad_right(t, FRIJ_SP_L, LV_PART_MAIN);
    lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    return t;
}

// The event's calendar name as a small colored last row (no-op if untagged).
static void add_cal_tag(lv_obj_t* col, const frij_event_view_t* v)
{
    if (v->cal[0]) {
        frij_label(col, v->cal, FRIJ_FONT_SMALL, v->color);
    }
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
    lv_obj_set_height(row, has_loc ? EVENT_ROW_H_LOC : EVENT_ROW_H);

    // each event's badge carries its calendar's color (gray for a holidays feed
    // is just that calendar's own color — no special case)
    lv_obj_t* dot = frij_circle_button(row, 44, v->color, v->badge, FRIJ_FONT_SMALL,
                                       on_color(v->color), NULL);
    if (v->days == 0) {  // today: a gentle breathing pulse
        frij_pulse(dot);
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

// Which list section an event falls in: 0 Today, 1 This week, 2 Later.
static int section_of(int days)
{
    const int THIS_WEEK_DAYS = 7;
    return days == 0 ? 0 : (days <= THIS_WEEK_DAYS ? 1 : 2);
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
        int sec = section_of(views[i].days);
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
    frij_event_view_t v;
    if (!frij_events_next(&v)) {
        frij_label(col, "No events", FRIJ_FONT_TITLE, FRIJ_TEXT);
        frij_label(col, "Add via Google Calendar", FRIJ_FONT_BODY, FRIJ_TEXT_2);
        return;
    }

    add_centered_title(col, v.title, FRIJ_TEXT);
    frij_label(col, v.rel, FRIJ_FONT_BODY, ACCENT);

    if (v.loc[0]) {
        lv_obj_t* loc = frij_label(col, v.loc, FRIJ_FONT_SMALL, FRIJ_TEXT_2);
        lv_obj_set_width(loc, LV_PCT(80));
        lv_obj_set_height(loc, lv_font_get_line_height(FRIJ_FONT_SMALL));
        lv_label_set_long_mode(loc, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(loc, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }

    add_cal_tag(col, &v);
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

    add_centered_title(col, v.title, ACCENT);
    frij_label(col, v.when, FRIJ_FONT_SMALL, FRIJ_TEXT_2);

    if (v.loc[0]) {  // where, if the event has a location
        lv_obj_t* loc = frij_label(col, v.loc, FRIJ_FONT_SMALL, FRIJ_TEXT_3);
        lv_obj_set_width(loc, LV_PCT(90));
        lv_label_set_long_mode(loc, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(loc, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }

    add_cal_tag(col, &v);
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
    if (index == SCREEN_LIST) {
        frij_events_sync();  // kick a background refresh; the list reads the cache
        build_list(parent);
    } else if (index == SCREEN_COUNTDOWN) {
        build_countdown(parent);
    } else {
        build_calendars(parent);
    }
}

// Header action: refresh (list screen only) — pull the latest and rebuild.
static const char* ev_action(int index)
{
    return index == SCREEN_LIST ? LV_SYMBOL_REFRESH : NULL;
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
    if (index != SCREEN_LIST || !s_list_col) {
        return;
    }
    frij_events_sync();
    frij_toast("Syncing...");
    lv_timer_t* t = lv_timer_create(ev_refresh_cb, 1500, NULL);
    lv_timer_set_repeat_count(t, 1);
}

const frij_app_t* events_app(void)
{
    static const frij_app_t app = {"Events", ACCENT, glance, SCREEN_COUNT, screen, ev_action, ev_on_action};
    return &app;
}
