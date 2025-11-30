#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"

#define JOY_X ADC1_CHANNEL_6
#define JOY_Y ADC1_CHANNEL_7
#define BUTTON GPIO_NUM_4

#define LED_R GPIO_NUM_27
#define LED_G GPIO_NUM_26
#define LED_B GPIO_NUM_25

#define DEADZONE 400 // Zona morta

extern "C" void app_main(void) {
    gpio_set_direction(LED_R, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_G, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_B, GPIO_MODE_OUTPUT);

    gpio_set_direction(BUTTON, GPIO_MODE_INPUT);
    gpio_pulldown_en(BUTTON);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(JOY_X, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(JOY_Y, ADC_ATTEN_DB_11);

    // Variables per controlar els canvis
    int last_x = -1;
    int last_y = -1;
    int last_button = -1;

    while (true) {
        int x = adc1_get_raw(JOY_X);
        int y = adc1_get_raw(JOY_Y);
        int button_state = gpio_get_level(BUTTON);

        // Mostra els valors només si han canviat
        if (x != last_x || y != last_y || button_state != last_button) {
            printf("Joystick X=%d Y=%d Button=%d\n", x, y, button_state);
            last_x = x;
            last_y = y;
            last_button = button_state;
        }

        if (button_state == 1) { // Botó premut
            int dx = x - 2048;
            int dy = y - 2048;

            if (abs(dx) > abs(dy)) {
                // Moviment horitzontal dominant
                if (dx > DEADZONE) {
                    // Dreta → vermell
                    gpio_set_level(LED_R, 1);
                    gpio_set_level(LED_G, 0);
                    gpio_set_level(LED_B, 0);
                } else if (dx < -DEADZONE) {
                    // Esquerra → blau
                    gpio_set_level(LED_R, 0);
                    gpio_set_level(LED_G, 0);
                    gpio_set_level(LED_B, 1);
                } else {
                    gpio_set_level(LED_R, 0);
                    gpio_set_level(LED_G, 0);
                    gpio_set_level(LED_B, 0);
                }
            } else {
                // Moviment vertical dominant
                if (dy > DEADZONE) {
                    // Amunt → verd
                    gpio_set_level(LED_R, 0);
                    gpio_set_level(LED_G, 1);
                    gpio_set_level(LED_B, 0);
                } else if (dy < -DEADZONE) {
                    // Avall → groc
                    gpio_set_level(LED_R, 1);
                    gpio_set_level(LED_G, 1);
                    gpio_set_level(LED_B, 0);
                } else {
                    gpio_set_level(LED_R, 0);
                    gpio_set_level(LED_G, 0);
                    gpio_set_level(LED_B, 0);
                }
            }
        } else {
            // Botó no premut
            gpio_set_level(LED_R, 0);
            gpio_set_level(LED_G, 0);
            gpio_set_level(LED_B, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
