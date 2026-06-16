#include "timesync.h"

#if defined(__has_include) && (__has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>))

// Emulator: the host clock is already set + in the local timezone.
void frij_time_sync_init(void) {}

#else

#include <Arduino.h>  // configTzTime (esp32-hal-time)

// POSIX TZ string. Default: Malaysia (UTC+8, no DST). Override per build with
// -DFRIJ_TZ="..." (e.g. Singapore "<+08>-8", London "GMT0BST,M3.5.0/1,M10.5.0").
#ifndef FRIJ_TZ
#define FRIJ_TZ "<+08>-8"
#endif

void frij_time_sync_init(void)
{
    // Starts SNTP in the background (syncs a second or two after the network is
    // up) and sets TZ + tzset() so localtime_r() returns local time. The watch
    // face re-reads time() every second, so it corrects itself on the next tick.
    configTzTime(FRIJ_TZ, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
}

#endif
