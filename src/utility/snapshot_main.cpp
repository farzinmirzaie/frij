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
#include <cstring>
#include <unistd.h>

#include "lvgl.h"

#include "apps/counter/counter.h"
#include "apps/settings/settings.h"
#include "apps/stopwatch/stopwatch.h"
#include "apps/todo/todo.h"
#include "system/wifi.h"
#include "ui/components.h"
#include "ui/theme.h"

extern void user_app(void);

// Render one app screen, round-clipped like the launcher does (for verifying a
// specific screen via FRIJ_SNAP=todo|counter|settings).
static void build_app_screen(const frij_app_t* app, int index)
{
    lv_obj_t* s = lv_screen_active();
    lv_obj_set_style_bg_color(s, lv_color_hex(FRIJ_OUTSIDE), LV_PART_MAIN);
    lv_obj_t* root = lv_obj_create(s);
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(root, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(root, true, LV_PART_MAIN);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    frij_apply_bg(root);
    if (!app) {
        return;
    }
    // subtle accent wash behind the header (mirrors the launcher)
    lv_obj_t* g = frij_top_tint(root, app->color);
    lv_obj_move_background(g);

    // mirror the launcher: header on the layer, content in an area below it
    int       hz      = 466 * 24 / 100;
    lv_obj_t* content = lv_obj_create(root);
    lv_obj_set_pos(content, 0, hz);
    lv_obj_set_size(content, 466, 466 - hz);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(content, 0, LV_PART_MAIN);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    if (app->build_screen) {
        app->build_screen(content, index);
        frij_page_under_header(content, hz);  // mirrors app_screen_builder
        frij_page_settle(content);
    }

    lv_obj_t* hdr = frij_header(root, app->name, NULL);
    if (app->action_symbol) {
        frij_header_set_action(hdr, app->action_symbol(index));
    }
}

// Render an app's home glance (no header), round-clipped like the launcher.
static void build_glance_view(const frij_app_t* app)
{
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(FRIJ_OUTSIDE), LV_PART_MAIN);
    lv_obj_t* root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(root, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(root, true, LV_PART_MAIN);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    frij_apply_bg(root);
    if (app && app->build_glance) {
        frij_glow(root, app->color);
        app->build_glance(root);
        frij_page_settle(root);
    }
}

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

    lv_timer_handler();  // warm the draw units (first render is slow)

    const char* scr = getenv("FRIJ_SNAP");
    if (scr && strcmp(scr, "todo") == 0) {
        build_app_screen(todo_app(), 0);
    } else if (scr && strcmp(scr, "todo_progress") == 0) {
        build_app_screen(todo_app(), 1);
    } else if (scr && strcmp(scr, "todo_add") == 0) {
        build_app_screen(todo_app(), 2);
    } else if (scr && strcmp(scr, "todo_glance") == 0) {
        build_glance_view(todo_app());
    } else if (scr && strcmp(scr, "counter") == 0) {
        build_app_screen(counter_app(), 0);
    } else if (scr && strcmp(scr, "stopwatch") == 0) {
        build_app_screen(stopwatch_app(), 0);
    } else if (scr && strcmp(scr, "stopwatch_glance") == 0) {
        build_glance_view(stopwatch_app());
    } else if (scr && strcmp(scr, "settings") == 0) {
        build_app_screen(settings_app(), 0);
    } else if (scr && strcmp(scr, "network") == 0) {
        build_app_screen(settings_app(), 1);
    } else if (scr && strcmp(scr, "netoff") == 0) {
        frij_wifi_set_enabled(false);
        build_app_screen(settings_app(), 1);
    } else if (scr && strcmp(scr, "sheet") == 0) {
        build_app_screen(settings_app(), 1);
        static const char* opts[] = {"Connect", "Forget"};
        frij_action_sheet("Linksys-5G", opts, 2, FRIJ_PRIMARY, NULL, NULL);
    } else if (scr && strcmp(scr, "about") == 0) {
        build_app_screen(settings_app(), 2);
    } else if (scr && strcmp(scr, "toast") == 0) {
        build_app_screen(settings_app(), 2);
        frij_toast("Syncing...");
    } else if (scr && strcmp(scr, "confirm") == 0) {
        build_app_screen(settings_app(), 2);
        frij_confirm("Reset settings?", "Restore everything to defaults.", "Reset",
                     FRIJ_DANGER, NULL);
    } else {
        user_app();  // default: the launcher (home)
    }

    // settle past entrance anims; the toast is transient, so sample it mid-hold
    s_tick_offset += (scr && strcmp(scr, "toast") == 0) ? 600 : 3000;
    lv_refr_now(disp);  // render the settled UI once

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
