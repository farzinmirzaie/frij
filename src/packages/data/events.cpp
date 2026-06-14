#include "events.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <ArduinoJson.h>

#include "core/datetime.h"
#include "store/store.h"

/*
 * Events data layer — see events.h. Parses store:events, computes day math,
 * and formats display-ready view structs. No UI/LVGL dependency.
 *
 * Store shape under key "events", soonest first:
 *   {"at": <sync epoch>, "ev": [{"t":"Dentist","d":"2026-06-14","tm":"09:30",
 *    "te":"10:30","l":"Qualiteeth"}, {"t":"Hari Raya","d":"...","h":true}, ...]}
 * ("tm"/"te" = start/end clock, absent for all-day; "de" = inclusive end date
 * for multi-day all-day events; "l" = location; "h" = holiday. A bare array,
 * the pre-"at" shape, still loads.)
 */

#define MAX_EVENTS FRIJ_EVENTS_MAX
#define TEXT_LEN   FRIJ_EVENT_TITLE
#define DATE_LEN   11  // "YYYY-MM-DD"
#define TIME_LEN   6   // "HH:MM"
#define STORE_KEY  "events"

static char   s_title[MAX_EVENTS][TEXT_LEN];
static char   s_date[MAX_EVENTS][DATE_LEN];
static char   s_date_end[MAX_EVENTS][DATE_LEN];  // "" = single day
static char   s_time[MAX_EVENTS][TIME_LEN];      // "" = all-day
static char   s_time_end[MAX_EVENTS][TIME_LEN];  // "" = no end / all-day
static char   s_loc[MAX_EVENTS][TEXT_LEN];       // "" = no location
static bool   s_holiday[MAX_EVENTS];
static time_t s_synced_at = 0;                   // bridge sync epoch (0 = unknown)
static int    s_n         = 0;

// ---- parse ------------------------------------------------------------------

static void reload_raw(void)
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
        snprintf(s_title[s_n], TEXT_LEN, "%s", t);
        snprintf(s_date[s_n], DATE_LEN, "%s", d);
        snprintf(s_date_end[s_n], DATE_LEN, "%s", o["de"] | "");
        snprintf(s_time[s_n], TIME_LEN, "%s", o["tm"] | "");
        snprintf(s_time_end[s_n], TIME_LEN, "%s", o["te"] | "");
        snprintf(s_loc[s_n], TEXT_LEN, "%s", o["l"] | "");
        s_holiday[s_n] = o["h"] | false;
        s_n++;
    }
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

static bool hidden(int idx, int days)
{
    return days < 0 || ended_today(idx, days);  // past, or over for today
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
    badge_text(v->badge, sizeof(v->badge), idx, days);
    abs_when(v->when, sizeof(v->when), idx);
    rel_when(v->rel, sizeof(v->rel), idx, days);
    v->days    = days;
    v->holiday = s_holiday[idx];
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

bool frij_events_next_family(frij_event_view_t* out)
{
    reload_raw();
    for (int i = 0; i < s_n; i++) {
        int days = days_until(s_date[i]);
        if (!s_holiday[i] && !hidden(i, days)) {
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
