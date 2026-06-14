#ifndef FRIJ_DATA_EVENTS_H
#define FRIJ_DATA_EVENTS_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Events data layer — the seam between the Events UI and the store/cloud.
 *
 * The app (apps/events) is pure UI: it never touches the store or knows the
 * "events" JSON shape. It calls these functions and lays out the returned
 * view structs (display-ready strings + a few flags). All parsing, the
 * {"at","ev"} payload shape, day math, badge units, and time formatting live
 * here. No LVGL/UI dependency — only store + core/datetime.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define FRIJ_EVENTS_MAX  10  // most events the list/load returns
#define FRIJ_EVENT_TITLE 64
#define FRIJ_EVENT_LOC   64

// One event, formatted for display.
typedef struct {
    char title[FRIJ_EVENT_TITLE];
    char badge[8];                 // "3h" / "3d" / "2w" / "5m" / "1y" / "Now"
    char when[56];                 // absolute: "Sat 13 Jun, 12:00 - 13:00" / ", all day" / span
    char rel[40];                  // relative: "Today, 12:00 - 13:00" / "Tomorrow" / "In 3 days"
    char loc[FRIJ_EVENT_LOC];      // "" if none
    int  days;                     // 0 = today … (section bucket + pulse + countdown)
    bool holiday;                  // gray badge vs the app accent
} frij_event_view_t;

// Kick a background cloud refresh (cache updates for the next load). Non-blocking.
void frij_events_sync(void);

// Fill `out` with the visible upcoming events (past / ended-today dropped),
// soonest first, capped to `max`. Returns the count. Reads the cache only.
int frij_events_load(frij_event_view_t* out, int max);

// The next family (non-holiday) event, for the countdown screen. False if none.
bool frij_events_next_family(frij_event_view_t* out);

// "Updated 5m ago" into `buf`; false if the sync time is unknown.
bool frij_events_synced_ago(char* buf, size_t n);

#ifdef __cplusplus
}
#endif

#endif  // FRIJ_DATA_EVENTS_H
