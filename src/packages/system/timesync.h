#ifndef FRIJ_TIMESYNC_H
#define FRIJ_TIMESYNC_H

// Network time. The device's RTC/system clock boots unset (near epoch), so the
// watch face + events show the wrong time until this runs. Device: start SNTP
// and set the POSIX timezone, so the clock self-corrects once Wi-Fi is up and
// localtime_r() returns local wall time. Emulator: no-op (the host clock is
// already correct + local). Safe to call before Wi-Fi connects — SNTP retries.
void frij_time_sync_init(void);

#endif  // FRIJ_TIMESYNC_H
