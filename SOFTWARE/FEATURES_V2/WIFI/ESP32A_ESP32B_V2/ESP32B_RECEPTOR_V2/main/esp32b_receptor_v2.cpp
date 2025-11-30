// ============================================================================
// 📘 ESP32B Receptor V2
// 🔹 Funció: rep ordres UDP d’un altre ESP32 (emissor) i mostra l'ordre amb
//    l'il·luminació d'une LED RGB
// 🔹 A més, mostra la potència del senyal Wi-Fi amb 4 LEDs addicionals
// ============================================================================

// 🔹 1. Headers bàsics del sistema C/C++
#include <string>
#include <cstring>

// 🔹 2. FreeRTOS (sempre abans d’altres dependències de tasks o drivers)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 🔹 3. ESP-IDF core (NVS, Wi-Fi, events, netif, log, errors)
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_err.h"

// 🔹 4. LwIP (sockets i xarxa baixa)
#include "lwip/sockets.h"

// 🔹 5. Drivers de maquinari
#include "driver/gpio.h"

// 🔹 6. Altres (si tens sensors, biblioteques pròpies, etc.)

// ============================================================================
// ⚙️ CONFIGURACIÓ
// ============================================================================
// WIFI
#define WIFI_SSID "joystick_esp"
#define WIFI_PASS "12345678"
#define PORT 3333

//LEDs qualitat senyal WIFI
#define LED1 GPIO_NUM_12
#define LED2 GPIO_NUM_13
#define LED3 GPIO_NUM_14
#define LED4 GPIO_NUM_15

// LEDs funcionament joystick
#define LED_R GPIO_NUM_25
#define LED_G GPIO_NUM_26
#define LED_B GPIO_NUM_27

static const char *TAG = "ESP32B_RECEPTOR_V2";

// ============================================================================
// 💡 Inicialització dels LEDs de senyal Wi-Fi
// ============================================================================
static void leds_init() {
    gpio_reset_pin(LED1);
    gpio_reset_pin(LED2);
    gpio_reset_pin(LED3);
    gpio_reset_pin(LED4);

    gpio_set_direction(LED1, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED2, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED3, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED4, GPIO_MODE_OUTPUT);

    // Apaga'ls inicialment
    gpio_set_level(LED1, 0);
    gpio_set_level(LED2, 0);
    gpio_set_level(LED3, 0);
    gpio_set_level(LED4, 0);
}

// ============================================================================
// 📶 Actualitza els LEDs segons la potència del senyal RSSI
// ============================================================================
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

// ============================================================================
// 🌐 Connexió Wi-Fi en mode STA (client)
// ============================================================================
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

    ESP_LOGI(TAG, "Connectant a la xarxa Wi-Fi: %s ...", WIFI_SSID);
    ESP_ERROR_CHECK(esp_wifi_connect());

    vTaskDelay(pdMS_TO_TICKS(5000)); // espera de seguretat
    ESP_LOGI(TAG, "Connexió Wi-Fi iniciada (verifica logs per estat)");
}

// ============================================================================
// 📡 Tasca per mostrar la potència del senyal Wi-Fi
// ============================================================================
void wifi_signal_task(void *pvParameters)
{
    while (true) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            int rssi = ap_info.rssi;
            ESP_LOGI("WIFI_SIGNAL", "RSSI: %d dBm", rssi);
            set_leds_by_signal(rssi);
        } else {
            ESP_LOGW("WIFI_SIGNAL", "Sense conexió WIFI");
            // Apaguem tots els LEDs si no hi ha connexió
            gpio_set_level(LED1, 0);
            gpio_set_level(LED2, 0);
            gpio_set_level(LED3, 0);
            gpio_set_level(LED4, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); // cada 2 segons
    }
}

// ============================================================================
// 🛰️ Tasca servidor UDP: rep ordres i encén LEDs segons la direcció
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
            gpio_set_level(LED_R, 1);
            gpio_set_level(LED_G, 0);
            gpio_set_level(LED_B, 0);}
        else if (strcmp(rx_buffer, "UP") == 0){
            gpio_set_level(LED_R, 0);
            gpio_set_level(LED_G, 1);
            gpio_set_level(LED_B, 0);}
        else if (strcmp(rx_buffer, "LEFT") == 0){
            gpio_set_level(LED_R, 0);
            gpio_set_level(LED_G, 0);
            gpio_set_level(LED_B, 1);}
        else if (strcmp(rx_buffer, "DOWN") == 0) {
            gpio_set_level(LED_R, 1);
            gpio_set_level(LED_G, 1);
            gpio_set_level(LED_B, 0);}

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
extern "C" void app_main() 
{
    // Inicia NVS (memòria no volàtil)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

   // Configura WIFI en mode STA
    wifi_init_sta();

    // Inicialitza LEDs qualitat senyal WIFI
    leds_init();

    // Llança tasques FreeRTOS
    xTaskCreate(wifi_signal_task, "wifi_signal_task", 4096, NULL, 4, NULL);
    xTaskCreate(udp_server_task, "udp_server_task", 4096, NULL, 5, NULL);

    // Mantén el programa actiu
    vTaskDelay(portMAX_DELAY);
}