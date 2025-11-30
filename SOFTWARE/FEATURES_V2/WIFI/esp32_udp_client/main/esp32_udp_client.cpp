// esp32_udp_client.c
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

#define WIFI_SSID     "iPhone Aleix"
#define WIFI_PASS     "12345678"
#define SERVER_IP     "172.20.10.1"  // posa aquí la IP del servidor
#define PORT          4444

static const char *TAG = "UDP_CLIENT";

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
    vTaskDelay(pdMS_TO_TICKS(5000));
}

void udp_client_task(void *pvParameters)
{
    struct sockaddr_in dest_addr;
    char rx_buffer[128];

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    dest_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    const char message[] = "Hola des del client UDP ESP32";
    int err = sendto(sock, message, strlen(message), 0,
                     (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Error on sendto: errno %d", errno);
    } else {
        ESP_LOGI(TAG, "Message sent");
    }

    // Esperar resposta (blocking)
    struct sockaddr_in source_addr;
    socklen_t socklen = sizeof(source_addr);
    int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer)-1, 0,
                       (struct sockaddr *)&source_addr, &socklen);
    if (len > 0) {
        rx_buffer[len] = 0;
        ESP_LOGI(TAG, "Received from %s:%d -> %s",
                 inet_ntoa(source_addr.sin_addr),
                 ntohs(source_addr.sin_port),
                 rx_buffer);
    } else {
        ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
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

    xTaskCreate(udp_client_task, "udp_client", 4096, NULL, 5, NULL);
}
