// ============================================================================
// 📘 ESP32A Emissor V3
// 🔹 Funció: Crea una WIFI i rep ordres UDP d'un jostick que mostra en un
//    LED RGB.
// ============================================================================

// 🔹 1. Headers bàsics del sistema C/C++
#include <stdio.h>
#include <string>

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

// ============================================================================
// ⚙️ CONFIGURACIÓ
// ============================================================================
// WIFI
#define WIFI_SSID "joystick_esp"
#define WIFI_PASS "12345678"
#define PORT 3333

// LEDs funcionament joystick
#define LED_R GPIO_NUM_25
#define LED_G GPIO_NUM_26
#define LED_B GPIO_NUM_27

// Identificació de les linies de log
static const char *TAG = "ESP32A_EMISSOR_V3";

// ============================================================================
// 💡 Configuració del Wi-Fi com a Punt d'Accés (AP)
// ============================================================================
static void wifi_init_ap() {
   ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t ap_config = {};
    strcpy((char *)ap_config.ap.ssid, WIFI_SSID);
    strcpy((char *)ap_config.ap.password, WIFI_PASS);
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

    ESP_LOGI(TAG, "✅ Wi-Fi AP creat -> SSID: %s | PASS: %s", WIFI_SSID, WIFI_PASS);
}

// ============================================================================
// 🛰️ Tasca UDP: rep ordres i encén LEDs segons la direcció
// ============================================================================
void udp_server_task(void *pvParameters)
{
    struct sockaddr_in server_addr, client_addr;
    socklen_t socklen = sizeof(client_addr);
    char rx_buffer[128];

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Error creant socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Error fent bind del socket: errno %d", errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Servidor UDP escoltant al port %d", PORT);

// Configura LEDs del joystick
    gpio_set_direction(LED_R, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_G, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_B, GPIO_MODE_OUTPUT);

    while (true) {
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
                           (struct sockaddr *)&client_addr, &socklen);
        if (len < 0) {
            ESP_LOGE(TAG, "Error a recvfrom: errno %d", errno);
            break;
        }
        rx_buffer[len] = 0;
        ESP_LOGI(TAG, "Missatge rebut de %s:%d -> %s",
                 inet_ntoa(client_addr.sin_addr),
                 ntohs(client_addr.sin_port),
                 rx_buffer);

        // Reinicia LEDs
        gpio_set_level(LED_R, 0);
        gpio_set_level(LED_G, 0);
        gpio_set_level(LED_B, 0);

        // 🔹 Interpreta la direcció rebuda
        if (strcmp(rx_buffer, "RIGHT") == 0){
            gpio_set_level(LED_R, 1);}
        else if (strcmp(rx_buffer, "DOWN") == 0){
            gpio_set_level(LED_G, 1);}
        else if (strcmp(rx_buffer, "LEFT") == 0){
            gpio_set_level(LED_B, 1);}
        else if (strcmp(rx_buffer, "UP") == 0) {
            gpio_set_level(LED_R, 1);
            gpio_set_level(LED_G, 1);}

        // Envia resposta ACK a l'emissor
        const char ack[] = "ACK";
        int err = sendto(sock, ack, sizeof(ack)-1, 0,
                         (struct sockaddr *)&client_addr, socklen);
        if (err < 0) {
            ESP_LOGE(TAG, "Error enviant ACK: errno %d", errno);
        }
    }

    close(sock);
    vTaskDelete(NULL);
}

// ============================================================================
// 🚀 Funció principal
// ============================================================================
extern "C" void app_main() {
    // Inicialitza NVS
    ESP_ERROR_CHECK(nvs_flash_init());

    // Crea la Wi-Fi
    wifi_init_ap();

    // Llança la tasca UDP
    xTaskCreate(udp_server_task, "udp_server", 4096, NULL, 5, NULL);

}
