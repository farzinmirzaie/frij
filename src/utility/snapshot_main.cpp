/*
 * Headless snapshot tool: renders the real UI to an offscreen LVGL display and
 * dumps /tmp/frij_snapshot.bmp — no window server needed. Lets the UI be
 * verified visually in environments where screencapture can't access the display.
 *
 * Build + run:  pio run -e snapshot && .pio/build/snapshot/program
 * Then convert: sips -s format png /tmp/frij_snapshot.bmp --out /tmp/frij_snapshot.png
 */
#if defined(FRIJ_SNAPSHOT)

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#include "lvgl.h"

extern void user_app(void);

extern "C" {
bool lvgl_port_lock(void)
{
    return true;
}
void lvgl_port_unlock(void) {}
}

static const int RES = 466;
static uint8_t   s_buf[RES * RES * 4];  // render buffer (oversized for any cf)

static uint32_t s_tick_offset = 0;  // lets us jump time forward to finish anims

static uint32_t tick_cb(void)
{
    using namespace std::chrono;
    static auto t0 = steady_clock::now();
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now() - t0).count() + s_tick_offset;
}

static void flush_cb(lv_display_t* d, const lv_area_t*, uint8_t*)
{
    lv_display_flush_ready(d);
}

// Write an ARGB8888 buffer (memory order B,G,R,A) as a 24-bit BMP.
static void write_bmp(const char* path, const uint8_t* px, int w, int h, int stride)
{
    FILE* f = fopen(path, "wb");
    if (!f) {
        return;
    }
    int     rowsz  = (w * 3 + 3) & ~3;
    int     imgsz  = rowsz * h;
    uint8_t hdr[54] = {0};
    hdr[0] = 'B';
    hdr[1] = 'M';
    *(uint32_t*)(hdr + 2)  = 54 + imgsz;
    *(uint32_t*)(hdr + 10) = 54;
    *(uint32_t*)(hdr + 14) = 40;
    *(int32_t*)(hdr + 18)  = w;
    *(int32_t*)(hdr + 22)  = h;
    *(uint16_t*)(hdr + 26) = 1;
    *(uint16_t*)(hdr + 28) = 24;
    *(uint32_t*)(hdr + 34) = imgsz;
    fwrite(hdr, 1, 54, f);

    uint8_t* row = (uint8_t*)calloc(rowsz, 1);
    for (int y = h - 1; y >= 0; y--) {  // BMP is bottom-up
        const uint8_t* src = px + y * stride;
        for (int x = 0; x < w; x++) {
            row[x * 3 + 0] = src[x * 4 + 0];  // B
            row[x * 3 + 1] = src[x * 4 + 1];  // G
            row[x * 3 + 2] = src[x * 4 + 2];  // R
        }
        fwrite(row, 1, rowsz, f);
    }
    free(row);
    fclose(f);
}

int main(int, char**)
{
    lv_init();
    lv_tick_set_cb(tick_cb);
    lv_display_t* disp = lv_display_create(RES, RES);
    lv_display_set_buffers(disp, s_buf, NULL, sizeof(s_buf), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(disp, flush_cb);

    lv_timer_handler();     // warm the draw units (first render is slow)
    user_app();
    s_tick_offset += 3000;  // jump past entrance animations
    lv_refr_now(disp);      // render the settled UI once

    lv_draw_buf_t* snap = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_ARGB8888);
    if (snap) {
        write_bmp("/tmp/frij_snapshot.bmp", (const uint8_t*)snap->data, snap->header.w,
                  snap->header.h, snap->header.stride);
        lv_draw_buf_destroy(snap);
        printf("wrote /tmp/frij_snapshot.bmp\n");
    } else {
        printf("snapshot failed\n");
    }
    return 0;
}

#endif  // FRIJ_SNAPSHOT
