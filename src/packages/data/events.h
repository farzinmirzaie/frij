#ifndef FRIJ_DATA_EVENTS_H
#define FRIJ_DATA_EVENTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Events data layer — the seam between the Events UI and the store/cloud.
 *
 * The app (apps/events) is pure UI: it never touches the store or knows the
 * "events" JSON shape. It calls these functions and lays out the returned
 * view structs (display-ready strings + a few flags). All parsing, the
 * {"at","cal","ev"} payload shape, day math, badge units, time formatting, and
 * the per-calendar on/off toggles live here. No LVGL/UI dependency — only
 * store + core/datetime.
 *
 * Events come from one or more calendars (declared off-device as GCALENDAR_*;
 * see packages/bridge), all treated alike. Each event carries the color of its
 * calendar, and the user can hide a whole calendar from the Calendars screen —
 * the hidden set persists in store:events_off and applies to the list, glance,
 * and countdown.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define FRIJ_EVENTS_MAX  50  // most events the list/load returns (~a year's worth)
#define FRIJ_EVENT_TITLE 64
#define FRIJ_EVENT_LOC   64
#define FRIJ_CAL_MAX     8   // most calendars (matches the bridge MAX_CALS)
#define FRIJ_CAL_NAME    32

// One event, formatted for display.
typedef struct {
    char     title[FRIJ_EVENT_TITLE];
    char     badge[8];                 // "3h" / "3d" / "2w" / "5m" / "1y" / "Now"
    char     when[56];                 // absolute: "Sat 13 Jun, 12:00 - 13:00" / ", all day" / span
    char     rel[40];                  // relative: "Today, 12:00 - 13:00" / "Tomorrow" / "In 3 days"
    char     loc[FRIJ_EVENT_LOC];      // "" if none
    uint32_t color;                    // the event's calendar color (0xRRGGBB)
    int      days;                     // 0 = today … (section bucket + pulse + countdown)
} frij_event_view_t;

// One calendar, for the Calendars toggle screen.
typedef struct {
    char     name[FRIJ_CAL_NAME];
    uint32_t color;                    // 0xRRGGBB
    bool     enabled;                  // user's show/hide toggle (default on)
} frij_calendar_t;

// Kick a background cloud refresh (cache updates for the next load). Non-blocking.
void frij_events_sync(void);

// Fill `out` with the visible upcoming events (past / ended-today / hidden-
// calendar dropped), soonest first, capped to `max`. Returns the count.
int frij_events_load(frij_event_view_t* out, int max);

// The next visible upcoming event, for the countdown. False if none.
bool frij_events_next(frij_event_view_t* out);

// "Updated 5m ago" into `buf`; false if the sync time is unknown.
bool frij_events_synced_ago(char* buf, size_t n);

// Fill `out` with every declared calendar + its current on/off state, capped to
// `max`. Returns the count.
int frij_events_calendars(frij_calendar_t* out, int max);

// Show/hide a calendar by name; persists to store:events_off (applies to the
// list, glance, and countdown on the next load).
void frij_events_set_calendar(const char* name, bool on);

#ifdef __cplusplus
}
#endif

#endif  // FRIJ_DATA_EVENTS_H
