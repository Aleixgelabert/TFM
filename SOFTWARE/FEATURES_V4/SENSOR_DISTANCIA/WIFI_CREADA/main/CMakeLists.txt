idf_component_register(SRCS "sensor_distancia.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES esp_timer driver esp_wifi esp_event nvs_flash)
