#include "couples.h"

#include <stdio.h>
#include <time.h>

#include <ArduinoJson.h>

// Milestone dates (YYYY-MM-DD) baked from the shell/.env at build time (see
// platformio.ini + .env.example). Empty when unset → the screen shows "Not set".
#ifndef FRIJ_TOGETHER_SINCE
#define FRIJ_TOGETHER_SINCE ""
#endif
#ifndef FRIJ_MARRIED_SINCE
#define FRIJ_MARRIED_SINCE ""
#endif

#include "store/store.h"
#include "system/haptics.h"
#include "ui/anim.h"
#include "ui/components.h"
#include "ui/theme.h"

/*
 * Couples — a gentle "fight tracker" for two people sharing the display. Once a
 * day you answer "Did we fight today?": tapping No changes nothing (it just
 * acknowledges a calm day), tapping Yes logs the day. The framing stays positive
 * — the hero number is the *peace streak* (days since the last fight), not a
 * blame counter — and a logged day can be undone in case of a mis-tap.
 *
 *   glance   : peace streak ("12 days since last fight") / calm empty state
 *   screen 0 : "Did we fight today?" — No / Yes pills; Yes flips to a logged
 *              state with an Undo
 *   screen 1 : stats — this week / this month / since last + a 7-day dot strip;
 *              the header has a clear-history (confirm) action
 *
 * Data: one entry per logged day, stored as a local-midnight *day index*
 * (epoch/86400 — small ints, no 2038 overflow) in a JSON array under `couples_
 * fights`, deduped per day and capped. Persists + cloud-syncs like the others.
 */

static const uint32_t ACCENT = FRIJ_PINK;
static const char*    KEY     = "couples_fights";    // logged fight days
static const char*    RKEY    = "couples_resolved";  // subset of KEY marked "made up"
#define MAX_DAYS 200            // ~6 months of logged days; oldest drop off
#define JSON_BUF 2048           // 200 day-ints + brackets/commas fits easily

// ---- data ------------------------------------------------------------------

// Local-midnight day index for "now" (days since the epoch in local time).
static int today_index(void)
{
    time_t    t  = time(NULL);
    struct tm lt = *localtime(&t);
    lt.tm_hour = 0;
    lt.tm_min  = 0;
    lt.tm_sec  = 0;
    return (int)(mktime(&lt) / 86400);
}

// Load a JSON array of day indices from `key` into `out` (ascending on save).
static int load_arr(const char* key, int* out, int max)
{
    char buf[JSON_BUF];
    if (!frij_store_load(key, buf, sizeof(buf))) {
        return 0;
    }
    JsonDocument doc;
    if (deserializeJson(doc, buf) != DeserializationError::Ok || !doc.is<JsonArray>()) {
        return 0;
    }
    int n = 0;
    for (JsonVariant v : doc.as<JsonArray>()) {
        if (n < max) {
            out[n++] = v.as<int>();
        }
    }
    return n;
}

static void save_arr(const char* key, const int* days, int count)
{
    JsonDocument doc;
    JsonArray    arr = doc.to<JsonArray>();
    for (int i = 0; i < count; i++) {
        arr.add(days[i]);
    }
    char out[JSON_BUF];
    serializeJson(doc, out, sizeof(out));
    frij_store_save(key, out);
}

static int  load_days(int* out, int max) { return load_arr(KEY, out, max); }
static void save_days(const int* d, int n) { save_arr(KEY, d, n); }

static void set_resolved_today(bool on);  // defined with the resolved helpers below

static bool has_day(const int* days, int n, int day)
{
    for (int i = 0; i < n; i++) {
        if (days[i] == day) {
            return true;
        }
    }
    return false;
}

static bool logged_today(void)
{
    int days[MAX_DAYS];
    int n = load_days(days, MAX_DAYS);
    return has_day(days, n, today_index());
}

// Log today (idempotent — one entry per day). Drops the oldest day if capped.
static void log_today(void)
{
    int days[MAX_DAYS];
    int n   = load_days(days, MAX_DAYS);
    int day = today_index();
    if (has_day(days, n, day)) {
        return;
    }
    if (n == MAX_DAYS) {  // make room: drop the oldest (first) entry
        for (int i = 1; i < n; i++) {
            days[i - 1] = days[i];
        }
        n--;
    }
    days[n++] = day;  // today is the newest, stays at the end (ascending)
    save_days(days, n);
}

static void unlog_today(void)
{
    int days[MAX_DAYS];
    int n   = load_days(days, MAX_DAYS);
    int day = today_index();
    int w   = 0;
    for (int i = 0; i < n; i++) {
        if (days[i] != day) {
            days[w++] = days[i];
        }
    }
    save_days(days, w);
    set_resolved_today(false);  // an undone fight can't be "made up"
}

// Count logged days within the last `span` calendar days (today inclusive).
static int count_window(const int* days, int n, int span)
{
    int today  = today_index();
    int oldest = today - (span - 1);
    int c      = 0;
    for (int i = 0; i < n; i++) {
        if (days[i] >= oldest && days[i] <= today) {
            c++;
        }
    }
    return c;
}

// Days since the most recent fight: 0 if logged today, -1 if never logged.
static int peace_streak(const int* days, int n)
{
    if (n == 0) {
        return -1;
    }
    int latest = days[0];
    for (int i = 1; i < n; i++) {
        if (days[i] > latest) {
            latest = days[i];
        }
    }
    return today_index() - latest;
}

// Longest run of consecutive peaceful days we can observe — the gaps between
// logged fights, plus the current run (last fight → today). -1 if none logged.
// (We can't know peace before the first ever log, so that span is ignored.)
static int best_streak(const int* days, int n)
{
    if (n == 0) {
        return -1;
    }
    int s[MAX_DAYS];  // sort a copy ascending (n is small)
    for (int i = 0; i < n; i++) {
        s[i] = days[i];
    }
    for (int i = 1; i < n; i++) {
        int v = s[i], j = i - 1;
        while (j >= 0 && s[j] > v) {
            s[j + 1] = s[j];
            j--;
        }
        s[j + 1] = v;
    }
    int best = today_index() - s[n - 1];  // current streak
    for (int i = 1; i < n; i++) {
        int gap = s[i] - s[i - 1] - 1;  // peaceful days strictly between two fights
        if (gap > best) {
            best = gap;
        }
    }
    return best;
}

// Count logged days in the inclusive day-index range [from, to].
static int count_range(const int* days, int n, int from, int to)
{
    int c = 0;
    for (int i = 0; i < n; i++) {
        if (days[i] >= from && days[i] <= to) {
            c++;
        }
    }
    return c;
}

// ---- resolved ("made up") set ----------------------------------------------

static bool resolved_today(void)
{
    int r[MAX_DAYS];
    int n = load_arr(RKEY, r, MAX_DAYS);
    return has_day(r, n, today_index());
}

static void set_resolved_today(bool on)
{
    int  r    = today_index();
    int  set[MAX_DAYS];
    int  n    = load_arr(RKEY, set, MAX_DAYS);
    bool have = has_day(set, n, r);
    if (on && !have) {
        if (n == MAX_DAYS) {  // make room: drop the oldest
            for (int i = 1; i < n; i++) {
                set[i - 1] = set[i];
            }
            n--;
        }
        set[n++] = r;
    } else if (!on && have) {
        int w = 0;
        for (int i = 0; i < n; i++) {
            if (set[i] != r) {
                set[w++] = set[i];
            }
        }
        n = w;
    } else {
        return;  // already in the wanted state
    }
    save_arr(RKEY, set, n);
}

// How many of the logged fights were marked "made up".
static int resolved_count(const int* days, int n)
{
    int r[MAX_DAYS];
    int rn = load_arr(RKEY, r, MAX_DAYS);
    int c  = 0;
    for (int i = 0; i < n; i++) {
        if (has_day(r, rn, days[i])) {
            c++;
        }
    }
    return c;
}

// ---- glance ----------------------------------------------------------------

static void glance(lv_obj_t* parent)
{
    int days[MAX_DAYS];
    int n      = load_days(days, MAX_DAYS);
    int streak = peace_streak(days, n);

    lv_obj_t* col = frij_page(parent);
    frij_label(col, "Couples", FRIJ_FONT_BODY, FRIJ_TEXT_2);

    if (n == 0) {  // nothing logged yet — calm, no number
        frij_label(col, "All good", FRIJ_FONT_DISPLAY, ACCENT);
        frij_label(col, "No fights logged", FRIJ_FONT_BODY, FRIJ_TEXT_2);
    } else if (streak == 0) {  // logged today
        frij_label(col, "Today", FRIJ_FONT_DISPLAY, ACCENT);
        frij_label(col, "Fight logged for today", FRIJ_FONT_BODY, FRIJ_TEXT_2);
    } else {
        lv_obj_t* big = frij_label(col, "", FRIJ_FONT_CLOCK, ACCENT);
        lv_label_set_text_fmt(big, "%d", streak);
        char sub[40];
        lv_snprintf(sub, sizeof(sub), "%s since last fight", streak == 1 ? "day" : "days");
        frij_label(col, sub, FRIJ_FONT_BODY, FRIJ_TEXT_2);
    }
}

// ---- screen 0: "Did we fight today?" ---------------------------------------

static lv_obj_t* s_choice = NULL;  // the screen-0 body, rebuilt in place on log/undo
static void build_choice(void);

// A full-width tappable pill with a centered label and press feedback + haptic.
static lv_obj_t* pill(lv_obj_t* parent, const char* text, uint32_t bg, uint32_t fg,
                      lv_event_cb_t cb)
{
    lv_obj_t* b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, LV_PCT(74), 64);
    lv_obj_set_style_radius(b, FRIJ_RADIUS_L, LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, lv_color_hex(bg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(b, LV_OPA_80, LV_PART_MAIN | LV_STATE_PRESSED);  // press dim
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* l = frij_label(b, text, FRIJ_FONT_TITLE, fg);
    lv_obj_center(l);

    frij_haptic_attach(b);
    if (cb) {
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    }
    return b;
}

static void on_no(lv_event_t* e)
{
    (void)e;
    frij_toast("Another peaceful day");  // No has no side effect — just acknowledge
}

static void on_yes(lv_event_t* e)
{
    (void)e;
    log_today();
    frij_haptic(FRIJ_HAPTIC_SUCCESS);
    frij_toast("Logged. Be kind to each other");
    build_choice();  // flip to the logged state
}

static void on_undo(lv_event_t* e)
{
    (void)e;
    unlog_today();
    frij_haptic(FRIJ_HAPTIC_SELECT);
    frij_toast("Undone");
    build_choice();  // back to the question
}

static void on_resolved(lv_event_t* e)
{
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    set_resolved_today(lv_obj_has_state(sw, LV_STATE_CHECKED));
    frij_haptic(FRIJ_HAPTIC_SELECT);
}

// (Re)fill the screen-0 body for the current state. Called on open and after a
// log/undo so the screen reflects today without leaving + reopening.
static void build_choice(void)
{
    if (!s_choice) {
        return;
    }
    lv_obj_clean(s_choice);

    if (logged_today()) {
        // Calm "done for today" state with a way back out.
        frij_circle_button(s_choice, 72, ACCENT, LV_SYMBOL_OK, FRIJ_FONT_SYMBOL_L, 0xFFFFFF, NULL);
        lv_obj_t* t = frij_label(s_choice, "Logged for today", FRIJ_FONT_TITLE, FRIJ_TEXT);
        lv_obj_set_style_margin_top(t, FRIJ_SP_S, LV_PART_MAIN);
        // Did you make up? A resolved-same-day flag (feeds the stats rate).
        lv_obj_t* sw = frij_toggle_row(s_choice, "Made up", resolved_today(), ACCENT);
        lv_obj_add_event_cb(sw, on_resolved, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_t* undo = pill(s_choice, "Undo", FRIJ_SURFACE_2, FRIJ_TEXT, on_undo);
        lv_obj_set_style_margin_top(undo, FRIJ_SP_S, LV_PART_MAIN);
    } else {
        frij_label(s_choice, "Did we fight today?", FRIJ_FONT_TITLE, FRIJ_TEXT);
        lv_obj_t* gap = frij_label(s_choice, "Only a yes is recorded", FRIJ_FONT_SMALL, FRIJ_TEXT_3);
        lv_obj_set_style_margin_bottom(gap, FRIJ_SP_S, LV_PART_MAIN);
        pill(s_choice, "No", FRIJ_SURFACE_2, FRIJ_TEXT, on_no);
        pill(s_choice, "Yes", ACCENT, 0xFFFFFF, on_yes);
    }
    frij_page_settle(s_choice);  // re-center after a state change (log/undo)
    frij_stagger_in(s_choice, 60);
}

static void on_choice_delete(lv_event_t* e)
{
    (void)e;
    s_choice = NULL;  // the body is being torn down; drop the dangling pointer
}

static void choice_screen(lv_obj_t* parent)
{
    frij_store_pull_async(KEY);  // best-effort cloud refresh (visible next read)

    lv_obj_t* col = frij_page(parent);
    lv_obj_set_style_pad_row(col, FRIJ_SP_M, LV_PART_MAIN);
    s_choice = col;
    lv_obj_add_event_cb(col, on_choice_delete, LV_EVENT_DELETE, NULL);
    build_choice();
    frij_page_settle(col);
}

// ---- screen 1: stats -------------------------------------------------------

// A row of 7 dots for the last week: pink = fight, muted = peace, today ringed.
static void week_strip(lv_obj_t* parent, const int* days, int n)
{
    int today = today_index();
    lv_obj_t* row = frij_col(parent, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, FRIJ_SP_S, LV_PART_MAIN);
    for (int i = 6; i >= 0; i--) {
        int       day = today - i;
        bool      fought = has_day(days, n, day);
        lv_obj_t* dot = lv_obj_create(row);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, 18, 18);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(dot, lv_color_hex(fought ? ACCENT : FRIJ_SURFACE_3),
                                  LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
        if (day == today) {  // mark today with an accent ring
            lv_obj_set_style_border_width(dot, 2, LV_PART_MAIN);
            lv_obj_set_style_border_color(dot, lv_color_hex(ACCENT), LV_PART_MAIN);
        }
    }
}

static void count_row(lv_obj_t* col, const char* label, int count)
{
    char v[16];
    lv_snprintf(v, sizeof(v), "%d", count);
    frij_value_row(col, label, v);
}

static void stats_screen(lv_obj_t* parent)
{
    int days[MAX_DAYS];
    int n      = load_days(days, MAX_DAYS);
    int streak = peace_streak(days, n);

    lv_obj_t* col = frij_page(parent);
    lv_obj_set_style_pad_row(col, FRIJ_SP_S, LV_PART_MAIN);

    // Hero: the peace streak (positive framing), or a calm empty line.
    if (n == 0) {
        frij_label(col, "All good", FRIJ_FONT_DISPLAY, ACCENT);
        frij_label(col, "No fights logged yet", FRIJ_FONT_BODY, FRIJ_TEXT_2);
    } else {
        lv_obj_t* big = frij_label(col, "", FRIJ_FONT_DISPLAY, ACCENT);
        lv_label_set_text_fmt(big, "%d", streak < 0 ? 0 : streak);
        frij_label(col, streak == 0 ? "fight logged today" : "days since last fight",
                   FRIJ_FONT_SMALL, FRIJ_TEXT_2);
    }

    week_strip(col, days, n);
    lv_obj_t* cap = frij_label(col, "Last 7 days", FRIJ_FONT_SMALL, FRIJ_TEXT_3);
    lv_obj_set_style_margin_bottom(cap, FRIJ_SP_S, LV_PART_MAIN);

    count_row(col, "This week", count_window(days, n, 7));
    count_row(col, "This month", count_window(days, n, 30));

    if (n > 0) {
        char v[16];
        int  best = best_streak(days, n);
        lv_snprintf(v, sizeof(v), "%d %s", best, best == 1 ? "day" : "days");
        frij_value_row(col, "Best streak", v);

        lv_snprintf(v, sizeof(v), "%d of %d", resolved_count(days, n), n);
        frij_value_row(col, "Made up", v);
    }
    count_row(col, "Total logged", n);

    // Month-over-month recap: this 30-day window vs the previous one.
    int today = today_index();
    int now30 = count_range(days, n, today - 29, today);
    int prev30 = count_range(days, n, today - 59, today - 30);
    if (now30 + prev30 > 0) {
        const char* trend = now30 < prev30  ? "Fewer fights than last month"
                            : now30 > prev30 ? "More fights than last month"
                                             : "Same as last month";
        lv_obj_t* r = frij_label(col, trend, FRIJ_FONT_SMALL, FRIJ_TEXT_3);
        lv_obj_set_style_margin_top(r, FRIJ_SP_S, LV_PART_MAIN);
    }

    frij_page_settle(col);
    frij_stagger_in(col, 40);
}

// ---- clear history (header action on stats) --------------------------------

static void do_clear(lv_event_t* e)
{
    (void)e;
    save_days(NULL, 0);          // empty the fight log
    save_arr(RKEY, NULL, 0);     // and the made-up set
    frij_haptic(FRIJ_HAPTIC_SUCCESS);
    frij_toast_status("History cleared", true);
}

static const char* couples_action(int index)
{
    return index == 1 ? LV_SYMBOL_TRASH : NULL;  // clear only on the stats screen
}

static void couples_on_action(int index)
{
    if (index != 1) {
        return;
    }
    frij_prompt_screen(LV_SYMBOL_TRASH, FRIJ_DANGER, "Clear history?",
                       "Forget every logged day. Can't be undone.", "Clear", "Cancel", do_clear);
}

// ---- screen 2: "together since" / "married since" --------------------------

static int days_in_month(int y, int m)  // m is 1..12
{
    static const int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2) {
        bool leap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
        return leap ? 29 : 28;
    }
    return d[m - 1];
}

static bool parse_date(const char* s, int* y, int* m, int* d)
{
    return s && s[0] && sscanf(s, "%d-%d-%d", y, m, d) == 3;
}

// Calendar-correct elapsed years/months/days from `iso` to today. False if the
// date is unset/malformed or in the future.
static bool elapsed_since(const char* iso, int* yy, int* mm, int* dd)
{
    int sy, sm, sd;
    if (!parse_date(iso, &sy, &sm, &sd)) {
        return false;
    }
    time_t    t  = time(NULL);
    struct tm lt = *localtime(&t);
    int       ty = lt.tm_year + 1900, tm = lt.tm_mon + 1, td = lt.tm_mday;
    if (sy > ty || (sy == ty && (sm > tm || (sm == tm && sd > td)))) {
        return false;  // future date
    }
    int years = ty - sy, months = tm - sm, days = td - sd;
    if (days < 0) {  // borrow days from the previous month
        int pm = tm - 1, py = ty;
        if (pm == 0) {
            pm = 12;
            py--;
        }
        days += days_in_month(py, pm);
        months--;
    }
    if (months < 0) {  // borrow a year
        months += 12;
        years--;
    }
    *yy = years;
    *mm = months;
    *dd = days;
    return true;
}

// "1 year 2 months 5 days" — drops zero leading parts, pluralizes, always days.
static void fmt_elapsed(char* buf, size_t n, int y, int m, int d)
{
    int off = 0;
    if (y > 0) {
        off += lv_snprintf(buf + off, n - off, "%d %s ", y, y == 1 ? "year" : "years");
    }
    if (y > 0 || m > 0) {
        off += lv_snprintf(buf + off, n - off, "%d %s ", m, m == 1 ? "month" : "months");
    }
    lv_snprintf(buf + off, n - off, "%d %s", d, d == 1 ? "day" : "days");
}

static void milestone(lv_obj_t* col, const char* label, const char* iso, bool first)
{
    // The name is the hero (big + pink); the elapsed duration sits muted below it.
    lv_obj_t* title = frij_label(col, label, FRIJ_FONT_DISPLAY, ACCENT);
    if (!first) {  // breathing room between the two milestone blocks
        lv_obj_set_style_margin_top(title, FRIJ_SP_XXL, LV_PART_MAIN);
    }
    int  y, m, d;
    char dur[48];
    bool ok = elapsed_since(iso, &y, &m, &d);
    if (ok) {
        fmt_elapsed(dur, sizeof(dur), y, m, d);
    }
    lv_obj_t* v = frij_label(col, ok ? dur : "Not set", FRIJ_FONT_BODY,
                             ok ? FRIJ_TEXT_2 : FRIJ_TEXT_3);
    lv_obj_set_width(v, LV_PCT(90));
    lv_label_set_long_mode(v, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
}

static void since_screen(lv_obj_t* parent)
{
    lv_obj_t* col = frij_page(parent);
    lv_obj_set_style_pad_row(col, FRIJ_SP_XS, LV_PART_MAIN);
    milestone(col, "Together", FRIJ_TOGETHER_SINCE, true);
    milestone(col, "Married", FRIJ_MARRIED_SINCE, false);
    frij_page_settle(col);
    frij_stagger_in(col, 60);
}

// ---- app contract ----------------------------------------------------------

static void screen(lv_obj_t* parent, int index)
{
    if (index == 1) {
        stats_screen(parent);
    } else if (index == 2) {
        since_screen(parent);
    } else {
        choice_screen(parent);
    }
}

const frij_app_t* couples_app(void)
{
    static const frij_app_t app = {"Couples",        ACCENT, glance, 3, screen,
                                   couples_action, couples_on_action};
    return &app;
}
