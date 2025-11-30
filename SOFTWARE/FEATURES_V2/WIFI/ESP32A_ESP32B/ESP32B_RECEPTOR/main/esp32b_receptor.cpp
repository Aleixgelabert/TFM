// esp32b_receptor.cpp
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "driver/gpio.h"
#include <cstring>

#define WIFI_SSID "joystick_esp"
#define WIFI_PASS "12345678"
#define PORT 3333
#define LED_R GPIO_NUM_25
#define LED_G GPIO_NUM_26
#define LED_B GPIO_NUM_27

static void wifi_init_sta() {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char*)wifi_config.sta.password, WIFI_PASS);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
}

extern "C" void app_main() {
    nvs_flash_init();
    wifi_init_sta();

    gpio_set_direction(LED_R, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_G, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_B, GPIO_MODE_OUTPUT);

    struct sockaddr_in addr;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    bind(sock, (struct sockaddr*)&addr, sizeof(addr));

    char rx_buffer[64];
    while (true) {
        int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len > 0) {
            rx_buffer[len] = 0;
            gpio_set_level(LED_R, 0);
            gpio_set_level(LED_G, 0);
            gpio_set_level(LED_B, 0);

            if (strcmp(rx_buffer, "UP") == 0) gpio_set_level(LED_R, 1);
            else if (strcmp(rx_buffer, "DOWN") == 0) gpio_set_level(LED_G, 1);
            else if (strcmp(rx_buffer, "LEFT") == 0) gpio_set_level(LED_B, 1);
            else if (strcmp(rx_buffer, "RIGHT") == 0) {
                gpio_set_level(LED_R, 1);
                gpio_set_level(LED_G, 1);
            }
        }
    }
}
