//led_bateria.cpp

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#define ADC_PIN ADC1_CHANNEL_6  // GPIO34
#define LED_R GPIO_NUM_25
#define LED_G GPIO_NUM_26
#define LED_B GPIO_NUM_27

// Tensió màxima i mínima de la bateria
#define BATT_MIN_V 3.0
#define BATT_MAX_V 4.2

static esp_adc_cal_characteristics_t adc_chars;

// Funció per calcular percentatge de càrrega
float get_battery_soc(float voltage) {
    if (voltage < BATT_MIN_V) return 0.0;
    if (voltage > BATT_MAX_V) return 100.0;
    return ((voltage - BATT_MIN_V) / (BATT_MAX_V - BATT_MIN_V)) * 100.0;
}

extern "C" void app_main(void) {
    // Configura els pins dels LEDs
    gpio_set_direction(LED_R, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_G, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_B, GPIO_MODE_OUTPUT);

    // Configura l’ADC
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_PIN, ADC_ATTEN_DB_11); // fins a 3.9V aprox
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);

    while (true) {
        int raw = adc1_get_raw(ADC_PIN);
        float voltage = esp_adc_cal_raw_to_voltage(raw, &adc_chars) / 1000.0; // en volts
        voltage *= 2.0; // Compensa divisor resistiu 1:1

        float soc = get_battery_soc(voltage);
        printf("Voltage: %.2f V | SoC: %.1f%%\n", voltage, soc);

        // Decideix color LED
        if (soc < 30.0) {
            gpio_set_level(LED_R, 1);
            gpio_set_level(LED_G, 0);
            gpio_set_level(LED_B, 0);
        } else if (soc < 60.0) {
            gpio_set_level(LED_R, 1);
            gpio_set_level(LED_G, 1);
            gpio_set_level(LED_B, 0);
        } else {
            gpio_set_level(LED_R, 0);
            gpio_set_level(LED_G, 1);
            gpio_set_level(LED_B, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // 1 segon
    }
}
