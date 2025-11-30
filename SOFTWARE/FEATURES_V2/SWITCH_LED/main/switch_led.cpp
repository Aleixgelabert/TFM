#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "BUTTON_LED";

// Definim els pins
#define LED_PIN     GPIO_NUM_5
#define BUTTON_PIN  GPIO_NUM_4

extern "C" void app_main(void)
{
    // Configuració del LED com a sortida
    gpio_config_t io_conf_led = {};
    io_conf_led.intr_type = GPIO_INTR_DISABLE;
    io_conf_led.mode = GPIO_MODE_OUTPUT;
    io_conf_led.pin_bit_mask = (1ULL << LED_PIN);
    io_conf_led.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf_led.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf_led);

    // Configuració del botó com a entrada amb pull-down intern
    gpio_config_t io_conf_button = {};
    io_conf_button.intr_type = GPIO_INTR_DISABLE;
    io_conf_button.mode = GPIO_MODE_INPUT;
    io_conf_button.pin_bit_mask = (1ULL << BUTTON_PIN);
    io_conf_button.pull_down_en = GPIO_PULLDOWN_ENABLE;  // Activem el pull-down intern
    io_conf_button.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf_button);

    ESP_LOGI(TAG, "Iniciant programa. Prem el botó per encendre el LED.");

    while (true) {
        int button_state = gpio_get_level(BUTTON_PIN);

        if (button_state == 1) {
            gpio_set_level(LED_PIN, 1);  // Encén LED
            ESP_LOGI(TAG, "Botó premut!");
        } else {
            gpio_set_level(LED_PIN, 0);  // Apaga LED
        }

        vTaskDelay(pdMS_TO_TICKS(50)); // Delay de 50 ms
    }
}
