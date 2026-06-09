#include "datetime.h"

#include "lvgl.h"  // lv_snprintf

#include "store/store.h"

bool frij_clock_is_24h(void)
{
    return frij_store_load_bool("clock24", true);
}

void frij_format_time(char* buf, size_t n, const struct tm* tmv)
{
    if (frij_clock_is_24h()) {
        strftime(buf, n, "%H:%M", tmv);
        return;
    }
    int hour12 = tmv->tm_hour % 12;
    if (hour12 == 0) {
        hour12 = 12;  // 0/12 -> 12
    }
    lv_snprintf(buf, n, "%d:%02d %s", hour12, tmv->tm_min, tmv->tm_hour < 12 ? "AM" : "PM");
}

void frij_format_relative(char* buf, size_t n, time_t then)
{
    long diff = (long)(time(NULL) - then);
    if (diff < 0) {
        diff = 0;  // clock skew — treat a future stamp as "now"
    }
    if (diff < 45) {
        lv_snprintf(buf, n, "Just now");
    } else if (diff < 3600) {
        lv_snprintf(buf, n, "%ldm ago", diff / 60);
    } else if (diff < 86400) {
        lv_snprintf(buf, n, "%ldh ago", diff / 3600);
    } else if (diff < 7 * 86400) {
        lv_snprintf(buf, n, "%ldd ago", diff / 86400);
    } else {
        struct tm tmv;
        localtime_r(&then, &tmv);
        frij_format_time(buf, n, &tmv);  // older than a week: show the clock time
    }
}
