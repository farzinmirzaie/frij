#include "carousel.h"

static void rebuild(frij_carousel_t* c)
{
    lv_obj_clean(c->page);          // remove old content, keep the page object
    c->builder(c->page, c->index, c->user);
}

void frij_carousel_init(frij_carousel_t* c, lv_obj_t* parent, int count,
                        frij_page_builder builder, void* user)
{
    c->count   = count < 1 ? 1 : count;
    c->index   = 0;
    c->builder = builder;
    c->user    = user;

    c->page = lv_obj_create(parent);
    lv_obj_set_size(c->page, LV_PCT(100), LV_PCT(100));
    lv_obj_center(c->page);
    lv_obj_set_style_bg_opa(c->page, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(c->page, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(c->page, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(c->page, 0, LV_PART_MAIN);
    lv_obj_clear_flag(c->page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(c->page, LV_OBJ_FLAG_EVENT_BUBBLE);  // let swipes reach the screen

    rebuild(c);
}

void frij_carousel_next(frij_carousel_t* c)
{
    c->index = (c->index + 1) % c->count;
    rebuild(c);
}

void frij_carousel_prev(frij_carousel_t* c)
{
    c->index = (c->index - 1 + c->count) % c->count;
    rebuild(c);
}

int frij_carousel_index(const frij_carousel_t* c)
{
    return c->index;
}
