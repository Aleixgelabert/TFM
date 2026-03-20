#include "system_tile.h"                          // Inclou el fitxer de capçalera propi d’aquest mòdul
#include "freertos/FreeRTOS.h"                    // Biblioteca base de FreeRTOS
#include "freertos/task.h"                        // Per crear i gestionar tasques
#include "freertos/semphr.h"                      // Per utilitzar semàfors
#include "esp_flash.h"                            // Per obtenir informació sobre la memòria flash
#include "esp_private/esp_clk.h"                  // Per llegir la freqüència de la CPU
#include "esp_pcf85063_port.h"                    // Control del rellotge RTC extern
#include "esp_3inch5_lcd_port.h"                  // Controlador per a la pantalla LCD 3.5”


// Variables globals per etiquetes de la interfície gràfica (LVGL)
lv_obj_t *label_brightness;
lv_obj_t *label_time;
lv_obj_t *label_date;
lv_obj_t *label_cylinder_pressure;
lv_obj_t *label_cylinder_position = nullptr;
lv_obj_t *label_cylinder_load;

//---------------------------------------------------------
// CALLBACK del slider de brillantor
//---------------------------------------------------------
static void slider_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);           // Obté el tipus d’esdeveniment
    if (code == LV_EVENT_VALUE_CHANGED)                    // Si el valor del slider ha canviat
    {
        lv_obj_t *slider = lv_event_get_target(e);          // Objecte slider que ha generat l’event
        int value = lv_slider_get_value(slider);            // Llegeix el valor del slider
        // printf("Slider value: %d\n", value);

        lv_label_set_text_fmt(label_brightness, "%d %%", value); // Actualitza el text de brillantor
        esp_3inch5_brightness_port_set(value);                   // Ajusta la brillantor del panell LCD realment
        lv_event_stop_bubbling(e);                               // Evita que l’event es propagui a altres objectes
    }
}

//---------------------------------------------------------
// CALLBACK del temporitzador que actualitza hora
//---------------------------------------------------------
static void system_time_cb(lv_timer_t *timer)
{
    char str[20];
    float tsens_out;
    RTC_DateTime datetime = rtc.getDateTime();                  // Llegeix la data i hora actual del RTC

    // Mostra la data i hora actuals a la interfície
    lv_label_set_text_fmt(label_date, "%02d-%02d-%d", datetime.day, datetime.month, datetime.year);
    lv_label_set_text_fmt(label_time, "%02d:%02d:%02d", datetime.hour, datetime.minute, datetime.second);
}


//---------------------------------------------------------
// Inicialització del sistema i lectura de recursos de maquinari
//---------------------------------------------------------
void system_init(void)
{
}


//---------------------------------------------------------
// Inicialitza tota la interfície gràfica del “System”
//---------------------------------------------------------
void system_tile_init(lv_obj_t *parent)
{
    lv_obj_t *list = lv_list_create(parent);                         // Crea una llista de paràmetres
    lv_obj_t *lable = lv_label_create(parent);                       // Crea el títol superior
    lv_obj_set_style_text_font(lable, &lv_font_montserrat_20, LV_PART_MAIN);    // Defineix la font del text de l’etiqueta com lv_font_montserrat_20, una mida de lletra mitjana/gran.
                                                                                // LV_PART_MAIN indica que el canvi s’aplica a la part principal de l’objecte.
    lv_label_set_text(lable, "J2 CYLINDER");
    lv_obj_align(lable, LV_ALIGN_TOP_MID, 0, 3);        // Col·loca el títol a la part superior i centrada horitzontalment (TOP_MID),
                                                        // amb un petit desplaçament vertical de 3 píxels.
    lv_obj_set_size(list, lv_pct(95), lv_pct(70));      // Dona a la llista una mida del 95% de l’amplada i 70% de l’alçada del contenidor pare (parent).
                                                        // Així ocupa gairebé tota la pantalla.
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 30);        // Col·loca la llista sota el títol, centrada a dalt, i amb 30 píxels de marge des del top.

    
    // Slider per controlar la brillantor
    lv_obj_t *slider = lv_slider_create(parent);
    lv_slider_set_range(slider, 1, 100);
    lv_slider_set_value(slider, 80, LV_ANIM_OFF);
    lv_obj_set_size(slider, lv_pct(50), lv_pct(5));
    lv_obj_align(slider, LV_ALIGN_BOTTOM_MID,75, -15);
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Elements de la llista amb diferents informacions del sistema
    lv_obj_t *list_item;

    list_item = lv_list_add_btn(list, NULL, "Position");
    label_cylinder_position = lv_label_create(list_item);
    lv_label_set_text(label_cylinder_position, "--- %");

    list_item = lv_list_add_btn(list, NULL, "Brightness");
    label_brightness = lv_label_create(list_item);
    lv_label_set_text(label_brightness, "80 %");

    list_item = lv_list_add_btn(list, NULL, "Load");
    label_cylinder_load = lv_label_create(list_item);
    lv_label_set_text(label_cylinder_load, "--- Kg");

    list_item = lv_list_add_btn(list, NULL, "Pressure");
    label_cylinder_pressure = lv_label_create(list_item);
    lv_label_set_text(label_cylinder_pressure, "--- BAR");

    list_item = lv_list_add_btn(list, NULL, "Date");
    label_date = lv_label_create(list_item);
    lv_label_set_text(label_date, "XX-XX-XXXX");

    list_item = lv_list_add_btn(list, NULL, "Time");
    label_time = lv_label_create(list_item);
    lv_label_set_text(label_time, "12:00:00");

    system_init();                                     // Inicialitza el maquinari
    lv_timer_create(system_time_cb, 1000, NULL);       // Temporitzador per actualitzar hora/temp cada segon
}
