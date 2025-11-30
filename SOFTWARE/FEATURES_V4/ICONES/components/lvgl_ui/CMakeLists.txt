# Recull tots els fitxers .cpp i .c de manera recursiva
file(GLOB_RECURSE SRC_CPP_FILES "*.cpp")
file(GLOB_RECURSE SRC_C_FILES "*.c")

# Recull tots els fitxers de les icones
file(GLOB ICON_SOURCES "../ui_icons/*.c")

# Combina tots els fitxers font
set(SRC_FILES ${SRC_CPP_FILES} ${SRC_C_FILES} ${ICON_SOURCES})

# Registra el component amb ESP-IDF
idf_component_register(
    SRCS ${SRC_FILES}
    INCLUDE_DIRS "." "../ui_icons"
    REQUIRES lvgl XPowersLib sensorlib freertos spi_flash esp_psram driver esp_hw_support esp32-camera espressif__esp_lvgl_port esp_port pmu_battery
)
