// esp32_udp_server.c
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


#define WIFI_SSID     "SkorpiosContainer "
#define WIFI_PASS     "Monaco001"
#define PORT          4444

static const char *TAG = "UDP_SERVER";

/* Simple Wi-Fi connect (blocking-ish). For producció s'usa event handler. */
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
            /* si tens WEP/WPA2 etc, afegeix configuració addicional si cal */
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to Wi-Fi...");
    ESP_ERROR_CHECK(esp_wifi_connect());

    /* esperar ip (simplificat): */
    vTaskDelay(pdMS_TO_TICKS(5000));
    esp_ip4_addr_t ip4;
    // No fem capture d'IP precisa aquí per simplicitat; el monitor mostrarà l'IP.
    ESP_LOGI(TAG, "Assume connected (check monitor logs)");
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

    xTaskCreate(udp_server_task, "udp_server", 4096, NULL, 5, NULL);
}
