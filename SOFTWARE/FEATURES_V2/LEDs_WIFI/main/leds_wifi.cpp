// leds_wifi.cpp
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_err.h"
#include "driver/gpio.h"

#define WIFI_SSID     "iPhone Aleix"
#define WIFI_PASS     "12345678"
#define PORT          4444

#define LED1 GPIO_NUM_12
#define LED2 GPIO_NUM_13
#define LED3 GPIO_NUM_14
#define LED4 GPIO_NUM_15

static const char *TAG = "UDP_SERVER";

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
static void wifi_init_sta(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to Wi-Fi...");
    ESP_ERROR_CHECK(esp_wifi_connect());

    vTaskDelay(pdMS_TO_TICKS(5000)); // espera una mica
    ESP_LOGI(TAG, "Assume connected (check monitor logs)");
}

// 🔹 Tasca per llegir la potència WiFi i actualitzar LEDs
void wifi_signal_task(void *pvParameters)
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
}

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

extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();
    leds_init();

    // Llança les dues tasques
    xTaskCreate(udp_server_task, "udp_server", 4096, NULL, 5, NULL);
    xTaskCreate(wifi_signal_task, "wifi_signal", 2048, NULL, 4, NULL);
}
