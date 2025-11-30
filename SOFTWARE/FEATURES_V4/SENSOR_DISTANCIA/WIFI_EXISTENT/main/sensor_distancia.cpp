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

static const char *TAG = "hc_sr04_sender";

/* ---------- CONFIG ---------------------- */
#define WIFI_SSID       "WIFI_ESP32"
#define WIFI_PASS       "12345678"
#define UDP_PORT        3333
#define RECEIVER_IP     "192.168.4.2"   // ⚠️ Canvia-ho per la IP del teu panell ESP32

#define TRIG_PIN        GPIO_NUM_17
#define ECHO_PIN        GPIO_NUM_18
/* ---------------------------------------- */

static void init_wifi();
static float read_distance_cm();
static void udp_send_task(void *arg);

/* Wi-Fi event handler */
extern "C" void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGW(TAG, "Wi-Fi disconnected, reconnecting...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

static void init_wifi()
{
    ESP_LOGI(TAG, "Init NVS");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, WIFI_PASS, sizeof(wifi_config.sta.password) - 1);

    ESP_LOGI(TAG, "Connecting to Wi-Fi %s", WIFI_SSID);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/* HC-SR04 distance measurement */
static float read_distance_cm()
{
    // Send trigger pulse (10µs)
    gpio_set_level(TRIG_PIN, 0);
    esp_rom_delay_us(2);
    gpio_set_level(TRIG_PIN, 1);
    esp_rom_delay_us(10);
    gpio_set_level(TRIG_PIN, 0);

    // Wait for echo start
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
    float distance_cm = (pulse_duration_us / 2.0f) / 29.1f; // sound speed 343 m/s
    return distance_cm;
}

/* UDP sender task */
static void udp_send_task(void *arg)
{
    struct sockaddr_in dest_addr = {};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(UDP_PORT);
    inet_pton(AF_INET, RECEIVER_IP, &dest_addr.sin_addr.s_addr);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Cannot create socket");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "UDP sender ready → %s:%d", RECEIVER_IP, UDP_PORT);

    while (true) {
        float dist = read_distance_cm();
        char msg[64];
        if (dist > 0)
            snprintf(msg, sizeof(msg), "Distància: %.1f cm", dist);
        else
            snprintf(msg, sizeof(msg), "Lectura invàlida");

        sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        ESP_LOGI(TAG, "Sent: %s", msg);
        vTaskDelay(pdMS_TO_TICKS(1000)); // cada 1 s
    }

    close(sock);
    vTaskDelete(NULL);
}

/* app_main */
extern "C" void app_main()
{
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

    init_wifi();

    xTaskCreate(udp_send_task, "udp_send_task", 4096, NULL, 5, NULL);
}
