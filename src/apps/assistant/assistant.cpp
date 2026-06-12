#include "assistant.h"

#include "system/haptics.h"
#include "ui/anim.h"
#include "ui/components.h"
#include "ui/theme.h"

/*
 * Frij AI — the voice assistant's UI shell. The full pipeline will be:
 * hold Key B -> record mic -> Supabase Edge Function (Whisper + LLM with
 * store tools) -> answer. For now the listening / thinking / answer flow is
 * real UI over a MOCK pipeline (canned Q&A, fixed delay) so the interaction
 * can be designed and felt before any cloud wiring.
 *
 *   glance   : branding + how to invoke
 *   screen 0 : the last 5 questions + answers (RAM only — resets on reboot)
 *   overlay  : push-to-talk — listening (while held), thinking, answer
 */

static const uint32_t ACCENT = FRIJ_INFO;  // violet — the AI color

// Canned Q&A until the cloud pipeline exists. PTT picks one at random
// (pretending it transcribed you); the preset screen asks a specific one.
typedef struct {
    const char* q;
    const char* a;
} qa_t;

static const qa_t QA[] = {
    {"What's happening this week?",
     "Dentist on Saturday at 12:00, and Gym class tomorrow evening. Hari Raya Haji is on Tuesday."},
    {"What's left on the list?",
     "Three things left: milk, water filters, and booking the car service."},
    {"What should we cook tonight?",
     "You have rice and frozen salmon - teriyaki salmon bowls take about 25 minutes."},
    {"How long to boil eggs?",
     "Soft yolk: 6 - 7 minutes. Hard-boiled: 10 minutes. Start the timer once the water boils."},
};
#define QA_COUNT ((int)(sizeof(QA) / sizeof(QA[0])))

// The last few asked questions, newest first (RAM only; entries point into QA
// until the real pipeline produces dynamic strings).
#define HIST_MAX 5
static const qa_t* s_hist[HIST_MAX];
static int         s_hist_n = 0;

static void hist_push(const qa_t* qa)
{
    for (int i = HIST_MAX - 1; i > 0; i--) {
        s_hist[i] = s_hist[i - 1];
    }
    s_hist[0] = qa;
    if (s_hist_n < HIST_MAX) {
        s_hist_n++;
    }
}

// ---- overlay state machine ---------------------------------------------------

typedef enum { AI_IDLE, AI_LISTENING, AI_THINKING, AI_ANSWER } ai_state_t;

static ai_state_t  s_state   = AI_IDLE;
static lv_obj_t*   s_overlay = NULL;
static lv_timer_t* s_timer   = NULL;  // the mock "thinking" delay
static int         s_qa      = 0;     // which canned pair this session uses

static void on_overlay_deleted(lv_event_t* e)
{
    (void)e;
    s_overlay = NULL;
    s_state   = AI_IDLE;
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
}

// Full-screen black overlay, registered with the modal system (Back closes it).
static lv_obj_t* make_overlay(void)
{
    lv_obj_t* o = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(o, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);  // swallow touches under it
    lv_obj_add_event_cb(o, on_overlay_deleted, LV_EVENT_DELETE, NULL);
    frij_modal_register(o);
    if (frij_anim_enabled()) {  // ease in from transparent
        lv_obj_set_style_opa(o, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, o);
        lv_anim_set_exec_cb(&a, frij_anim_exec_opa);
        lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_duration(&a, FRIJ_ANIM_MS);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_start(&a);
    }
    return o;
}

// The centered column every overlay state builds into.
static lv_obj_t* overlay_col(void)
{
    lv_obj_t* col = frij_col(s_overlay, FRIJ_SP_M);
    lv_obj_set_width(col, LV_PCT(86));
    lv_obj_center(col);
    return col;
}

// Soft ambient glow behind whichever state is showing.
static void overlay_glow(void)
{
    lv_obj_t* g = frij_glow(s_overlay, ACCENT);
    lv_obj_center(g);
}

static void build_listening(void)
{
    lv_obj_clean(s_overlay);
    overlay_glow();

    // a very subtle accent halo breathing at the screen's rim — "the room is
    // listening", without competing with the center visual
    lv_obj_t* edge = frij_edge_glow(s_overlay, ACCENT);
    if (frij_anim_enabled()) {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, edge);
        lv_anim_set_exec_cb(&a, frij_anim_exec_opa);
        lv_anim_set_values(&a, 90, 255);
        lv_anim_set_duration(&a, 1100);
        lv_anim_set_playback_duration(&a, 1100);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        lv_anim_start(&a);
    }

    lv_obj_t* col = overlay_col();

    // rippling rings with the audio glyph at their heart
    lv_obj_t* ring = frij_pulse_ring(col, 150, ACCENT);
    lv_obj_t* core = lv_obj_create(ring);
    lv_obj_remove_style_all(core);
    lv_obj_set_size(core, 84, 84);
    lv_obj_center(core);
    lv_obj_set_style_radius(core, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(core, lv_color_hex(ACCENT), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(core, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_t* bars = frij_sound_bars(core, 40, 0xFFFFFF);
    lv_obj_center(bars);

    lv_obj_t* title = frij_label(col, "Listening", FRIJ_FONT_TITLE, FRIJ_TEXT);
    lv_obj_set_style_margin_top(title, FRIJ_SP_L, LV_PART_MAIN);
    frij_label(col, "Release to ask", FRIJ_FONT_SMALL, FRIJ_TEXT_3);
}

static void show_answer(lv_timer_t* t);

static void build_thinking(void)
{
    lv_obj_clean(s_overlay);
    overlay_glow();
    lv_obj_t* col = overlay_col();

    // a slow-spinning open arc — "working on it"
    lv_obj_t* arc = lv_arc_create(col);
    lv_obj_set_size(arc, 96, 96);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 80);  // short sweep; the rotation anim spins it
    lv_arc_set_value(arc, 0);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_remove_style(arc, NULL, LV_PART_INDICATOR);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(arc, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(ACCENT), LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    if (frij_anim_enabled()) {
        lv_obj_set_style_transform_pivot_x(arc, lv_pct(50), LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_y(arc, lv_pct(50), LV_PART_MAIN);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, arc);
        lv_anim_set_exec_cb(&a, frij_anim_exec_rotation);
        lv_anim_set_values(&a, 0, 3600);
        lv_anim_set_duration(&a, 1100);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);
    }

    lv_obj_t* title = frij_label(col, "Thinking", FRIJ_FONT_TITLE, FRIJ_TEXT);
    lv_obj_set_style_margin_top(title, FRIJ_SP_L, LV_PART_MAIN);
    lv_obj_t* q = frij_label(col, QA[s_qa].q, FRIJ_FONT_SMALL, FRIJ_TEXT_3);
    lv_obj_set_width(q, LV_PCT(90));
    lv_label_set_long_mode(q, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(q, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    s_state = AI_THINKING;
    s_timer = lv_timer_create(show_answer, 1500, NULL);  // the mock "cloud"
    lv_timer_set_repeat_count(s_timer, 1);
}

static void on_answer_done(lv_event_t* e)
{
    (void)e;
    frij_modal_close_top();  // the overlay is the registered modal
}

static void show_answer(lv_timer_t* t)
{
    (void)t;
    s_timer = NULL;  // one-shot timer self-deletes after this returns
    if (!s_overlay) {
        return;
    }
    s_state = AI_ANSWER;
    lv_obj_clean(s_overlay);
    overlay_glow();
    lv_obj_t* col = overlay_col();

    lv_obj_t* q = frij_label(col, QA[s_qa].q, FRIJ_FONT_SMALL, FRIJ_TEXT_3);
    lv_obj_set_width(q, LV_PCT(90));
    lv_label_set_long_mode(q, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(q, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    lv_obj_t* a = frij_label(col, QA[s_qa].a, FRIJ_FONT_BODY, FRIJ_TEXT);
    lv_obj_set_width(a, LV_PCT(100));
    lv_label_set_long_mode(a, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(a, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_margin_top(a, FRIJ_SP_S, LV_PART_MAIN);
    lv_obj_set_style_margin_bottom(a, FRIJ_SP_M, LV_PART_MAIN);

    frij_circle_button(col, 56, ACCENT, LV_SYMBOL_OK, FRIJ_FONT_SYMBOL, 0xFFFFFF,
                       on_answer_done);
    frij_anim_enter(col, 0);
    frij_haptic(FRIJ_HAPTIC_SUCCESS);
    hist_push(&QA[s_qa]);
}

// ---- push-to-talk (the input layer drives this) -------------------------------

void frij_assistant_ptt(bool pressed)
{
    if (pressed) {
        if (s_overlay) {
            return;  // already in a session — ignore re-presses
        }
        s_qa      = (int)lv_rand(0, QA_COUNT - 1);  // "transcription" preview
        s_overlay = make_overlay();
        s_state   = AI_LISTENING;
        build_listening();
        frij_haptic(FRIJ_HAPTIC_SELECT);
    } else {
        if (!s_overlay || s_state != AI_LISTENING) {
            return;  // releases only matter while listening
        }
        build_thinking();
    }
}

// ---- app contract --------------------------------------------------------------

static void glance(lv_obj_t* parent)
{
    lv_obj_t* col = frij_page(parent);

    lv_obj_t* ring = frij_pulse_ring(col, 110, ACCENT);
    lv_obj_t* core = lv_obj_create(ring);
    lv_obj_remove_style_all(core);
    lv_obj_set_size(core, 62, 62);
    lv_obj_center(core);
    lv_obj_set_style_radius(core, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(core, lv_color_hex(ACCENT), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(core, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_t* bars = frij_sound_bars(core, 28, 0xFFFFFF);
    lv_obj_center(bars);

    lv_obj_t* title = frij_label(col, "Frij AI", FRIJ_FONT_TITLE, FRIJ_TEXT);
    lv_obj_set_style_margin_top(title, FRIJ_SP_M, LV_PART_MAIN);
    frij_label(col, "Hold the blue button to ask", FRIJ_FONT_BODY, FRIJ_TEXT_2);
}

// Screen 1: the recent questions, newest first.
static void build_history(lv_obj_t* col)
{
    if (s_hist_n == 0) {
        frij_empty_state(col, "No questions yet", "Hold the blue button\nto ask something");
        return;
    }
    frij_section_label(col, "Recent");
    for (int i = 0; i < s_hist_n; i++) {
        lv_obj_t* row = frij_surface_row(col);
        lv_obj_set_height(row, LV_SIZE_CONTENT);  // Q + wrapped A need free height

        lv_obj_t* texts = frij_col(row, 4);
        lv_obj_set_flex_grow(texts, 1);
        lv_obj_set_style_flex_cross_place(texts, LV_FLEX_ALIGN_START, LV_PART_MAIN);
        lv_obj_t* q = frij_label(texts, s_hist[i]->q, FRIJ_FONT_BODY, FRIJ_TEXT);
        lv_obj_set_width(q, LV_PCT(100));
        lv_label_set_long_mode(q, LV_LABEL_LONG_WRAP);
        lv_obj_t* a = frij_label(texts, s_hist[i]->a, FRIJ_FONT_SMALL, FRIJ_TEXT_2);
        lv_obj_set_width(a, LV_PCT(100));
        lv_label_set_long_mode(a, LV_LABEL_LONG_WRAP);
    }
    frij_stagger_in(col, 40);
}

static void screen(lv_obj_t* parent, int index)
{
    (void)index;
    build_history(frij_page(parent));
}

const frij_app_t* assistant_app(void)
{
    static const frij_app_t app = {"Frij AI", ACCENT, glance, 1, screen, NULL, NULL};
    return &app;
}
