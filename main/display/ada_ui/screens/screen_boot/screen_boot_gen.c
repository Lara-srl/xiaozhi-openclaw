/**
 * @file screen_boot_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_boot_gen.h"
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

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * screen_boot_create(void)
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
    lv_obj_set_name_static(lv_obj_0, "screen_boot_#");

    lv_obj_add_style(lv_obj_0, &screen_bg, 0);
    lv_obj_t * title_label = boot_title_create(lv_obj_0);
    lv_obj_set_name(title_label, "title_label");
    lv_obj_set_x(title_label, 0);
    lv_obj_set_y(title_label, 90);
    
    lv_obj_t * status_label = boot_status_create(lv_obj_0);
    lv_obj_set_name(status_label, "status_label");
    lv_obj_set_x(status_label, 56);
    lv_obj_set_y(status_label, 200);

    LV_TRACE_OBJ_CREATE("finished");

    return lv_obj_0;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

