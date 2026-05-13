/**
 * @file ada_ui_gen.h
 */

#ifndef LV_USE_OBJ_NAME
#define LV_USE_OBJ_NAME 1
#endif

#ifndef ADA_UI_GEN_H
#define ADA_UI_GEN_H

#ifndef UI_SUBJECT_STRING_LENGTH
#define UI_SUBJECT_STRING_LENGTH 256
#endif

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

#define BG_DARK lv_color_hex(0x000000)

#define ADA_BLUE lv_color_hex(0x00AAFF)

#define ADA_EW 70

#define ADA_EH 100

#define ADA_ER 35

#define ADA_GAP 60

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL VARIABLES
 **********************/

/*-------------------
 * Permanent screens
 *------------------*/

/*----------------
 * Global styles
 *----------------*/

/*----------------
 * Fonts
 *----------------*/

/*----------------
 * Images
 *----------------*/

/*----------------
 * Subjects
 *----------------*/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/*----------------
 * Event Callbacks
 *----------------*/

/**
 * Initialize the component library
 */

void ada_ui_init_gen(const char * asset_path);

/**********************
 *      MACROS
 **********************/

/**********************
 *   POST INCLUDES
 **********************/

/*Include all the widgets, components and screens of this library*/
#include "components/eye/eye_gen.h"
#include "screens/screen_idle/screen_idle_gen.h"
#include "components/eye/eye_gen.h"
#include "components/boot_title/boot_title_gen.h"
#include "components/boot_status/boot_status_gen.h"
#include "screens/screen_idle/screen_idle_gen.h"
#include "screens/screen_boot/screen_boot_gen.h"
#include "components/ear_body/ear_body_gen.h"
#include "components/ear_cut/ear_cut_gen.h"
#include "components/ear_ridge/ear_ridge_gen.h"
#include "components/ear_dot/ear_dot_gen.h"
#include "components/boot_title/boot_title_gen.h"
#include "components/boot_status/boot_status_gen.h"
#include "components/listening_label/listening_label_gen.h"
#include "screens/screen_boot/screen_boot_gen.h"
#include "screens/screen_listening/screen_listening_gen.h"

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*ADA_UI_GEN_H*/