#include "anim.h"

#include "theme.h"

void frij_anim_exec_opa(void* o, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t*)o, (lv_opa_t)v, LV_PART_MAIN);
}

void frij_anim_exec_bg_opa(void* o, int32_t v)
{
    lv_obj_set_style_bg_opa((lv_obj_t*)o, (lv_opa_t)v, LV_PART_MAIN);
}

void frij_anim_exec_translate_y(void* o, int32_t v)
{
    lv_obj_set_style_translate_y((lv_obj_t*)o, v, LV_PART_MAIN);
}

void frij_anim_exec_scale(void* o, int32_t v)
{
    lv_obj_set_style_transform_scale_x((lv_obj_t*)o, v, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_y((lv_obj_t*)o, v, LV_PART_MAIN);
}

void frij_anim_enter(lv_obj_t* obj, uint32_t delay_ms)
{
    lv_obj_set_style_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_translate_y(obj, 14, LV_PART_MAIN);

    lv_anim_t fade;
    lv_anim_init(&fade);
    lv_anim_set_var(&fade, obj);
    lv_anim_set_exec_cb(&fade, frij_anim_exec_opa);
    lv_anim_set_values(&fade, 0, LV_OPA_COVER);
    lv_anim_set_duration(&fade, FRIJ_ANIM_MS);
    lv_anim_set_delay(&fade, delay_ms);
    lv_anim_set_path_cb(&fade, lv_anim_path_ease_out);
    lv_anim_start(&fade);

    lv_anim_t rise;
    lv_anim_init(&rise);
    lv_anim_set_var(&rise, obj);
    lv_anim_set_exec_cb(&rise, frij_anim_exec_translate_y);
    lv_anim_set_values(&rise, 14, 0);
    lv_anim_set_duration(&rise, FRIJ_ANIM_MS + 40);
    lv_anim_set_delay(&rise, delay_ms);
    lv_anim_set_path_cb(&rise, lv_anim_path_ease_out);
    lv_anim_start(&rise);
}

void frij_stagger_in(lv_obj_t* container, int step_ms)
{
    uint32_t n = lv_obj_get_child_count(container);
    for (uint32_t i = 0; i < n; i++) {
        frij_anim_enter(lv_obj_get_child(container, i), i * step_ms);
    }
}
