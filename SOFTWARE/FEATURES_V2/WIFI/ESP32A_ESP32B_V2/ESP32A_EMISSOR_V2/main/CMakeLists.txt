idf_component_register(SRCS "esp32a_emissor_v2.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_wifi esp_netif nvs_flash lwip)