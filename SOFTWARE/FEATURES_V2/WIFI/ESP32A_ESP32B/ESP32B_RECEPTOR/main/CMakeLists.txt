idf_component_register(SRCS "esp32b_receptor.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_wifi esp_netif nvs_flash lwip)