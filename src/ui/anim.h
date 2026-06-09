#ifndef FRIJ_ANIM_H
#define FRIJ_ANIM_H

#include "lvgl.h"

/*
 * Shared motion: entrance helpers + common lv_anim exec callbacks. Keeping the
 * animation vocabulary in one place means widgets animate consistently (same
 * properties, same easing) instead of each re-deriving it.
 */

// Reduce-motion switch. When disabled, frij_anim_enter / frij_stagger_in skip
// the motion and snap straight to the final state (the user's "Animations"
// setting). Defaults to enabled. (Does not affect direct lv_anim_start calls.)
void frij_anim_set_enabled(bool on);
bool frij_anim_enabled(void);

// lv_anim exec callbacks for common properties (pass to lv_anim_set_exec_cb).
void frij_anim_exec_opa(void* obj, int32_t v);          // whole-object opacity
void frij_anim_exec_bg_opa(void* obj, int32_t v);       // background opacity
void frij_anim_exec_translate_y(void* obj, int32_t v);  // vertical translate
void frij_anim_exec_scale(void* obj, int32_t v);        // transform scale (x+y)

// Entrance animation: fade in + rise. `delay_ms` lets callers stagger a list.
void frij_anim_enter(lv_obj_t* obj, uint32_t delay_ms);

// Run frij_anim_enter on every direct child, each delayed by `step_ms` more than
// the last — a staggered list entrance. Call after building the rows.
void frij_stagger_in(lv_obj_t* container, int step_ms);

#endif  // FRIJ_ANIM_H
