/**
 * @file screen_state_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_state_gen.h"
#include "../../ada_ui.h"
#include "state_label_gen.h"       

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

static lv_anim_timeline_t * timeline_blink_create(lv_obj_t * obj);
static void free_timeline_event_cb(lv_event_t * e);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * screen_state_create(void)
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
    lv_obj_set_name_static(lv_obj_0, "screen_state_#");

    lv_obj_add_style(lv_obj_0, &screen_bg, 0);
    lv_obj_t * left_eye = eye_create(lv_obj_0);
    lv_obj_set_name(left_eye, "left_eye");
    lv_obj_set_x(left_eye, 109);
    lv_obj_set_y(left_eye, 100);
    
    lv_obj_t * right_eye = eye_create(lv_obj_0);
    lv_obj_set_name(right_eye, "right_eye");
    lv_obj_set_x(right_eye, 234);
    lv_obj_set_y(right_eye, 100);
    
    lv_obj_t * status_text = state_label_create(lv_obj_0);
    lv_obj_set_name(status_text, "status_text");
    lv_obj_set_x(status_text, -1);
    lv_obj_set_y(status_text, 287);
    
    
    /* create animation timeline(s) */
    lv_anim_timeline_t ** at_array = lv_malloc(sizeof(lv_anim_timeline_t *) * _SCREEN_STATE_TIMELINE_CNT);
    at_array[SCREEN_STATE_TIMELINE_BLINK] = timeline_blink_create(lv_obj_0);
    lv_obj_set_user_data(lv_obj_0, at_array);
    lv_obj_add_event_cb(lv_obj_0, free_timeline_event_cb, LV_EVENT_DELETE, at_array);

    lv_obj_add_play_timeline_event(lv_obj_0, LV_EVENT_SCREEN_LOADED, screen_state_get_timeline(lv_obj_0, SCREEN_STATE_TIMELINE_BLINK), 0, false);

    LV_TRACE_OBJ_CREATE("finished");

    return lv_obj_0;
}

lv_anim_timeline_t * screen_state_get_timeline(lv_obj_t * obj, screen_state_timeline_t timeline_id)
{
    if (timeline_id >= _SCREEN_STATE_TIMELINE_CNT) {
        LV_LOG_WARN("screen_state has no timeline with %d ID", timeline_id);
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

static lv_anim_timeline_t * timeline_blink_create(lv_obj_t * obj)
{
    lv_anim_timeline_t * at = lv_anim_timeline_create();
    lv_anim_timeline_t * at_to_merge = NULL;

    lv_anim_t a;
    uint32_t selector_and_prop;

    selector_and_prop = ((LV_STYLE_HEIGHT & 0xff) << 24) | 0;
    lv_anim_init(&a);
    lv_anim_set_custom_exec_cb(&a, int_anim_exec_cb);
    lv_anim_set_var(&a, lv_obj_find_by_name(obj, "left_eye"));
    lv_anim_set_values(&a, 100, 6);
    lv_anim_set_duration(&a, 100);
    lv_anim_set_user_data(&a, (void *)((uintptr_t)selector_and_prop));
    lv_anim_timeline_add(at, 0, &a);

    selector_and_prop = ((LV_STYLE_HEIGHT & 0xff) << 24) | 0;
    lv_anim_init(&a);
    lv_anim_set_custom_exec_cb(&a, int_anim_exec_cb);
    lv_anim_set_var(&a, lv_obj_find_by_name(obj, "left_eye"));
    lv_anim_set_values(&a, 6, 100);
    lv_anim_set_duration(&a, 100);
    lv_anim_set_user_data(&a, (void *)((uintptr_t)selector_and_prop));
    lv_anim_timeline_add(at, 100, &a);

    selector_and_prop = ((LV_STYLE_HEIGHT & 0xff) << 24) | 0;
    lv_anim_init(&a);
    lv_anim_set_custom_exec_cb(&a, int_anim_exec_cb);
    lv_anim_set_var(&a, lv_obj_find_by_name(obj, "right_eye"));
    lv_anim_set_values(&a, 100, 6);
    lv_anim_set_duration(&a, 100);
    lv_anim_set_user_data(&a, (void *)((uintptr_t)selector_and_prop));
    lv_anim_timeline_add(at, 0, &a);

    selector_and_prop = ((LV_STYLE_HEIGHT & 0xff) << 24) | 0;
    lv_anim_init(&a);
    lv_anim_set_custom_exec_cb(&a, int_anim_exec_cb);
    lv_anim_set_var(&a, lv_obj_find_by_name(obj, "right_eye"));
    lv_anim_set_values(&a, 6, 100);
    lv_anim_set_duration(&a, 100);
    lv_anim_set_user_data(&a, (void *)((uintptr_t)selector_and_prop));
    lv_anim_timeline_add(at, 100, &a);

    return at;
}

static void free_timeline_event_cb(lv_event_t * e)
{
    lv_anim_timeline_t ** at_array = lv_event_get_user_data(e);
    uint32_t i;
    for(i = 0; i < _SCREEN_STATE_TIMELINE_CNT; i++) {
        lv_anim_timeline_delete(at_array[i]);
    }
    lv_free(at_array);
}

