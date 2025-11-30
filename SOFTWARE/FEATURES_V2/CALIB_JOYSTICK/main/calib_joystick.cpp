#include "driver/adc.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <math.h>

// Canvia aquests segons com tinguis el cablejat
#define RX_PIN_X ADC1_CHANNEL_6  // GPIO34
#define RX_PIN_Y ADC1_CHANNEL_7  // GPIO35

#define DEADZONE 200 // marge per considerar "CENTER"

extern "C" void app_main() {
    // Configura ADC
    adc1_config_width(ADC_WIDTH_BIT_12); // resolució 12 bits (0-4095)
    adc1_config_channel_atten(RX_PIN_X, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(RX_PIN_Y, ADC_ATTEN_DB_11);

    int min_x = 4095, max_x = 0;
    int min_y = 4095, max_y = 0;
    int center_x = 0, center_y = 0;
    int sample_count = 0;

    printf("\n=== MODE CALIBRATGE JOYSTICK ===\n");
    printf("Mou el joystick a totes les cantonades i deixa'l al centre al final.\n");
    printf("Llegeix valors X i Y cada 200ms...\n\n");

    while (true) {
        int x = adc1_get_raw(RX_PIN_X);
        int y = adc1_get_raw(RX_PIN_Y);

        // Actualitza mínims i màxims
        if (x < min_x) min_x = x;
        if (x > max_x) max_x = x;
        if (y < min_y) min_y = y;
        if (y > max_y) max_y = y;

        // Calcula mitjana (centre aproximat)
        center_x = ((center_x * sample_count) + x) / (sample_count + 1);
        center_y = ((center_y * sample_count) + y) / (sample_count + 1);
        sample_count++;

        // Determina direcció segons el moviment
        const char* direction = "CENTER";
        if (x > center_x + DEADZONE) direction = "+X";
        else if (x < center_x - DEADZONE) direction = "-X";
        else if (y > center_y + DEADZONE) direction = "+Y";
        else if (y < center_y - DEADZONE) direction = "-Y";

        printf("X=%4d  Y=%4d  | Dir=%-6s  | X[min=%4d max=%4d] Y[min=%4d max=%4d]\n",
               x, y, direction, min_x, max_x, min_y, max_y);

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
