/**
 * @file screen_listening_gen.h
 */

#ifndef SCREEN_LISTENING_H
#define SCREEN_LISTENING_H

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
    SCREEN_LISTENING_TIMELINE_PULSE = 0,
    _SCREEN_LISTENING_TIMELINE_CNT = 1
}screen_listening_timeline_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/



lv_obj_t * screen_listening_create(void);

/**
 * Get a timeline of a screen_listening
 * @param obj          pointer to a screen_listening component
 * @param timeline_id  ID of the the timeline
 * @return             pointer to the timeline or NULL if not found
 */
lv_anim_timeline_t * screen_listening_get_timeline(lv_obj_t * obj, screen_listening_timeline_t timeline_id);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*SCREEN_LISTENING_H*/