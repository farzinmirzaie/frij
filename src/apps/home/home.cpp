#include "home.h"

#include <time.h>

#include "store/store.h"
#include "ui/components.h"
#include "ui/theme.h"

/*
 * Home — a watch face. Shows the current time + date, ticking every second.
 * Reads the "24-hour time" setting. Later: battery, weather, richer faces.
 *
 * Each built instance owns its label set + a 1s timer; the timer (and its
 * context) are freed when the page is rebuilt (LV_EVENT_DELETE), so glance and
 * the opened screen never share state.
 */

static const uint32_t ACCENT = FRIJ_PRIMARY;

typedef struct {
    lv_obj_t* arc;   // seconds ring
    lv_obj_t* time;
    lv_obj_t* date;
    bool      h24;
} clock_ctx_t;

static bool read_clock24(void)
{
    char b[8];
    return frij_store_load("clock24", b, sizeof(b)) ? (b[0] == '1') : true;
}

static void render(clock_ctx_t* c)
{
    time_t    now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);

    char t[8];
    strftime(t, sizeof(t), c->h24 ? "%H:%M" : "%I:%M", &tmv);
    const char* tp = (!c->h24 && t[0] == '0') ? t + 1 : t;  // drop 12h leading zero
    if (c->time) {
        lv_label_set_text(c->time, tp);
    }

    char d[40];
    strftime(d, sizeof(d), c->h24 ? "%a %d %b" : "%a %d %b  %p", &tmv);
    if (c->date) {
        lv_label_set_text(c->date, d);
    }

    if (c->arc) {
        lv_arc_set_value(c->arc, tmv.tm_sec);  // sweep once a minute
    }
}

static void tick(lv_timer_t* t)
{
    render((clock_ctx_t*)lv_timer_get_user_data(t));
}

static void on_delete(lv_event_t* e)
{
    lv_timer_t*  t = (lv_timer_t*)lv_event_get_user_data(e);
    clock_ctx_t* c = (clock_ctx_t*)lv_timer_get_user_data(t);
    lv_timer_delete(t);
    lv_free(c);
}

static void build_clock(lv_obj_t* parent)
{
    lv_obj_t*    col = frij_page(parent);
    clock_ctx_t* c   = (clock_ctx_t*)lv_malloc(sizeof(clock_ctx_t));
    c->h24           = read_clock24();

    // seconds ring with the time centered inside
    c->arc = frij_progress_ring(col, 150, 0, ACCENT);
    lv_arc_set_range(c->arc, 0, 60);
    c->time = frij_label(c->arc, "--:--", FRIJ_FONT_DISPLAY, FRIJ_TEXT);
    lv_obj_center(c->time);

    c->date = frij_label(col, "", FRIJ_FONT_BODY, FRIJ_TEXT_2);
    render(c);

    lv_timer_t* timer = lv_timer_create(tick, 1000, c);
    lv_obj_add_event_cb(col, on_delete, LV_EVENT_DELETE, timer);
}

static void glance(lv_obj_t* parent)
{
    build_clock(parent);
}

static void screen(lv_obj_t* parent, int index)
{
    (void)index;
    build_clock(parent);
}

const frij_app_t* home_app(void)
{
    static const frij_app_t app = {"Home", ACCENT, glance, 1, screen};
    return &app;
}
