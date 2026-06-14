#include "storage.h"

#include "lvgl.h"  // lv_snprintf

/*
 * Board-specific behind the usual guard: the emulator mocks the StopWatch's
 * 16MB flash; the device reads the real chip + firmware sizes via the Arduino
 * ESP class. (When the on-device store grows a filesystem, its usage can be
 * added on top of the firmware here.)
 */
#if defined(__has_include) && (__has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>))

bool frij_storage_kb(uint32_t* used_kb, uint32_t* total_kb)
{
    *total_kb = 16u * 1024u;  // mock the 16MB flash
    *used_kb  = 1320u;        // ~the firmware image
    return true;
}

#else

#include <Arduino.h>

bool frij_storage_kb(uint32_t* used_kb, uint32_t* total_kb)
{
    uint32_t total = ESP.getFlashChipSize();
    if (total == 0) {
        return false;
    }
    *total_kb = total / 1024u;
    *used_kb  = ESP.getSketchSize() / 1024u;  // installed firmware
    return true;
}

#endif

void frij_storage_free_str(char* buf, size_t n)
{
    uint32_t used = 0, total = 0;
    if (!frij_storage_kb(&used, &total) || used > total) {
        lv_snprintf(buf, n, "-");
        return;
    }
    uint32_t free_kb = total - used;
    if (free_kb >= 1024u) {  // "12.3 MB free" (integer math — no float printf)
        uint32_t mb10 = free_kb * 10u / 1024u;
        lv_snprintf(buf, n, "%u.%u MB free", (unsigned)(mb10 / 10u), (unsigned)(mb10 % 10u));
    } else {
        lv_snprintf(buf, n, "%u KB free", (unsigned)free_kb);
    }
}
