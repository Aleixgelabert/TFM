// esp32a_emissor.cpp
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "driver/adc.h"
#include <string>
#include <cstring>

#define WIFI_SSID "joystick_esp"
#define WIFI_PASS "12345678"
#define PORT 3333
#define RX_PIN_X ADC1_CHANNEL_6  // GPIO34
#define RX_PIN_Y ADC1_CHANNEL_7  // GPIO35
#define BUTTON GPIO_NUM_4

#define DEADZONE 400 // Zona morta

static void wifi_init_ap() {
    esp_netif_init();
    esp_event_loop_create_default();

    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t ap_config = {};
    strcpy((char*)ap_config.ap.ssid, WIFI_SSID);
    strcpy((char*)ap_config.ap.password, WIFI_PASS);
    ap_config.ap.ssid_len = strlen(WIFI_SSID);
    ap_config.ap.channel = 1;
    ap_config.ap.max_connection = 2;
    ap_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    ap_config.ap.ssid_hidden = 0;

    if (strlen(WIFI_PASS) == 0) {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    printf("WiFi AP creat: SSID=%s, PASS=%s\n", WIFI_SSID, WIFI_PASS);
}


extern "C" void app_main() {
    nvs_flash_init();
    wifi_init_ap();

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr("192.168.4.2"); // IP del receptor
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);


    gpio_set_direction(BUTTON, GPIO_MODE_INPUT);
    gpio_pulldown_en(BUTTON);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(RX_PIN_X, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(RX_PIN_Y, ADC_ATTEN_DB_11);

    // Variables per controlar els canvis
    int last_x = -1;
    int last_y = -1;
    int last_button = -1;

    while (true) {
        int x = adc1_get_raw(RX_PIN_X);
        int y = adc1_get_raw(RX_PIN_Y);
        int button_state = gpio_get_level(BUTTON);

        // Mostra els valors només si han canviat
        if (x != last_x || y != last_y || button_state != last_button) {
            printf("Joystick X=%d Y=%d Button=%d\n", x, y, button_state);
            last_x = x;
            last_y = y;
            last_button = button_state;
        }

        std::string dir = "CENTER";
        if (button_state == 1) { // Botó premut
            int dx = x - 2048;
            int dy = y - 2048;

            if (abs(dx) > abs(dy)) {
                // Moviment horitzontal dominant
                if (dx > DEADZONE) {
                    dir = "RIGHT";
                } else if (dx < -DEADZONE) {
                    dir = "LEFT";
                } else {
                    dir = "CENTER";
                }
            } else {
                // Moviment vertical dominant
                if (dy > DEADZONE) {
                    dir = "UP";
                } else if (dy < -DEADZONE) {
                    dir = "DOWN";
                } else {
                    dir = "CENTER";
                }
            }
        } else {
            dir = "CENTER";
        }

        sendto(sock, dir.c_str(), dir.size(), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
