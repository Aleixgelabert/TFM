// esp32a_emissor_v2.cpp
// 🔹 1. Headers bàsics del sistema C/C++
#include <stdio.h>
#include <string>
#include <cstring>

// 🔹 2. FreeRTOS (sempre abans d’altres dependències de tasks o drivers)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 🔹 3. ESP-IDF core (NVS, Wi-Fi, events, netif, log, errors)
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_err.h"

// 🔹 4. LwIP (sockets i xarxa baixa)
#include "lwip/sockets.h"
#include "lwip/netdb.h"

// 🔹 5. Drivers de maquinari
#include "driver/gpio.h"
#include "driver/adc.h"

// 🔹 6. Altres (si tens sensors, biblioteques pròpies, etc.)

// WIFI
#define WIFI_SSID "joystick_esp"
#define WIFI_PASS "12345678"
#define PORT 3333

// Joystick
#define RX_PIN_X ADC1_CHANNEL_4  // GPIO32
#define RX_PIN_Y ADC1_CHANNEL_7  // GPIO35

//Botó confirm
#define BUTTON GPIO_NUM_4

// LEDs funcionament joystick
#define LED_R GPIO_NUM_25
#define LED_G GPIO_NUM_26
#define LED_B GPIO_NUM_27

#define DEADZONE 400 // Zona morta

// LEDs qualitat senyal WIFI
#define LED1 GPIO_NUM_12
#define LED2 GPIO_NUM_13
#define LED3 GPIO_NUM_14
#define LED4 GPIO_NUM_15

// Identificació de les linies de log
static const char *TAG = "ESP32A_EMISSOR";


// 🔹 Configura els LEDs com a sortides
static void leds_init() {
    gpio_reset_pin(LED1);
    gpio_reset_pin(LED2);
    gpio_reset_pin(LED3);
    gpio_reset_pin(LED4);

    gpio_set_direction(LED1, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED2, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED3, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED4, GPIO_MODE_OUTPUT);
}

// 🔹 Engega els LEDs segons la potència del senyal
static void set_leds_by_signal(int rssi) {
    gpio_set_level(LED1, 0);
    gpio_set_level(LED2, 0);
    gpio_set_level(LED3, 0);
    gpio_set_level(LED4, 0);

    int leds = 0;
    if (rssi >= -30) leds = 4;       // excel·lent
    else if (rssi >= -50) leds = 3;  // bo
    else if (rssi >= -80) leds = 2;  // acceptable
    else leds = 1;                   // molt dèbil

    if (leds >= 1) gpio_set_level(LED1, 1);
    if (leds >= 2) gpio_set_level(LED2, 1);
    if (leds >= 3) gpio_set_level(LED3, 1);
    if (leds == 4) gpio_set_level(LED4, 1);
}

/* Simple Wi-Fi connect (blocking-ish). */

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

// 🔹 Tasca per llegir la potència WiFi i actualitzar LEDs
// Eliminat pq aquest ESP és només l'emissor (AP + servidor UDP)
/*void wifi_signal_task(void *pvParameters)
{
    while (true) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            int rssi = ap_info.rssi;
            ESP_LOGI("WIFI_SIGNAL", "RSSI: %d dBm", rssi);
            set_leds_by_signal(rssi);
        } else {
            ESP_LOGW("WIFI_SIGNAL", "No connectat a cap AP");
            // Apaguem tots els LEDs si no hi ha connexió
            gpio_set_level(LED1, 0);
            gpio_set_level(LED2, 0);
            gpio_set_level(LED3, 0);
            gpio_set_level(LED4, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); // cada 2 segons
    }
}*/

void udp_server_task(void *pvParameters)
{
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t socklen = sizeof(client_addr);
    char rx_buffer[128];

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Socket bind failed: errno %d", errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "UDP server listening on port %d", PORT);

    while (1) {
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
                           (struct sockaddr *)&client_addr, &socklen);
        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            break;
        }
        rx_buffer[len] = 0;
        ESP_LOGI(TAG, "Received %d bytes from %s:%d -> %s",
                 len,
                 inet_ntoa(client_addr.sin_addr),
                 ntohs(client_addr.sin_port),
                 rx_buffer);

        // Respondre amb ACK
        const char ack[] = "ACK";
        int err = sendto(sock, ack, sizeof(ack)-1, 0,
                         (struct sockaddr *)&client_addr, socklen);
        if (err < 0) {
            ESP_LOGE(TAG, "sendto failed: errno %d", errno);
        }
    }

    close(sock);
    vTaskDelete(NULL);

}

extern "C" void app_main() {
    nvs_flash_init();
    wifi_init_ap();
    leds_init();

    // Llança la tasca del servidor UDP
    xTaskCreate(udp_server_task, "udp_server", 4096, NULL, 5, NULL);

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr("192.168.4.2"); // IP del receptor
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);


    gpio_set_direction(LED_R, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_G, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_B, GPIO_MODE_OUTPUT);

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

        if (button_state == 1) { // Botó premut
            int dx = x - 1930;
            int dy = y - 1880;

            if (abs(dx) > abs(dy)) {
                // Moviment horitzontal dominant
                if (dx > DEADZONE) {
                    // Dreta → vermell
                    gpio_set_level(LED_R, 1);
                    gpio_set_level(LED_G, 0);
                    gpio_set_level(LED_B, 0);
                } else if (dx < -DEADZONE) {
                    // Esquerra → blau
                    gpio_set_level(LED_R, 0);
                    gpio_set_level(LED_G, 0);
                    gpio_set_level(LED_B, 1);
                } else {
                    gpio_set_level(LED_R, 0);
                    gpio_set_level(LED_G, 0);
                    gpio_set_level(LED_B, 0);
                }
            } else {
                // Moviment vertical dominant
                if (dy > DEADZONE) {
                    // Amunt → verd
                    gpio_set_level(LED_R, 0);
                    gpio_set_level(LED_G, 1);
                    gpio_set_level(LED_B, 0);
                } else if (dy < -DEADZONE) {
                    // Avall → groc
                    gpio_set_level(LED_R, 1);
                    gpio_set_level(LED_G, 1);
                    gpio_set_level(LED_B, 0);
                } else {
                    gpio_set_level(LED_R, 0);
                    gpio_set_level(LED_G, 0);
                    gpio_set_level(LED_B, 0);
                }
            }
        } else {
            // Botó no premut
            gpio_set_level(LED_R, 0);
            gpio_set_level(LED_G, 0);
            gpio_set_level(LED_B, 0);
        }

        std::string dir = "CENTER";
        if (button_state == 1) { // Botó premut
            int dx = x - 1930;
            int dy = y - 1880;

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
                    dir = "DOWN";
                } else if (dy < -DEADZONE) {
                    dir = "UP";
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
