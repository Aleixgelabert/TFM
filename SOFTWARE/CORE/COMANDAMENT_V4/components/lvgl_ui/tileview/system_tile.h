#ifndef __SYSTEM_TILE_H__
#define __SYSTEM_TILE_H__

#include "../lvgl_ui.h"
#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif


void system_tile_init(lv_obj_t *parent);

void system_set_joystick_vx(int vx);
void system_set_joystick_vy(int vy);

extern lv_obj_t *label_cylinder_position;
extern lv_obj_t *label_brightness;
//extern lv_obj_t *label_time;
//extern lv_obj_t *label_date;
//extern lv_obj_t *label_cylinder_pressure;
//extern lv_obj_t *label_cylinder_load;
extern lv_obj_t *label_joystick_position_A;
extern lv_obj_t *label_output_joystick_A;
extern lv_obj_t *label_joystick_position_B;
extern lv_obj_t *label_output_joystick_B;

extern lv_obj_t *icon_battery;
extern lv_obj_t *icon_wifi;
extern lv_obj_t *label_batt_pct;

int system_get_wifi_level();

void system_update_battery(int pct, bool charging);
void system_update_wifi(int level);


#ifdef __cplusplus
}
#endif



#endif