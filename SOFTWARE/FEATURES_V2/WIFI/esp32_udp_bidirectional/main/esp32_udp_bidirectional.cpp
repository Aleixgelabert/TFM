#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#define WIFI_SSID "SkorpiosContainer "
#define WIFI_PASS "Monaco001"

// Configura l’IP del dispositiu i del peer
#define LOCAL_IP "192.168.1.100"     // ESP32 A, canvia al B si és l’altre dispositiu
#define PEER_IP  "192.168.1.101"     // IP de l’altre ESP32
#define PORT 4444

static const char *TAG = "UDP_BIDIR";

/* ------------------- Inicialització Wi-Fi amb IP estàtica ------------------- */
void wifi_init_sta(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();

    // Configuració Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

    // Configura IP estàtica
    esp_netif_ip_info_t ip_info;
    ip_info.ip.addr = ipaddr_addr(LOCAL_IP);
    ip_info.gw.addr = ipaddr_addr("192.168.1.1"); // porta d’enllaç
    ip_info.netmask.addr = ipaddr_addr("255.255.255.0");
    esp_netif_dhcpc_stop(sta_netif);            // deshabilita DHCP
    esp_netif_set_ip_info(sta_netif, &ip_info);

    esp_wifi_start();
    esp_wifi_connect();

    vTaskDelay(3000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Wi-Fi configurat amb IP: %s", LOCAL_IP);
}

/* ------------------- Tasca servidor UDP ------------------- */
void udp_server_task(void *pvParameters)
{
    int sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char rx_buffer[128];

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

    ESP_LOGI(TAG, "Servidor UDP escoltant al port %d", PORT);

    while(1) {
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer)-1, 0,
                           (struct sockaddr *)&client_addr, &addr_len);
        if(len > 0) {
            rx_buffer[len] = 0;
            ESP_LOGI(TAG, "Rebut de %s:%d -> %s",
                     inet_ntoa(client_addr.sin_addr),
                     ntohs(client_addr.sin_port),
                     rx_buffer);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    close(sock);
    vTaskDelete(NULL);
}

/* ------------------- Tasca client UDP ------------------- */
void udp_client_task(void *pvParameters)
{
    int sock;
    struct sockaddr_in dest_addr;

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    dest_addr.sin_addr.s_addr = inet_addr(PEER_IP);

    int counter = 0;
    char message[64];

    while(1) {
        sprintf(message, "Hola des de ESP32 %s, missatge #%d", LOCAL_IP, counter++);
        sendto(sock, message, strlen(message), 0,
               (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        ESP_LOGI(TAG, "Client: enviat -> %s", message);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    close(sock);
    vTaskDelete(NULL);
}

/* ------------------- Main ------------------- */
extern "C" void app_main(void)
{
    nvs_flash_init();
    wifi_init_sta();

    xTaskCreate(udp_server_task, "udp_server", 4096, NULL, 5, NULL);
    xTaskCreate(udp_client_task, "udp_client", 4096, NULL, 5, NULL);
}
