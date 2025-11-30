idf_component_register(SRCS "servo.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES esp_timer driver esp_wifi esp_event nvs_flash)
