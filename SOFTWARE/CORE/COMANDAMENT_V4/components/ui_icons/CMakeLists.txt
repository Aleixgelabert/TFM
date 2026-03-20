idf_component_register(SRCS 
    "battery_0.c"
    "battery_25.c"
    "battery_50.c"
    "battery_75.c"
    "battery_100.c"
    "battery_charging.c"
    "wifi_off.c"
    "wifi_1.c"
    "wifi_2.c"
    "wifi_3.c"
    INCLUDE_DIRS "."  # manté aquest directori
        "${CMAKE_SOURCE_DIR}/managed_components/lvgl__lvgl"  # afegeix LVGL
)