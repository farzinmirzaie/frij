#include "events.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <ArduinoJson.h>

#include "core/datetime.h"
#include "store/store.h"
#include "ui/anim.h"
#include "ui/components.h"
#include "ui/theme.h"

/*
 * Events — countdowns to the family calendar's upcoming events. Read-only:
 * the off-device bridge (bridge/calendar_to_frij.py) mirrors the Google
 * Calendar iCal feed into the store; the device just renders it.
 *
 * Data: under the key "events", soonest first:
 *   {"at": <sync epoch>, "ev": [{"t":"Dentist","d":"2026-06-14","tm":"09:30",
 *    "te":"10:30","l":"Qualiteeth"}, {"t":"Hari Raya","d":"...","h":true}, ...]}
 * ("tm"/"te" = start/end clock, absent for all-day events; "de" = inclusive
 * end date for multi-day all-day events; "l" = location; "h" = holiday — all
 * optional. A bare array, the pre-"at" shape, still loads.)
 *   glance   : the nearest upcoming event + how soon
 *   screen 0 : the list — Today / This week / Later sections, a day-count
 *              badge per event (accent = family, gray = holiday), and an
 *              "Updated Xm ago" footer
 *   screen 1 : big-number countdown to the next family event
 */

#define MAX_EVENTS 10
#define TEXT_LEN   64
#define DATE_LEN   11  // "YYYY-MM-DD"
#define TIME_LEN   6   // "HH:MM"
#define STORE_KEY  "events"

static const uint32_t ACCENT = FRIJ_PINK;  // Events' color scheme

static char   s_title[MAX_EVENTS][TEXT_LEN];
static char   s_date[MAX_EVENTS][DATE_LEN];
static char   s_date_end[MAX_EVENTS][DATE_LEN];  // "" = single day
static char   s_time[MAX_EVENTS][TIME_LEN];      // "" = all-day
static char   s_time_end[MAX_EVENTS][TIME_LEN];  // "" = no end / all-day
static char   s_loc[MAX_EVENTS][TEXT_LEN];       // "" = no location
static bool   s_holiday[MAX_EVENTS];
static time_t s_synced_at = 0;                   // bridge sync epoch (0 = unknown)
static int    s_n = 0;

// ---- data -------------------------------------------------------------------

static void load_events(void)
{
    s_n         = 0;
    s_synced_at = 0;
    char buf[2048];
    if (!frij_store_load(STORE_KEY, buf, sizeof(buf))) {
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, buf) != DeserializationError::Ok) {
        return;
    }
    JsonArray arr;
    if (doc.is<JsonObject>()) {  // current shape: {"at": epoch, "ev": [...]}
        s_synced_at = (time_t)(doc["at"] | 0L);
        arr         = doc["ev"].as<JsonArray>();
    } else if (doc.is<JsonArray>()) {  // pre-"at" shape: a bare array
        arr = doc.as<JsonArray>();
    }
    if (arr.isNull()) {
        return;
    }
    for (JsonObject o : arr) {
        if (s_n >= MAX_EVENTS) {
            break;
        }
        const char* t = o["t"] | "";
        const char* d = o["d"] | "";
        if (!t[0] || !d[0]) {
            continue;
        }
        strncpy(s_title[s_n], t, TEXT_LEN - 1);
        s_title[s_n][TEXT_LEN - 1] = '\0';
        strncpy(s_date[s_n], d, DATE_LEN - 1);
        s_date[s_n][DATE_LEN - 1] = '\0';
        strncpy(s_date_end[s_n], o["de"] | "", DATE_LEN - 1);
        s_date_end[s_n][DATE_LEN - 1] = '\0';
        strncpy(s_time[s_n], o["tm"] | "", TIME_LEN - 1);
        s_time[s_n][TIME_LEN - 1] = '\0';
        strncpy(s_time_end[s_n], o["te"] | "", TIME_LEN - 1);
        s_time_end[s_n][TIME_LEN - 1] = '\0';
        strncpy(s_loc[s_n], o["l"] | "", TEXT_LEN - 1);
        s_loc[s_n][TEXT_LEN - 1] = '\0';
        s_holiday[s_n] = o["h"] | false;
        s_n++;
    }
}

// Whole days from today to `date` ("YYYY-MM-DD"); 0 = today, negative = past
// (the bridge drops past events, but the cache can be a day stale). Both ends
// are anchored at noon so a DST hour can't shift the count.
static int days_until(const char* date)
{
    int y, m, d;
    if (sscanf(date, "%d-%d-%d", &y, &m, &d) != 3) {
        return -1;
    }
    struct tm ev = {};
    ev.tm_year = y - 1900;
    ev.tm_mon  = m - 1;
    ev.tm_mday = d;
    ev.tm_hour = 12;
    time_t    now = time(NULL);
    struct tm today_tm;
    localtime_r(&now, &today_tm);
    today_tm.tm_hour = 12;
    today_tm.tm_min  = 0;
    today_tm.tm_sec  = 0;
    double diff = difftime(mktime(&ev), mktime(&today_tm));
    return (int)((diff + (diff < 0 ? -43200.0 : 43200.0)) / 86400.0);
}

// True once a today event's end time has passed ("Done" — no point counting
// down to it anymore). Events with no end time stay visible all day.
static bool ended_today(int idx)
{
    if (days_until(s_date[idx]) != 0 || !s_time_end[idx][0]) {
        return false;
    }
    int eh, em;
    if (sscanf(s_time_end[idx], "%d:%d", &eh, &em) != 2) {
        return false;
    }
    time_t    now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    return tmv.tm_hour * 60 + tmv.tm_min > eh * 60 + em;
}

// Hidden from every screen: already past, or over for today.
static bool ev_hidden(int idx)
{
    return days_until(s_date[idx]) < 0 || ended_today(idx);
}

// Format a stored "HH:MM" per the 24-hour setting; false if absent/invalid.
static bool clock_phrase(char* buf, size_t n, const char* hhmm)
{
    int hh, mm;
    if (sscanf(hhmm, "%d:%d", &hh, &mm) != 2) {
        return false;
    }
    struct tm tmv = {};
    tmv.tm_hour   = hh;
    tmv.tm_min    = mm;
    frij_format_time(buf, n, &tmv);
    return true;
}

// The clock part of an event: "12:00 - 13:00", "12:00", or "all day". Plain
// ASCII separators only: neither font subset carries U+00B7 or en dashes.
static void time_phrase(char* buf, size_t n, int idx)
{
    char from[16], to[16];
    if (!clock_phrase(from, sizeof(from), s_time[idx])) {
        lv_snprintf(buf, n, "all day");
        return;
    }
    if (clock_phrase(to, sizeof(to), s_time_end[idx])) {
        lv_snprintf(buf, n, "%s - %s", from, to);
    } else {
        lv_snprintf(buf, n, "%s", from);
    }
}

// "Today" / "Tomorrow" / "In 12 days", with the clock appended for timed
// events ("Today, 12:00 - 13:00").
static void when_phrase(char* buf, size_t n, int idx)
{
    int  days = days_until(s_date[idx]);
    char when[24];
    if (days <= 0) {
        lv_snprintf(when, sizeof(when), "Today");
    } else if (days == 1) {
        lv_snprintf(when, sizeof(when), "Tomorrow");
    } else {
        lv_snprintf(when, sizeof(when), "In %d days", days);
    }
    if (s_time[idx][0]) {
        char clock[36];
        time_phrase(clock, sizeof(clock), idx);
        lv_snprintf(buf, n, "%s, %s", when, clock);
    } else {
        lv_snprintf(buf, n, "%s", when);
    }
}

// The badge text: "Now" today, else the day count ("3d", "99+" beyond).
static void badge_text(char* buf, size_t n, int days)
{
    if (days <= 0) {
        lv_snprintf(buf, n, "Now");
    } else if (days > 99) {
        lv_snprintf(buf, n, "99+");
    } else {
        lv_snprintf(buf, n, "%dd", days);
    }
}

// A stored date through strftime ("%a %d %b" -> "Sun 14 Jun").
static void date_fmt(char* buf, size_t n, const char* date, const char* fmt)
{
    int y, m, d;
    if (sscanf(date, "%d-%d-%d", &y, &m, &d) != 3) {
        buf[0] = '\0';
        return;
    }
    struct tm tmv = {};
    tmv.tm_year   = y - 1900;
    tmv.tm_mon    = m - 1;
    tmv.tm_mday   = d;
    tmv.tm_hour   = 12;
    mktime(&tmv);  // derive the weekday
    strftime(buf, n, fmt, &tmv);
}

// The full date/time line of a list row: "Sun 14 Jun, 12:00 - 13:00",
// "Wed 23 Sep, all day", or "Sat 04 Jul - 17 Jul" for multi-day events.
static void row_when(char* buf, size_t n, int idx)
{
    char date[20];
    date_fmt(date, sizeof(date), s_date[idx], "%a %d %b");
    if (s_date_end[idx][0]) {
        char last[12];
        date_fmt(last, sizeof(last), s_date_end[idx], "%d %b");
        lv_snprintf(buf, n, "%s - %s", date, last);
        return;
    }
    char clock[36];
    time_phrase(clock, sizeof(clock), idx);
    lv_snprintf(buf, n, "%s, %s", date, clock);
}

// ---- the list screen ----------------------------------------------------------

static lv_obj_t* s_list_col = NULL;  // the list page, for in-place refresh

static void on_list_deleted(lv_event_t* e)
{
    if (s_list_col == lv_event_get_target(e)) {
        s_list_col = NULL;
    }
}

static void populate_list(lv_obj_t* col)
{
    static const char* SECTIONS[] = {"Today", "This week", "Later"};

    int shown    = 0;
    int last_sec = -1;
    for (int i = 0; i < s_n; i++) {
        if (ev_hidden(i)) {
            continue;  // past (stale cache) or already over for today
        }
        shown++;

        int days = days_until(s_date[i]);
        int sec  = days == 0 ? 0 : (days <= 7 ? 1 : 2);
        if (sec != last_sec) {  // events arrive sorted, so sections are runs
            frij_section_label(col, SECTIONS[sec]);
            last_sec = sec;
        }

        bool has_loc = s_loc[i][0] != '\0';

        lv_obj_t* row = frij_surface_row(col);
        // two text lines + the badge need more than ROW_H; three with a location
        lv_obj_set_height(row, has_loc ? 88 : 72);

        char badge[8];
        badge_text(badge, sizeof(badge), days);
        // family events carry the app accent; holidays stay neutral gray
        bool hol = s_holiday[i];
        frij_circle_button(row, 44, hol ? FRIJ_SURFACE_3 : ACCENT, badge, FRIJ_FONT_SMALL,
                           hol ? FRIJ_TEXT_2 : 0x101216, NULL);

        lv_obj_t* texts = frij_col(row, 2);
        lv_obj_set_flex_grow(texts, 1);
        lv_obj_set_style_flex_cross_place(texts, LV_FLEX_ALIGN_START, LV_PART_MAIN);
        lv_obj_t* title = frij_label(texts, s_title[i], FRIJ_FONT_BODY, FRIJ_TEXT);
        lv_obj_set_width(title, LV_PCT(100));
        lv_obj_set_height(title, lv_font_get_line_height(FRIJ_FONT_BODY));
        lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);

        char when[56];
        row_when(when, sizeof(when), i);
        frij_label(texts, when, FRIJ_FONT_SMALL, FRIJ_TEXT_2);

        if (has_loc) {
            lv_obj_t* loc = frij_label(texts, s_loc[i], FRIJ_FONT_SMALL, FRIJ_TEXT_2);
            lv_obj_set_width(loc, LV_PCT(100));
            lv_obj_set_height(loc, lv_font_get_line_height(FRIJ_FONT_SMALL));
            lv_label_set_long_mode(loc, LV_LABEL_LONG_DOT);
        }
    }
    if (shown == 0) {
        frij_empty_state(col, "No upcoming events", "Add events in the family\nGoogle Calendar");
        return;
    }

    if (s_synced_at > 0) {  // bridge freshness, tiny, at the very bottom
        char ago[24], upd[40];
        frij_format_relative(ago, sizeof(ago), s_synced_at);
        lv_snprintf(upd, sizeof(upd), "Updated %s", ago);
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
    load_events();
    lv_obj_t* col = frij_page(parent);

    int next = -1;
    for (int i = 0; i < s_n; i++) {
        if (!ev_hidden(i)) {
            next = i;
            break;
        }
    }

    if (next < 0) {
        frij_label(col, "No events", FRIJ_FONT_TITLE, FRIJ_TEXT);
        frij_label(col, "Add via Google Calendar", FRIJ_FONT_BODY, FRIJ_TEXT_2);
        return;
    }

    frij_label(col, "Coming up", FRIJ_FONT_BODY, FRIJ_TEXT_2);
    lv_obj_t* title = frij_label(col, s_title[next], FRIJ_FONT_TITLE, FRIJ_TEXT);
    lv_obj_set_width(title, LV_PCT(100));
    lv_obj_set_style_pad_left(title, FRIJ_SP_L, LV_PART_MAIN);
    lv_obj_set_style_pad_right(title, FRIJ_SP_L, LV_PART_MAIN);
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    char when[56];
    when_phrase(when, sizeof(when), next);
    frij_label(col, when, FRIJ_FONT_BODY, ACCENT);

    if (s_loc[next][0]) {
        lv_obj_t* loc = frij_label(col, s_loc[next], FRIJ_FONT_SMALL, FRIJ_TEXT_2);
        lv_obj_set_width(loc, LV_PCT(80));
        lv_obj_set_height(loc, lv_font_get_line_height(FRIJ_FONT_SMALL));
        lv_label_set_long_mode(loc, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(loc, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }
}

// Countdown screen: the next family event (holidays excluded) as a big number.
static void build_countdown(lv_obj_t* parent)
{
    load_events();
    lv_obj_t* col = frij_page(parent);

    int next = -1;
    for (int i = 0; i < s_n; i++) {
        if (!s_holiday[i] && !ev_hidden(i)) {
            next = i;
            break;
        }
    }
    if (next < 0) {
        frij_empty_state(col, "No countdown", "Family events appear here");
        return;
    }

    int days = days_until(s_date[next]);
    if (days == 0) {  // the clock font is digits-only, so words use Display
        frij_label(col, "Today", FRIJ_FONT_DISPLAY, FRIJ_TEXT);
    } else if (days == 1) {
        frij_label(col, "Tomorrow", FRIJ_FONT_DISPLAY, FRIJ_TEXT);
    } else {
        char num[8];
        lv_snprintf(num, sizeof(num), "%d", days);
        frij_label(col, num, FRIJ_FONT_CLOCK, FRIJ_TEXT);
        frij_label(col, "days", FRIJ_FONT_BODY, FRIJ_TEXT_2);
    }

    lv_obj_t* title = frij_label(col, s_title[next], FRIJ_FONT_TITLE, ACCENT);
    lv_obj_set_width(title, LV_PCT(100));
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    char when[56];
    row_when(when, sizeof(when), next);
    frij_label(col, when, FRIJ_FONT_SMALL, FRIJ_TEXT_2);
}

static void screen(lv_obj_t* parent, int index)
{
    if (index == 0) {
        frij_store_pull_async(STORE_KEY);
        load_events();
        build_list(parent);
    } else {
        build_countdown(parent);
    }
}

// Header action: refresh — pull the latest from the cloud (the calendar bridge
// keeps it current) and rebuild. Same non-blocking pattern as Todo.
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
    load_events();
    lv_obj_clean(s_list_col);
    populate_list(s_list_col);
    frij_page_settle_at(s_list_col, y);
}

static void ev_on_action(int index)
{
    if (index != 0 || !s_list_col) {
        return;
    }
    frij_store_pull_async(STORE_KEY);
    frij_toast("Syncing...");
    lv_timer_t* t = lv_timer_create(ev_refresh_cb, 1500, NULL);
    lv_timer_set_repeat_count(t, 1);
}

const frij_app_t* events_app(void)
{
    static const frij_app_t app = {"Events", ACCENT, glance, 2, screen, ev_action, ev_on_action};
    return &app;
}
