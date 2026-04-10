#ifndef __SYSTEM_TILE_H__
#define __SYSTEM_TILE_H__

#include "../lvgl_ui.h"
#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif


void system_tile_init(lv_obj_t *parent);
void system_set_estop(bool estop_active);
void system_set_confirm(bool pressed);
void system_set_joystick_vx(int vx);
void system_set_joystick_vy(int vy);
void system_set_joystick_vx_pct(int Vx_pct);
void system_set_joystick_vy_pct(int Vy_pct);


extern lv_obj_t *label_sensor_position;
extern lv_obj_t *label_joystick_position_A;
extern lv_obj_t *label_output_joystick_A;
extern lv_obj_t *label_joystick_position_B;
extern lv_obj_t *label_output_joystick_B;
extern lv_obj_t *label_estop;
extern lv_obj_t *label_confirm;
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