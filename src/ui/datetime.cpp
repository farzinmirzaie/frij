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
