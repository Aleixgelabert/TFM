#ifndef __SYSTEM_TILE_H__
#define __SYSTEM_TILE_H__

#include "../lvgl_ui.h"


#ifdef __cplusplus
extern "C" {
#endif

void system_tile_init(lv_obj_t *parent);

void system_set_joystick_vx(int vx);
void system_set_joystick_vy(int vy);


extern lv_obj_t *label_cylinder_position;
extern lv_obj_t *label_brightness;
extern lv_obj_t *label_time;
extern lv_obj_t *label_date;
extern lv_obj_t *label_cylinder_pressure;
extern lv_obj_t *label_cylinder_load;


#ifdef __cplusplus
}
#endif



#endif