/**
 * @file screen_listening_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_listening_gen.h"
#include "../../ada_ui.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/***********************
 *  STATIC VARIABLES
 **********************/

/***********************
 *  STATIC PROTOTYPES
 **********************/

static lv_anim_timeline_t * timeline_pulse_create(lv_obj_t * obj);
static void free_timeline_event_cb(lv_event_t * e);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * screen_listening_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t screen_bg;

    static bool style_inited = false;

    if (!style_inited) {
        lv_style_init(&screen_bg);
        lv_style_set_width(&screen_bg, 412);
        lv_style_set_height(&screen_bg, 412);
        lv_style_set_bg_color(&screen_bg, BG_DARK);
        lv_style_set_bg_opa(&screen_bg, 255);

        style_inited = true;
    }

    lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
    lv_obj_set_name_static(lv_obj_0, "screen_listening_#");

    lv_obj_add_style(lv_obj_0, &screen_bg, 0);
    lv_obj_t * ear_body = ear_body_create(lv_obj_0);
    lv_obj_set_name(ear_body, "ear_body");
    lv_obj_set_x(ear_body, 50);
    lv_obj_set_y(ear_body, 81);
    lv_obj_set_width(ear_body, 139);
    lv_obj_set_height(ear_body, 220);
    
    lv_obj_t * ear_cut = ear_cut_create(lv_obj_0);
    lv_obj_set_name(ear_cut, "ear_cut");
    lv_obj_set_x(ear_cut, 52);
    lv_obj_set_y(ear_cut, 94);
    lv_obj_set_width(ear_cut, 129);
    lv_obj_set_height(ear_cut, 196);
    
    lv_obj_t * ear_ridge = ear_ridge_create(lv_obj_0);
    lv_obj_set_name(ear_ridge, "ear_ridge");
    lv_obj_set_x(ear_ridge, 238);
    lv_obj_set_y(ear_ridge, 146);
    
    lv_obj_t * ear_dot = ear_dot_create(lv_obj_0);
    lv_obj_set_name(ear_dot, "ear_dot");
    lv_obj_set_x(ear_dot, 320);
    lv_obj_set_y(ear_dot, 171);
    lv_obj_set_width(ear_dot, 17);
    lv_obj_set_height(ear_dot, 22);
    
    /*
    lv_obj_t * status_text = listening_label_create(lv_obj_0);
    lv_obj_set_name(status_text, "status_text");
    //lv_obj_set_x(status_text, -1);
    lv_obj_set_y(status_text, 335);
    */
    
    /* create animation timeline(s) */
    lv_anim_timeline_t ** at_array = lv_malloc(sizeof(lv_anim_timeline_t *) * _SCREEN_LISTENING_TIMELINE_CNT);
    at_array[SCREEN_LISTENING_TIMELINE_PULSE] = timeline_pulse_create(lv_obj_0);
    lv_obj_set_user_data(lv_obj_0, at_array);
    lv_obj_add_event_cb(lv_obj_0, free_timeline_event_cb, LV_EVENT_DELETE, at_array);

    lv_obj_add_play_timeline_event(lv_obj_0, LV_EVENT_SCREEN_LOADED, screen_listening_get_timeline(lv_obj_0, SCREEN_LISTENING_TIMELINE_PULSE), 0, false);

    LV_TRACE_OBJ_CREATE("finished");

    return lv_obj_0;
}

lv_anim_timeline_t * screen_listening_get_timeline(lv_obj_t * obj, screen_listening_timeline_t timeline_id)
{
    if (timeline_id >= _SCREEN_LISTENING_TIMELINE_CNT) {
        LV_LOG_WARN("screen_listening has no timeline with %d ID", timeline_id);
        return NULL;
    }

    lv_anim_timeline_t ** at_array = lv_obj_get_user_data(obj);
    return at_array[timeline_id];
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/* Helper function to execute animations */
static void int_anim_exec_cb(lv_anim_t * a, int32_t v)
{
    uint32_t data = (lv_uintptr_t)lv_anim_get_user_data(a);
    lv_style_prop_t prop = data >> 24;
    lv_style_selector_t selector = data & 0x00ffffff;

    lv_style_value_t style_value;
    style_value.num = v;
    lv_obj_set_local_style_prop(a->var, prop, style_value, selector);
}

static lv_anim_timeline_t * timeline_pulse_create(lv_obj_t * obj)
{
    lv_anim_timeline_t * at = lv_anim_timeline_create();
    lv_anim_timeline_t * at_to_merge = NULL;

    lv_anim_t a;
    uint32_t selector_and_prop;

    selector_and_prop = ((LV_STYLE_HEIGHT & 0xff) << 24) | 0;
    lv_anim_init(&a);
    lv_anim_set_custom_exec_cb(&a, int_anim_exec_cb);
    lv_anim_set_var(&a, lv_obj_find_by_name(obj, "ear_body"));
    lv_anim_set_values(&a, 220, 260);
    lv_anim_set_duration(&a, 300);
    lv_anim_set_user_data(&a, (void *)((uintptr_t)selector_and_prop));
    lv_anim_timeline_add(at, 0, &a);

    selector_and_prop = ((LV_STYLE_HEIGHT & 0xff) << 24) | 0;
    lv_anim_init(&a);
    lv_anim_set_custom_exec_cb(&a, int_anim_exec_cb);
    lv_anim_set_var(&a, lv_obj_find_by_name(obj, "ear_body"));
    lv_anim_set_values(&a, 260, 220);
    lv_anim_set_duration(&a, 300);
    lv_anim_set_user_data(&a, (void *)((uintptr_t)selector_and_prop));
    lv_anim_timeline_add(at, 300, &a);

    selector_and_prop = ((LV_STYLE_HEIGHT & 0xff) << 24) | 0;
    lv_anim_init(&a);
    lv_anim_set_custom_exec_cb(&a, int_anim_exec_cb);
    lv_anim_set_var(&a, lv_obj_find_by_name(obj, "ear_cut"));
    lv_anim_set_values(&a, 196, 236);
    lv_anim_set_duration(&a, 300);
    lv_anim_set_user_data(&a, (void *)((uintptr_t)selector_and_prop));
    lv_anim_timeline_add(at, 0, &a);

    selector_and_prop = ((LV_STYLE_HEIGHT & 0xff) << 24) | 0;
    lv_anim_init(&a);
    lv_anim_set_custom_exec_cb(&a, int_anim_exec_cb);
    lv_anim_set_var(&a, lv_obj_find_by_name(obj, "ear_cut"));
    lv_anim_set_values(&a, 236, 196);
    lv_anim_set_duration(&a, 300);
    lv_anim_set_user_data(&a, (void *)((uintptr_t)selector_and_prop));
    lv_anim_timeline_add(at, 300, &a);

    selector_and_prop = ((LV_STYLE_HEIGHT & 0xff) << 24) | 0;
    lv_anim_init(&a);
    lv_anim_set_custom_exec_cb(&a, int_anim_exec_cb);
    lv_anim_set_var(&a, lv_obj_find_by_name(obj, "ear_ridge"));
    lv_anim_set_values(&a, 80, 130);
    lv_anim_set_duration(&a, 250);
    lv_anim_set_user_data(&a, (void *)((uintptr_t)selector_and_prop));
    lv_anim_timeline_add(at, 50, &a);

    selector_and_prop = ((LV_STYLE_HEIGHT & 0xff) << 24) | 0;
    lv_anim_init(&a);
    lv_anim_set_custom_exec_cb(&a, int_anim_exec_cb);
    lv_anim_set_var(&a, lv_obj_find_by_name(obj, "ear_ridge"));
    lv_anim_set_values(&a, 130, 80);
    lv_anim_set_duration(&a, 250);
    lv_anim_set_user_data(&a, (void *)((uintptr_t)selector_and_prop));
    lv_anim_timeline_add(at, 300, &a);

    selector_and_prop = ((LV_STYLE_OPA & 0xff) << 24) | 0;
    lv_anim_init(&a);
    lv_anim_set_custom_exec_cb(&a, int_anim_exec_cb);
    lv_anim_set_var(&a, lv_obj_find_by_name(obj, "ear_dot"));
    lv_anim_set_values(&a, 255, 30);
    lv_anim_set_duration(&a, 300);
    lv_anim_set_user_data(&a, (void *)((uintptr_t)selector_and_prop));
    lv_anim_timeline_add(at, 100, &a);

    selector_and_prop = ((LV_STYLE_OPA & 0xff) << 24) | 0;
    lv_anim_init(&a);
    lv_anim_set_custom_exec_cb(&a, int_anim_exec_cb);
    lv_anim_set_var(&a, lv_obj_find_by_name(obj, "ear_dot"));
    lv_anim_set_values(&a, 30, 255);
    lv_anim_set_duration(&a, 300);
    lv_anim_set_user_data(&a, (void *)((uintptr_t)selector_and_prop));
    lv_anim_timeline_add(at, 400, &a);

    return at;
}

static void free_timeline_event_cb(lv_event_t * e)
{
    lv_anim_timeline_t ** at_array = lv_event_get_user_data(e);
    uint32_t i;
    for(i = 0; i < _SCREEN_LISTENING_TIMELINE_CNT; i++) {
        lv_anim_timeline_delete(at_array[i]);
    }
    lv_free(at_array);
}

