/**
 * @file screen_idle_gen.h
 */

#ifndef SCREEN_IDLE_H
#define SCREEN_IDLE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
    #include "src/core/lv_obj_class_private.h"
#else
    #include "lvgl/lvgl.h"
    #include "lvgl/src/core/lv_obj_class_private.h"
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

typedef enum {
    SCREEN_IDLE_TIMELINE_BLINK = 0,
    _SCREEN_IDLE_TIMELINE_CNT = 1
}screen_idle_timeline_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/



lv_obj_t * screen_idle_create(void);

/**
 * Get a timeline of a screen_idle
 * @param obj          pointer to a screen_idle component
 * @param timeline_id  ID of the the timeline
 * @return             pointer to the timeline or NULL if not found
 */
lv_anim_timeline_t * screen_idle_get_timeline(lv_obj_t * obj, screen_idle_timeline_t timeline_id);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*SCREEN_IDLE_H*/