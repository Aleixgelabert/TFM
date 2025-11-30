idf_component_register(SRCS "comandament_V4.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES lvgl lvgl_ui esp_io_expander_tca9554 esp_port wifi_manager
)
