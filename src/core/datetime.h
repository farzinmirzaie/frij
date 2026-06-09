#ifndef FRIJ_DATETIME_H
#define FRIJ_DATETIME_H

#include <stddef.h>
#include <time.h>

/*
 * Shared date/time formatting. The single source of truth for the user's
 * "24-hour time" setting, so every screen (watch face, Last sync, …) renders
 * times the same way and reacts to the toggle.
 */

// The "24-hour time" preference (defaults to true when unset).
bool frij_clock_is_24h(void);

// Format `tmv` as a clock string into `buf`: "14:30" (24h) or "2:30 PM" (12h).
void frij_format_time(char* buf, size_t n, const struct tm* tmv);

// Format a coarse "time ago" for `then` relative to now into `buf`:
// "Just now", "5m ago", "3h ago", "2d ago". For timestamps older than a week
// it falls back to the clock time. `then` is a unix epoch (seconds).
void frij_format_relative(char* buf, size_t n, time_t then);

#endif  // FRIJ_DATETIME_H
