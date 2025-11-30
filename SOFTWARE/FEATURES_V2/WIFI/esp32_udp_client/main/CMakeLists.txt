idf_component_register(SRCS "esp32_udp_client.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_netif nvs_flash lwip)
