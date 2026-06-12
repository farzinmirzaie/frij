#include "assistant.h"

#include <string.h>

#include "system/ai.h"
#include "system/haptics.h"
#include "ui/anim.h"
#include "ui/components.h"
#include "ui/theme.h"

/*
 * Frij AI — the voice assistant. With the cloud configured (system/ai ->
 * the Supabase "ask" edge function -> Gemini with store tools), releasing the
 * button sends a REAL question and renders the real answer. Until the device
 * has a mic, the emulator picks one of the sample questions as the "voice"
 * input. With no cloud (device today, or no .env) it falls back to canned
 * answers so the interaction still demos.
 *
 *   glance   : branding + how to invoke
 *   screen 0 : the last 5 questions + answers (RAM only — resets on reboot)
 *   overlay  : push-to-talk — listening (while held), thinking, answer
 */

static const uint32_t ACCENT = FRIJ_INFO;  // violet — the AI color

// Sample questions: the emulator's stand-in for the mic (one is picked at
// random and actually sent to the cloud); the answers are the offline mock.
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

// The last few asked questions, newest first (RAM only).
#define HIST_MAX 5
#define Q_LEN    160
#define A_LEN    600
static char s_hist_q[HIST_MAX][Q_LEN];
static char s_hist_a[HIST_MAX][A_LEN];
static int  s_hist_n = 0;

static void hist_push(const char* q, const char* a)
{
    for (int i = HIST_MAX - 1; i > 0; i--) {
        strcpy(s_hist_q[i], s_hist_q[i - 1]);
        strcpy(s_hist_a[i], s_hist_a[i - 1]);
    }
    lv_snprintf(s_hist_q[0], Q_LEN, "%s", q);
    lv_snprintf(s_hist_a[0], A_LEN, "%s", a);
    if (s_hist_n < HIST_MAX) {
        s_hist_n++;
    }
}

// The in-flight session's question + answer (cloud or mock fills them).
static char s_cur_q[Q_LEN];
static char s_cur_a[A_LEN];

// ---- overlay state machine ---------------------------------------------------

typedef enum { AI_IDLE, AI_LISTENING, AI_THINKING, AI_ANSWER } ai_state_t;

static ai_state_t  s_state   = AI_IDLE;
static lv_obj_t*   s_overlay = NULL;
static lv_timer_t* s_timer   = NULL;  // thinking: cloud poll, or the mock delay
static int         s_qa      = 0;     // which sample question this session uses
static bool        s_cloud   = false; // this session asks the real backend
static bool        s_error   = false; // the answer is an error message
static bool        s_voice   = false; // device mic capture is in flight

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

static void show_answer(void);

// Thinking tick: cloud mode polls the worker; mock mode is a fixed delay.
static void think_tick(lv_timer_t* t)
{
    (void)t;
    if (!s_overlay || s_state != AI_THINKING) {
        return;
    }
    if (s_cloud) {
        frij_ai_state_t st = frij_ai_state();
        if (st != FRIJ_AI_DONE && st != FRIJ_AI_ERROR) {
            return;  // still working
        }
        s_error = (st == FRIJ_AI_ERROR);
        frij_ai_take(s_cur_q, sizeof(s_cur_q), s_cur_a, sizeof(s_cur_a));
    }
    lv_timer_delete(s_timer);
    s_timer = NULL;
    show_answer();
}

static void build_thinking(void)
{
    lv_obj_clean(s_overlay);
    overlay_glow();
    lv_obj_t* col = overlay_col();

    // same rippling rings as Listening, but with loading dots at the heart
    // (not the voice bars) — "heard you, working on it"
    lv_obj_t* ring = frij_pulse_ring(col, 150, ACCENT);
    lv_obj_t* core = lv_obj_create(ring);
    lv_obj_remove_style_all(core);
    lv_obj_set_size(core, 84, 84);
    lv_obj_center(core);
    lv_obj_set_style_radius(core, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(core, lv_color_hex(ACCENT), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(core, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_t* dots = frij_loading_dots(core, 12, 0xFFFFFF);
    lv_obj_center(dots);

    lv_obj_t* title = frij_label(col, "Thinking", FRIJ_FONT_TITLE, FRIJ_TEXT);
    lv_obj_set_style_margin_top(title, FRIJ_SP_L, LV_PART_MAIN);
    if (s_cur_q[0]) {  // device voice: the question is unknown until it answers
        lv_obj_t* q = frij_label(col, s_cur_q, FRIJ_FONT_SMALL, FRIJ_TEXT_3);
        lv_obj_set_width(q, LV_PCT(90));
        lv_label_set_long_mode(q, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(q, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }

    s_state = AI_THINKING;
    // The ask was already kicked in frij_assistant_ptt (audio on device, text
    // on the emulator); cloud asks poll fast, the mock pretends for ~1.5s.
    s_timer = lv_timer_create(think_tick, s_cloud ? 200 : 1500, NULL);
}

static void on_answer_done(lv_event_t* e)
{
    (void)e;
    frij_modal_close_top();  // the overlay is the registered modal
}

// Error: dismiss our overlay and show the shared full-screen prompt (the same
// one Reset/Erase use), single OK action. Errors aren't saved to Recent.
static void show_error(void)
{
    char msg[A_LEN];
    lv_snprintf(msg, sizeof(msg), "%s", s_cur_a);
    if (s_overlay) {
        lv_obj_delete(s_overlay);  // on_overlay_deleted resets state + timer
        s_overlay = NULL;
    }
    frij_prompt_screen(LV_SYMBOL_WARNING, FRIJ_WARNING, "Frij AI", msg, "OK", NULL, NULL);
}

static void show_answer(void)
{
    if (!s_overlay) {
        return;
    }
    if (s_error) {
        show_error();
        return;
    }
    s_state = AI_ANSWER;
    lv_obj_clean(s_overlay);
    overlay_glow();
    lv_obj_t* col = overlay_col();

    lv_obj_t* q = frij_label(col, s_cur_q, FRIJ_FONT_SMALL, FRIJ_TEXT_3);
    lv_obj_set_width(q, LV_PCT(90));
    lv_label_set_long_mode(q, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(q, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    lv_obj_t* a = frij_label(col, s_cur_a, FRIJ_FONT_BODY, FRIJ_TEXT);
    lv_obj_set_width(a, LV_PCT(100));
    lv_label_set_long_mode(a, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(a, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_margin_top(a, FRIJ_SP_S, LV_PART_MAIN);
    lv_obj_set_style_margin_bottom(a, FRIJ_SP_M, LV_PART_MAIN);

    frij_circle_button(col, 56, ACCENT, LV_SYMBOL_OK, FRIJ_FONT_SYMBOL, 0xFFFFFF,
                       on_answer_done);
    frij_anim_enter(col, 0);
    frij_haptic(FRIJ_HAPTIC_SUCCESS);
    hist_push(s_cur_q, s_cur_a);
}

// ---- push-to-talk (the input layer drives this) -------------------------------

void frij_assistant_ptt(bool pressed)
{
    if (pressed) {
        if (s_overlay) {
            return;  // already in a session — ignore re-presses
        }
        s_error = false;
        // Device: start real mic capture. Emulator: no-op (false) -> a random
        // sample question stands in for the recording on release.
        s_voice = frij_ai_listen_start();
        s_qa    = (int)lv_rand(0, QA_COUNT - 1);
        lv_snprintf(s_cur_q, sizeof(s_cur_q), "%s", QA[s_qa].q);
        lv_snprintf(s_cur_a, sizeof(s_cur_a), "%s", QA[s_qa].a);  // mock fallback
        s_overlay = make_overlay();
        s_state   = AI_LISTENING;
        build_listening();
        frij_haptic(FRIJ_HAPTIC_SELECT);
    } else {
        if (!s_overlay || s_state != AI_LISTENING) {
            return;  // releases only matter while listening
        }
        // Kick the ask now (build_thinking just shows the UI + polls):
        if (s_voice) {
            // device: send the recorded audio; the question is unknown until
            // the cloud transcribes it, so blank it for the Thinking screen.
            s_cloud    = frij_ai_listen_ask();
            s_cur_q[0] = '\0';
        } else {
            s_cloud = frij_ai_ask(s_cur_q);  // emulator: text ask (or mock)
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
        lv_obj_t* q = frij_label(texts, s_hist_q[i], FRIJ_FONT_BODY, FRIJ_TEXT);
        lv_obj_set_width(q, LV_PCT(100));
        lv_label_set_long_mode(q, LV_LABEL_LONG_WRAP);
        lv_obj_t* a = frij_label(texts, s_hist_a[i], FRIJ_FONT_SMALL, FRIJ_TEXT_2);
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

#if defined(FRIJ_SNAPSHOT)
// Render the error overlay for the snapshot harness (no real failure needed).
void frij_assistant_demo_error(void)
{
    lv_snprintf(s_cur_a, sizeof(s_cur_a), "%s",
                "Frij AI is busy right now. Try again in a moment.");
    show_error();  // opens the shared prompt screen directly
}

// Render the thinking overlay for the snapshot harness. s_cloud=true makes
// think_tick poll the (idle) cloud, so it never advances to an answer.
void frij_assistant_demo_thinking(void)
{
    s_cloud    = true;
    s_cur_q[0] = '\0';
    s_overlay  = make_overlay();
    build_thinking();
}
#endif

const frij_app_t* assistant_app(void)
{
    static const frij_app_t app = {"Frij AI", ACCENT, glance, 1, screen, NULL, NULL};
    return &app;
}
