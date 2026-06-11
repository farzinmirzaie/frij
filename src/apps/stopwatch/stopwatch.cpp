#include "stopwatch.h"

#include <stdint.h>

#include "system/haptics.h"
#include "ui/anim.h"
#include "ui/components.h"
#include "ui/theme.h"

/*
 * Stopwatch — on-brand for the StopWatch dev kit. Counts up in MM:SS.cs with
 * start/stop, reset, and laps.
 *
 *   glance   : current elapsed (live when running) + state
 *   screen 0 : big readout + Start/Stop + Lap/Reset, with a lap list
 *
 * Timing is kept in module-level state (not the UI), so the watch keeps running
 * while you navigate away; each built page just attaches its own refresh timer
 * and reads the shared elapsed(). State is intentionally NOT persisted — a
 * stopwatch resets when the firmware restarts.
 */

static const uint32_t ACCENT = FRIJ_SECONDARY;  // green
#define MAX_LAPS 20

static bool     s_running    = false;
static uint32_t s_acc_ms     = 0;  // elapsed accumulated across previous runs
static uint32_t s_start_tick = 0;  // lv_tick when the current run started
static uint32_t s_laps[MAX_LAPS];
static int      s_lap_n = 0;

static uint32_t elapsed_ms(void)
{
    return s_running ? s_acc_ms + lv_tick_elaps(s_start_tick) : s_acc_ms;
}

static void fmt_time(char* b, size_t n, uint32_t ms)
{
    uint32_t cs = (ms / 10) % 100;
    uint32_t s  = (ms / 1000) % 60;
    uint32_t m  = ms / 60000;
    lv_snprintf(b, n, "%02u:%02u.%02u", (unsigned)m, (unsigned)s, (unsigned)cs);
}

// ---- per-page UI (screen + glance share these via their own ctx) -----------

typedef struct {
    lv_obj_t*   time;
    lv_obj_t*   status;     // "Ready / Running / Paused" (screen only)
    lv_obj_t*   primary;    // Start / Stop
    lv_obj_t*   secondary;  // Lap / Reset
    lv_obj_t*   laps;       // lap list column (screen only; NULL on the glance)
    lv_timer_t* timer;
} sw_ctx_t;

static void set_button(lv_obj_t* btn, const char* glyph, uint32_t bg, uint32_t fg)
{
    if (!btn) {
        return;
    }
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg), LV_PART_MAIN);
    lv_obj_t* lbl = lv_obj_get_child(btn, 0);
    if (lbl) {
        lv_label_set_text(lbl, glyph);
        lv_obj_set_style_text_color(lbl, lv_color_hex(fg), LV_PART_MAIN);
    }
}

// Split for lap i: time since the previous lap (lap 0 == its own cumulative).
static uint32_t lap_split(int i)
{
    return i == 0 ? s_laps[0] : s_laps[i] - s_laps[i - 1];
}

static void populate_laps(lv_obj_t* col)
{
    lv_obj_clean(col);

    // find the fastest/slowest split to highlight (only with 2+ laps)
    int fast = -1, slow = -1;
    if (s_lap_n >= 2) {
        fast = slow = 0;
        for (int i = 1; i < s_lap_n; i++) {
            if (lap_split(i) < lap_split(fast)) fast = i;
            if (lap_split(i) > lap_split(slow)) slow = i;
        }
    }

    // newest lap on top; show "split   cumulative"
    for (int i = s_lap_n - 1; i >= 0; i--) {
        char num[12], sp[16], tot[16], val[36];
        lv_snprintf(num, sizeof(num), "Lap %d", i + 1);
        fmt_time(sp, sizeof(sp), lap_split(i));
        fmt_time(tot, sizeof(tot), s_laps[i]);
        lv_snprintf(val, sizeof(val), "%s   %s", sp, tot);
        lv_obj_t* row = frij_value_row(col, num, val);
        if (i == fast) {
            lv_obj_set_style_text_color(lv_obj_get_child(row, 1), lv_color_hex(FRIJ_SECONDARY),
                                        LV_PART_MAIN);
        } else if (i == slow) {
            lv_obj_set_style_text_color(lv_obj_get_child(row, 1), lv_color_hex(FRIJ_WARNING),
                                        LV_PART_MAIN);
        }
    }
}

static void refresh_ui(sw_ctx_t* c)
{
    if (c->time) {
        char b[16];
        fmt_time(b, sizeof(b), elapsed_ms());
        lv_label_set_text(c->time, b);
    }
    if (c->status) {
        lv_label_set_text(c->status, s_running ? "Running" : (s_acc_ms ? "Paused" : "Ready"));
    }
    // Start (green play) <-> Stop (red pause)
    if (s_running) {
        set_button(c->primary, LV_SYMBOL_PAUSE, FRIJ_DANGER, 0xFFFFFF);
        set_button(c->secondary, LV_SYMBOL_PLUS, FRIJ_SURFACE_2, ACCENT);  // Lap
    } else {
        set_button(c->primary, LV_SYMBOL_PLAY, ACCENT, 0x101216);
        set_button(c->secondary, LV_SYMBOL_REFRESH, FRIJ_SURFACE_2, FRIJ_TEXT_2);  // Reset
    }
}

static void tick(lv_timer_t* t)
{
    sw_ctx_t* c = (sw_ctx_t*)lv_timer_get_user_data(t);
    if (c->time) {
        char b[16];
        fmt_time(b, sizeof(b), elapsed_ms());
        lv_label_set_text(c->time, b);
    }
}

// The 33ms refresh only needs to run while the watch is running — pause it when
// stopped so an idle stopwatch page costs (nearly) nothing.
static void sync_timer(sw_ctx_t* c)
{
    if (!c->timer) {
        return;
    }
    if (s_running) {
        lv_timer_resume(c->timer);
    } else {
        lv_timer_pause(c->timer);
    }
}

static void on_delete(lv_event_t* e)
{
    sw_ctx_t* c = (sw_ctx_t*)lv_event_get_user_data(e);
    if (c->timer) {
        lv_timer_delete(c->timer);
    }
    lv_free(c);
}

// ---- actions ---------------------------------------------------------------

static void on_start_stop(lv_event_t* e)
{
    sw_ctx_t* c = (sw_ctx_t*)lv_event_get_user_data(e);
    if (s_running) {
        s_acc_ms += lv_tick_elaps(s_start_tick);  // bank the run
        s_running = false;
    } else {
        s_start_tick = lv_tick_get();
        s_running    = true;
    }
    frij_haptic(FRIJ_HAPTIC_SELECT);
    refresh_ui(c);
    sync_timer(c);
}

static void on_lap_reset(lv_event_t* e)
{
    sw_ctx_t* c = (sw_ctx_t*)lv_event_get_user_data(e);
    if (s_running) {
        if (s_lap_n < MAX_LAPS) {
            s_laps[s_lap_n++] = elapsed_ms();
            frij_haptic(FRIJ_HAPTIC_TAP);
            if (c->laps) {
                populate_laps(c->laps);
                if (lv_obj_get_child_count(c->laps) > 0) {
                    frij_anim_enter(lv_obj_get_child(c->laps, 0), 0);  // newest lap pops in
                }
            }
        }
    } else {
        s_acc_ms  = 0;  // reset
        s_lap_n   = 0;
        frij_haptic(FRIJ_HAPTIC_SUCCESS);
        if (c->laps) {
            populate_laps(c->laps);
        }
        refresh_ui(c);
    }
}

// ---- builders --------------------------------------------------------------

static void glance(lv_obj_t* parent)
{
    lv_obj_t* col = frij_page(parent);
    sw_ctx_t* c   = (sw_ctx_t*)lv_malloc(sizeof(sw_ctx_t));
    if (c == NULL) {
        return;
    }
    lv_memzero(c, sizeof(*c));

    frij_label(col, "Stopwatch", FRIJ_FONT_BODY, FRIJ_TEXT_2);
    c->time = frij_label(col, "00:00.00", FRIJ_FONT_DISPLAY, s_running ? ACCENT : FRIJ_TEXT);
    frij_label(col, s_running ? "Running" : (s_acc_ms ? "Paused" : "Ready"), FRIJ_FONT_BODY,
               FRIJ_TEXT_2);

    refresh_ui(c);  // sets the time text (buttons are NULL here — safely skipped)
    c->timer = lv_timer_create(tick, 33, c);
    sync_timer(c);  // paused while the watch isn't running
    lv_obj_add_event_cb(col, on_delete, LV_EVENT_DELETE, c);
}

static void build_buttons(sw_ctx_t* c, lv_obj_t* col)
{
    lv_obj_t* row = lv_obj_create(col);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, FRIJ_SP_XL, LV_PART_MAIN);

    // glyphs/colors are set by refresh_ui(); pass placeholders + NULL handlers
    // here (screen() attaches the real handlers with the ctx as user_data).
    c->secondary = frij_circle_button(row, 64, FRIJ_SURFACE_2, LV_SYMBOL_REFRESH,
                                      FRIJ_FONT_SYMBOL, FRIJ_TEXT_2, NULL);
    c->primary   = frij_circle_button(row, 80, ACCENT, LV_SYMBOL_PLAY,
                                      FRIJ_FONT_SYMBOL, 0x101216, NULL);
}

static void screen(lv_obj_t* parent, int index)
{
    (void)index;
    lv_obj_t* col = frij_page(parent);
    sw_ctx_t* c   = (sw_ctx_t*)lv_malloc(sizeof(sw_ctx_t));
    if (c == NULL) {
        return;
    }
    lv_memzero(c, sizeof(*c));

    c->time   = frij_label(col, "00:00.00", FRIJ_FONT_DISPLAY, FRIJ_TEXT);
    c->status = frij_label(col, "", FRIJ_FONT_SMALL, FRIJ_TEXT_2);
    build_buttons(c, col);

    c->laps = frij_col(col, FRIJ_SP_S);  // lap list under the buttons
    populate_laps(c->laps);

    // the circle buttons own their click handlers; give them this ctx
    lv_obj_add_event_cb(c->primary, on_start_stop, LV_EVENT_CLICKED, c);
    lv_obj_add_event_cb(c->secondary, on_lap_reset, LV_EVENT_CLICKED, c);

    refresh_ui(c);
    c->timer = lv_timer_create(tick, 33, c);
    sync_timer(c);  // paused while the watch isn't running
    lv_obj_add_event_cb(col, on_delete, LV_EVENT_DELETE, c);
}

const frij_app_t* stopwatch_app(void)
{
    static const frij_app_t app = {"Stopwatch", ACCENT, glance, 1, screen};
    return &app;
}
