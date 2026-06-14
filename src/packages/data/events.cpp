#include "events.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <ArduinoJson.h>

#include "core/datetime.h"
#include "store/store.h"

/*
 * Events data layer — see events.h. Parses store:events, computes day math,
 * formats display-ready view structs, and applies the per-calendar hide set.
 * No UI/LVGL dependency.
 *
 * Store shape under key "events", soonest first:
 *   {"at": <sync epoch>,
 *    "cal": [{"n":"Family","c":"F472B6"}, {"n":"Holidays","c":"6B6B74"}],
 *    "ev": [{"t":"Dentist","d":"2026-06-14","tm":"09:30","te":"10:30",
 *            "l":"Qualiteeth","c":"Family"},
 *           {"t":"Hari Raya","d":"...","c":"Holidays"}, ...]}
 * ("tm"/"te" = start/end clock, absent for all-day; "de" = inclusive end date
 * for multi-day all-day events; "l" = location; "c" = calendar name.) All
 * calendars are treated alike (a holidays feed is just one whose color is gray).
 * The hidden-calendar set is a JSON array of names under "events_off"
 * (e.g. ["Holidays"]).
 */

#define MAX_EVENTS    FRIJ_EVENTS_MAX
#define TEXT_LEN      FRIJ_EVENT_TITLE
#define DATE_LEN      11  // "YYYY-MM-DD"
#define TIME_LEN      6   // "HH:MM"
#define STORE_KEY     "events"
#define OFF_KEY       "events_off"
#define DEFAULT_COLOR 0xF472B6u  // brand pink, when a calendar color is missing

static char     s_title[MAX_EVENTS][TEXT_LEN];
static char     s_date[MAX_EVENTS][DATE_LEN];
static char     s_date_end[MAX_EVENTS][DATE_LEN];  // "" = single day
static char     s_time[MAX_EVENTS][TIME_LEN];      // "" = all-day
static char     s_time_end[MAX_EVENTS][TIME_LEN];  // "" = no end / all-day
static char     s_loc[MAX_EVENTS][TEXT_LEN];       // "" = no location
static char     s_cal_of[MAX_EVENTS][FRIJ_CAL_NAME];  // "" = no calendar tag
static time_t   s_synced_at = 0;                   // bridge sync epoch (0 = unknown)
static int      s_n         = 0;

static char     s_cal_name[FRIJ_CAL_MAX][FRIJ_CAL_NAME];
static uint32_t s_cal_color[FRIJ_CAL_MAX];
static int      s_cal_n = 0;

static char     s_off[FRIJ_CAL_MAX][FRIJ_CAL_NAME];  // hidden calendar names
static int      s_off_n = 0;

// ---- parse ------------------------------------------------------------------

static uint32_t parse_color_hex(const char* hex)
{
    if (!hex || !hex[0]) {
        return DEFAULT_COLOR;
    }
    char* end = NULL;
    unsigned long v = strtoul(hex, &end, 16);
    return (end && *end == '\0') ? (uint32_t)v : DEFAULT_COLOR;
}

// Load the hidden-calendar names from store:events_off (a JSON array).
static void load_disabled(void)
{
    s_off_n = 0;
    char buf[256];
    if (!frij_store_load(OFF_KEY, buf, sizeof(buf))) {
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, buf) != DeserializationError::Ok || !doc.is<JsonArray>()) {
        return;
    }
    for (JsonVariant n : doc.as<JsonArray>()) {
        if (s_off_n >= FRIJ_CAL_MAX) {
            break;
        }
        snprintf(s_off[s_off_n++], FRIJ_CAL_NAME, "%s", n.as<const char*>() ? n.as<const char*>() : "");
    }
}

static void reload_raw(void)
{
    s_n         = 0;
    s_cal_n     = 0;
    s_synced_at = 0;
    load_disabled();
    // Big enough for FRIJ_EVENTS_MAX events + the calendar list; static (not on
    // the stack) since 50 events of JSON is ~12 KB. Single-threaded UI use.
    static char buf[16384];
    if (!frij_store_load(STORE_KEY, buf, sizeof(buf))) {
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, buf) != DeserializationError::Ok || !doc.is<JsonObject>()) {
        return;
    }
    s_synced_at = (time_t)(doc["at"] | 0L);

    for (JsonObject c : doc["cal"].as<JsonArray>()) {  // declared calendars
        if (s_cal_n >= FRIJ_CAL_MAX) {
            break;
        }
        const char* name = c["n"] | "";
        if (!name[0]) {
            continue;
        }
        snprintf(s_cal_name[s_cal_n], FRIJ_CAL_NAME, "%s", name);
        s_cal_color[s_cal_n] = parse_color_hex(c["c"] | "");
        s_cal_n++;
    }

    for (JsonObject o : doc["ev"].as<JsonArray>()) {
        if (s_n >= MAX_EVENTS) {
            break;
        }
        const char* t = o["t"] | "";
        const char* d = o["d"] | "";
        if (!t[0] || !d[0]) {
            continue;
        }
        snprintf(s_title[s_n], TEXT_LEN, "%s", t);
        snprintf(s_date[s_n], DATE_LEN, "%s", d);
        snprintf(s_date_end[s_n], DATE_LEN, "%s", o["de"] | "");
        snprintf(s_time[s_n], TIME_LEN, "%s", o["tm"] | "");
        snprintf(s_time_end[s_n], TIME_LEN, "%s", o["te"] | "");
        snprintf(s_loc[s_n], TEXT_LEN, "%s", o["l"] | "");
        snprintf(s_cal_of[s_n], FRIJ_CAL_NAME, "%s", o["c"] | "");
        s_n++;
    }
}

// ---- calendars --------------------------------------------------------------

static int cal_index(const char* name)
{
    for (int i = 0; i < s_cal_n; i++) {
        if (strcmp(s_cal_name[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

static bool cal_hidden(const char* name)
{
    for (int i = 0; i < s_off_n; i++) {
        if (strcmp(s_off[i], name) == 0) {
            return true;
        }
    }
    return false;
}

// ---- day math ---------------------------------------------------------------

// Whole days from today to `date` ("YYYY-MM-DD"); 0 = today, negative = past.
// Both ends anchored at noon so a DST hour can't shift the count.
static int days_until(const char* date)
{
    int y, m, d;
    if (sscanf(date, "%d-%d-%d", &y, &m, &d) != 3) {
        return -1;
    }
    struct tm ev = {};
    ev.tm_year   = y - 1900;
    ev.tm_mon    = m - 1;
    ev.tm_mday   = d;
    ev.tm_hour   = 12;
    time_t    now = time(NULL);
    struct tm today_tm;
    localtime_r(&now, &today_tm);
    today_tm.tm_hour = 12;
    today_tm.tm_min  = 0;
    today_tm.tm_sec  = 0;
    double diff = difftime(mktime(&ev), mktime(&today_tm));
    return (int)((diff + (diff < 0 ? -43200.0 : 43200.0)) / 86400.0);
}

// True once a today event's end time has passed (no point counting down).
static bool ended_today(int idx, int days)
{
    if (days != 0 || !s_time_end[idx][0]) {
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

// Past, over for today, or on a calendar the user has hidden.
static bool hidden(int idx, int days)
{
    return days < 0 || ended_today(idx, days) || cal_hidden(s_cal_of[idx]);
}

static int minutes_until_today(int idx)
{
    int hh, mm;
    if (sscanf(s_time[idx], "%d:%d", &hh, &mm) != 2) {
        return -1;  // all-day
    }
    time_t    now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    return (hh * 60 + mm) - (tmv.tm_hour * 60 + tmv.tm_min);
}

// ---- formatting -------------------------------------------------------------

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

// "12:00 - 13:00" / "12:00" / "all day". ASCII separators only (font subset).
static void time_phrase(char* buf, size_t n, int idx)
{
    char from[16], to[16];
    if (!clock_phrase(from, sizeof(from), s_time[idx])) {
        snprintf(buf, n, "all day");
        return;
    }
    if (clock_phrase(to, sizeof(to), s_time_end[idx])) {
        snprintf(buf, n, "%s - %s", from, to);
    } else {
        snprintf(buf, n, "%s", from);
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

// "Sun 14 Jun, 12:00 - 13:00" / "Wed 23 Sep, all day" / "Sat 04 Jul - 17 Jul".
static void abs_when(char* buf, size_t n, int idx)
{
    char date[20];
    date_fmt(date, sizeof(date), s_date[idx], "%a %d %b");
    if (s_date_end[idx][0]) {
        char last[12];
        date_fmt(last, sizeof(last), s_date_end[idx], "%d %b");
        snprintf(buf, n, "%s - %s", date, last);
        return;
    }
    char clock[36];
    time_phrase(clock, sizeof(clock), idx);
    snprintf(buf, n, "%s, %s", date, clock);
}

// "Today, 12:00 - 13:00" / "Tomorrow" / "In 12 days".
static void rel_when(char* buf, size_t n, int idx, int days)
{
    char when[24];
    if (days <= 0) {
        snprintf(when, sizeof(when), "Today");
    } else if (days == 1) {
        snprintf(when, sizeof(when), "Tomorrow");
    } else {
        snprintf(when, sizeof(when), "In %d days", days);
    }
    if (s_time[idx][0]) {
        char clock[36];
        time_phrase(clock, sizeof(clock), idx);
        snprintf(buf, n, "%s, %s", when, clock);
    } else {
        snprintf(buf, n, "%s", when);
    }
}

// Compact "time until" badge, unit-scaled: "2h"/"3d"/"2w"/"5m"/"1y"/"Now".
static void badge_text(char* buf, size_t n, int idx, int days)
{
    if (days <= 0) {
        int mins = minutes_until_today(idx);
        if (mins > 0) {
            snprintf(buf, n, "%dh", (mins + 59) / 60);  // round up, min 1h
        } else {
            snprintf(buf, n, "Now");
        }
    } else if (days < 7) {
        snprintf(buf, n, "%dd", days);
    } else if (days < 30) {
        snprintf(buf, n, "%dw", days / 7);
    } else if (days < 365) {
        snprintf(buf, n, "%dm", days / 30);  // months
    } else {
        snprintf(buf, n, "%dy", days / 365);
    }
}

static void build_view(int idx, int days, frij_event_view_t* v)
{
    snprintf(v->title, sizeof(v->title), "%s", s_title[idx]);
    snprintf(v->loc, sizeof(v->loc), "%s", s_loc[idx]);
    snprintf(v->cal, sizeof(v->cal), "%s", s_cal_of[idx]);
    badge_text(v->badge, sizeof(v->badge), idx, days);
    abs_when(v->when, sizeof(v->when), idx);
    rel_when(v->rel, sizeof(v->rel), idx, days);
    v->days = days;

    int ci   = cal_index(s_cal_of[idx]);
    v->color = ci >= 0 ? s_cal_color[ci] : DEFAULT_COLOR;
}

// ---- public -----------------------------------------------------------------

void frij_events_sync(void)
{
    frij_store_pull_async(STORE_KEY);
}

int frij_events_load(frij_event_view_t* out, int max)
{
    reload_raw();
    int n = 0;
    for (int i = 0; i < s_n && n < max; i++) {
        int days = days_until(s_date[i]);
        if (hidden(i, days)) {
            continue;
        }
        build_view(i, days, &out[n++]);
    }
    return n;
}

bool frij_events_next(frij_event_view_t* out)
{
    reload_raw();
    for (int i = 0; i < s_n; i++) {
        int days = days_until(s_date[i]);
        if (!hidden(i, days)) {
            build_view(i, days, out);
            return true;
        }
    }
    return false;
}

bool frij_events_synced_ago(char* buf, size_t n)
{
    if (s_synced_at <= 0) {
        return false;
    }
    char ago[24];
    frij_format_relative(ago, sizeof(ago), s_synced_at);
    snprintf(buf, n, "Updated %s", ago);
    return true;
}

int frij_events_calendars(frij_calendar_t* out, int max)
{
    reload_raw();
    int n = 0;
    for (int i = 0; i < s_cal_n && n < max; i++) {
        snprintf(out[n].name, FRIJ_CAL_NAME, "%s", s_cal_name[i]);
        out[n].color   = s_cal_color[i];
        out[n].enabled = !cal_hidden(s_cal_name[i]);
        n++;
    }
    return n;
}

void frij_events_set_calendar(const char* name, bool on)
{
    load_disabled();
    // Rebuild the hidden list: drop `name`, then append it if hiding.
    JsonDocument doc;
    JsonArray    arr = doc.to<JsonArray>();
    for (int i = 0; i < s_off_n; i++) {
        if (strcmp(s_off[i], name) != 0) {
            arr.add(s_off[i]);
        }
    }
    if (!on) {
        arr.add(name);
    }
    char buf[256];
    serializeJson(doc, buf, sizeof(buf));
    frij_store_save(OFF_KEY, buf);
}
