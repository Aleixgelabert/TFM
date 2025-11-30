#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"

// Wi-Fi i PLC
const char* ssid = "SkorpiosContainer";
const char* password = "Monaco001";

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int sock;
struct sockaddr_in dest_addr;

sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
if (sock < 0) {
    printf("Error creant el socket\n");
}

dest_addr.sin_family = AF_INET;
dest_addr.sin_port = htons(12000); // port del PLC
dest_addr.sin_addr.s_addr = inet_addr("192.168.1.50"); // IP del PLC


// Pins
#define JOY_X 34
#define JOY_Y 35
#define BTN_CONFIRM 25
#define BAT_ADC 36
#define LED_BAT_100 26
#define LED_BAT_50 27
#define LED_BAT_30 2
#define LED_WIFI_1 12
#define LED_WIFI_2 13
#define LED_WIFI_3 15
#define LED_WIFI_4 14

#define INACTIVITY_TIMEOUT 60000 // 1 minut

unsigned long lastActivity = 0;
int lastX = 0, lastY = 0;
int lastButton = 1;

// Funcions auxiliars
float readBatteryVoltage() {
    int raw = analogRead(BAT_ADC);
    float v_adc = (raw / 4095.0) * 3.3;
    return v_adc * (147.0 / 47.0);
}

void showBatteryLevel(float vbat) {
    digitalWrite(LED_BAT_100, LOW);
    digitalWrite(LED_BAT_50, LOW);
    digitalWrite(LED_BAT_30, LOW);
    float soc = (vbat - 3.3) / (4.2 - 3.3) * 100;
    soc = constrain(soc, 0, 100);
    if (soc < 30) digitalWrite(LED_BAT_30, HIGH);
    else if (soc < 50) digitalWrite(LED_BAT_50, HIGH);
    else digitalWrite(LED_BAT_100, HIGH);
}

void showWifiBars() {
    int rssi = WiFi.RSSI(); // -30 a -90
    int n_leds = 0;
    if (rssi > -45) n_leds = 4;
    else if (rssi > -60) n_leds = 3;
    else if (rssi > -75) n_leds = 2;
    else n_leds = 1;

    digitalWrite(LED_WIFI_1, n_leds >= 1);
    digitalWrite(LED_WIFI_2, n_leds >= 2);
    digitalWrite(LED_WIFI_3, n_leds >= 3);
    digitalWrite(LED_WIFI_4, n_leds >= 4);
}

// Punt d'entrada ESP-IDF
extern "C" void app_main(void) {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { 
        vTaskDelay(pdMS_TO_TICKS(200)); 
        Serial.print("."); 
    }
    Serial.println("\nWiFi connectat");

    

    pinMode(BTN_CONFIRM, INPUT_PULLUP);
    pinMode(LED_BAT_100, OUTPUT);
    pinMode(LED_BAT_50, OUTPUT);
    pinMode(LED_BAT_30, OUTPUT);
    pinMode(LED_WIFI_1, OUTPUT);
    pinMode(LED_WIFI_2, OUTPUT);
    pinMode(LED_WIFI_3, OUTPUT);
    pinMode(LED_WIFI_4, OUTPUT);
    analogReadResolution(12);

    lastActivity = millis();

    while (1) {
        int x = analogRead(JOY_X);
        int y = analogRead(JOY_Y);
        int button = digitalRead(BTN_CONFIRM);

        bool moved = (abs(x - lastX) > 50) || (abs(y - lastY) > 50);
        bool pressed = (button == LOW && lastButton == HIGH);

        if (moved || pressed) {
            lastActivity = millis();
            char msg[64];
            snprintf(msg, sizeof(msg), "X:%d Y:%d BTN:%d", x, y, button == LOW ? 1 : 0);
            int err = sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            if (err < 0) {
                printf("Error enviant missatge\n");
            Serial.println(msg);
        }

        lastX = x;
        lastY = y;
        lastButton = button;

        showBatteryLevel(readBatteryVoltage());
        showWifiBars();

        if (millis() - lastActivity > INACTIVITY_TIMEOUT) {
            Serial.println("1 min sense activitat -> Light Sleep");
            esp_sleep_enable_ext0_wakeup((gpio_num_t)BTN_CONFIRM, 0);
            esp_light_sleep_start();
            Serial.println("Surt de Light Sleep");
            lastActivity = millis();
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
