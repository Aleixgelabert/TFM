#include "esp_axp2101_port.h"

extern XPowersPMU power;

int pmu_get_battery_percentage()
{
    if (!power.isBatteryConnect())
        return -1;      // No hi ha bateria

    return power.getBatteryPercent();
}

bool pmu_is_charging()
{
    return power.isCharging();
}

bool pmu_is_vbus_in()
{
    return power.isVbusIn();
}
