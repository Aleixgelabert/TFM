idf_component_register(SRCS "leds_wifi.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES driver esp_wifi esp_netif nvs_flash lwip)

