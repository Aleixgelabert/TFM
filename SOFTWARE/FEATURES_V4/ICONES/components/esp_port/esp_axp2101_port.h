#pragma once

#include <stdio.h>
#include "driver/i2c_master.h"

#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"

// extern XPowersPMU power;

esp_err_t esp_axp2101_port_init(i2c_master_bus_handle_t bus_handle);
void pmu_isr_handler(void);

// Afegit per poder tenir les iconesde Wi-Fi i Bateria
int pmu_get_battery_percentage();
bool pmu_is_charging();