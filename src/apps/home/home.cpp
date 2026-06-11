#include "home.h"

#include <time.h>

#include "core/datetime.h"
#include "system/battery.h"
#include "ui/anim.h"
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
    lv_obj_t* arc;      // seconds ring (outer)
    lv_obj_t* arc_min;  // minutes ring (inner, dimmer)
    lv_obj_t* time;
    lv_obj_t* date;
} clock_ctx_t;

// Battery glyph for a charge level (or the charging bolt while plugged in).
static const char* battery_glyph(uint8_t pct, bool charging)
{
    if (charging) {
        return LV_SYMBOL_CHARGE;
    }
    if (pct >= 90) {
        return LV_SYMBOL_BATTERY_FULL;
    }
    if (pct >= 65) {
        return LV_SYMBOL_BATTERY_3;
    }
    if (pct >= 40) {
        return LV_SYMBOL_BATTERY_2;
    }
    if (pct >= 15) {
        return LV_SYMBOL_BATTERY_1;
    }
    return LV_SYMBOL_BATTERY_EMPTY;
}

static void render(clock_ctx_t* c)
{
    bool      h24 = frij_clock_is_24h();  // re-read each tick so the setting reflects live
    time_t    now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);

    char t[8];
    strftime(t, sizeof(t), h24 ? "%H:%M" : "%I:%M", &tmv);
    const char* tp = (!h24 && t[0] == '0') ? t + 1 : t;  // drop 12h leading zero
    if (c->time) {
        lv_label_set_text(c->time, tp);
    }

    // date: "Wed 8 Jun" (+ AM/PM in 12h), day with no leading zero
    char wd[8];
    char mo[8];
    strftime(wd, sizeof(wd), "%a", &tmv);
    strftime(mo, sizeof(mo), "%b", &tmv);
    char d[48];
    if (h24) {
        lv_snprintf(d, sizeof(d), "%s %d %s", wd, tmv.tm_mday, mo);
    } else {
        char ap[4];
        strftime(ap, sizeof(ap), "%p", &tmv);
        lv_snprintf(d, sizeof(d), "%s %d %s  %s", wd, tmv.tm_mday, mo, ap);
    }
    if (c->date) {
        lv_label_set_text(c->date, d);
    }

    if (c->arc) {
        // glide the seconds hand between ticks (skip the 59->0 wrap + reduce-motion)
        int target = tmv.tm_sec;
        int cur    = lv_arc_get_value(c->arc);
        if (frij_anim_enabled() && target > cur) {
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, c->arc);
            lv_anim_set_exec_cb(&a, frij_anim_exec_arc);
            lv_anim_set_values(&a, cur, target);
            lv_anim_set_duration(&a, 950);  // finishes before the next 1s tick
            lv_anim_set_path_cb(&a, lv_anim_path_linear);
            lv_anim_start(&a);
        } else {
            lv_arc_set_value(c->arc, target);
        }
    }
    if (c->arc_min) {
        lv_arc_set_value(c->arc_min, tmv.tm_min);  // minutes sweep once an hour
    }
    // battery is observer-driven (see battery_observer_cb) — not polled here
}

// Observer: refresh the battery readout whenever the level/charging subjects
// change. Bound to both subjects, so either one firing updates the label.
static void battery_observer_cb(lv_observer_t* obs, lv_subject_t* subject)
{
    (void)subject;
    lv_obj_t* lbl      = (lv_obj_t*)lv_observer_get_target(obs);
    uint8_t   pct      = frij_battery_pct();
    bool      charging = frij_battery_charging();
    lv_label_set_text_fmt(lbl, "%s %d%%", battery_glyph(pct, charging), pct);
    // warn (amber) when low and unplugged; muted otherwise
    lv_obj_set_style_text_color(
        lbl, lv_color_hex((pct <= 15 && !charging) ? FRIJ_WARNING : FRIJ_TEXT_2), LV_PART_MAIN);
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
    if (c == NULL) {
        return;  // out of memory — skip the face rather than deref NULL
    }

    // A thin seconds ring fills most of the face (scales with the screen)...
    int ring = frij_screen_min() * 80 / 100;
    c->arc   = frij_progress_ring(col, ring, 0, ACCENT);
    lv_arc_set_range(c->arc, 0, 60);
    lv_obj_set_style_arc_width(c->arc, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_width(c->arc, 5, LV_PART_INDICATOR);

    // ...a dimmer inner ring tracks the minutes...
    c->arc_min = lv_arc_create(c->arc);
    lv_obj_set_size(c->arc_min, ring * 80 / 100, ring * 80 / 100);
    lv_obj_center(c->arc_min);
    lv_arc_set_rotation(c->arc_min, 270);
    lv_arc_set_bg_angles(c->arc_min, 0, 360);
    lv_arc_set_range(c->arc_min, 0, 60);
    lv_obj_remove_style(c->arc_min, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(c->arc_min, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_opa(c->arc_min, LV_OPA_TRANSP, LV_PART_MAIN);  // no track
    lv_obj_set_style_arc_width(c->arc_min, 3, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(c->arc_min, lv_color_hex(ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(c->arc_min, LV_OPA_50, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(c->arc_min, true, LV_PART_INDICATOR);

    // ...with the big time + date stacked dead-center inside it.
    lv_obj_t* inner = frij_col(c->arc, 2);
    lv_obj_set_width(inner, LV_SIZE_CONTENT);
    lv_obj_center(inner);
    c->time = frij_label(inner, "--:--", FRIJ_FONT_CLOCK, FRIJ_TEXT);
    c->date = frij_label(inner, "", FRIJ_FONT_BODY, FRIJ_TEXT_2);

    // Small battery readout under the date — bound to the battery subjects, so it
    // updates live (and everywhere) via the observer, not a per-tick poll.
    lv_obj_t* bat = lv_label_create(inner);
    lv_label_set_text(bat, "");  // never show LVGL's default "Text"
    lv_obj_set_style_text_font(bat, FRIJ_FONT_SMALL, LV_PART_MAIN);
    lv_subject_add_observer_obj(frij_battery_level_subject(), battery_observer_cb, bat, NULL);
    lv_subject_add_observer_obj(frij_battery_charging_subject(), battery_observer_cb, bat, NULL);

    render(c);

    lv_timer_t* timer = lv_timer_create(tick, 1000, c);
    lv_obj_add_event_cb(col, on_delete, LV_EVENT_DELETE, timer);
}

static void glance(lv_obj_t* parent)
{
    build_clock(parent);
}

const frij_app_t* home_app(void)
{
    // glance-only (watch face): no screens, so it can't be opened by swiping up.
    static const frij_app_t app = {"Home", ACCENT, glance, 0, NULL};
    return &app;
}
