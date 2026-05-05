idf_component_register(SRCS "model_simulacio.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES esp_timer driver esp_wifi esp_event nvs_flash)
