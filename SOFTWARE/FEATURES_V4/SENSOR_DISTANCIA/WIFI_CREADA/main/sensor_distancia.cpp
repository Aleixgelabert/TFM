// sensor_distancia.cpp
// ESP32 actua com a punt d'accés i envia dades UDP cada segon a la pantalla (192.168.4.2)

#include <string>
#include <cstring>
#include <cstdio>
#include <cmath>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "lwip/sockets.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
}

static const char *TAG = "Sensor_distancia";

/* ---------- CONFIG ---------------------- */
#define WIFI_SSID       "WIFI_ESP32"
#define WIFI_PASS       "12345678"
#define UDP_PORT        3333
#define RECEIVER_IP     "192.168.4.2"   // IP del dispositiu receptor

#define TRIG_PIN        GPIO_NUM_17
#define ECHO_PIN        GPIO_NUM_18
/* ---------------------------------------- */

static void init_wifi_ap();
static float read_distance_cm();
static void udp_send_task(void *arg);

/* ---------- Wi-Fi Access Point ---------- */
static void init_wifi_ap()
{
    ESP_LOGI(TAG, "Inicialitzant NVS...");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.ap.ssid, WIFI_SSID, sizeof(wifi_config.ap.ssid) - 1);
    strncpy((char*)wifi_config.ap.password, WIFI_PASS, sizeof(wifi_config.ap.password) - 1);
    wifi_config.ap.ssid_len = strlen(WIFI_SSID);
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Punt d'accés creat → SSID:%s  PASS:%s", WIFI_SSID, WIFI_PASS);
    ESP_LOGI(TAG, "IP per defecte: 192.168.4.1");
}

/* ---------- HC-SR04 Distance Measurement ---------- */
static float read_distance_cm()
{
    gpio_set_level(TRIG_PIN, 0);
    esp_rom_delay_us(2);
    gpio_set_level(TRIG_PIN, 1);
    esp_rom_delay_us(10);
    gpio_set_level(TRIG_PIN, 0);

    int64_t start_time = esp_timer_get_time();
    while (gpio_get_level(ECHO_PIN) == 0) {
        if (esp_timer_get_time() - start_time > 2000000) return -1; // timeout 2s
    }

    int64_t echo_start = esp_timer_get_time();
    while (gpio_get_level(ECHO_PIN) == 1) {
        if (esp_timer_get_time() - echo_start > 2000000) return -1;
    }
    int64_t echo_end = esp_timer_get_time();

    float pulse_duration_us = echo_end - echo_start;
    float distance_cm = (pulse_duration_us / 2.0f) / 29.1f; // velocitat del so 343 m/s
    return distance_cm;
}

/* ---------- UDP Sender Task ---------- */
static void udp_send_task(void *arg)
{
    struct sockaddr_in dest_addr = {};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(UDP_PORT);
    inet_pton(AF_INET, RECEIVER_IP, &dest_addr.sin_addr.s_addr);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Error creant socket UDP");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Enviant dades UDP a %s:%d", RECEIVER_IP, UDP_PORT);

    while (true) {
        float dist = read_distance_cm();
        char msg[64];
        if (dist > 0)
            snprintf(msg, sizeof(msg), "%.1f", dist);
            // nprintf(msg, sizeof(msg), "Distància: %.1f cm", dist);

        else
            snprintf(msg, sizeof(msg), "Lectura invàlida");

        sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        ESP_LOGI(TAG, "Enviat: %s", msg);
        vTaskDelay(pdMS_TO_TICKS(1000)); // cada 1 s
    }

    close(sock);
    vTaskDelete(NULL);
}

/* ---------- app_main ---------- */
extern "C" void app_main()
{
    // Configuració dels pins del sensor HC-SR04
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << TRIG_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << ECHO_PIN);
    gpio_config(&io_conf);

    // Inicia Wi-Fi en mode punt d'accés
    init_wifi_ap();

    // Llança la tasca d'enviament UDP
    xTaskCreate(udp_send_task, "udp_send_task", 4096, NULL, 5, NULL);
}
