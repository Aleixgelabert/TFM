#include "system_tile.h"                          // Inclou el fitxer de capçalera propi d’aquest mòdul
#include "freertos/FreeRTOS.h"                    // Biblioteca base de FreeRTOS
#include "freertos/task.h"                        // Per crear i gestionar tasques
#include "freertos/semphr.h"                      // Per utilitzar semàfors
#include "esp_flash.h"                            // Per obtenir informació sobre la memòria flash
#include "esp_private/esp_clk.h"                  // Per llegir la freqüència de la CPU
#include "esp_pcf85063_port.h"                    // Control del rellotge RTC extern
#include "esp_3inch5_lcd_port.h"                  // Controlador per a la pantalla LCD 3.5”
#include "esp_axp2101_port.h"                     // Controlador ax2101
#include "esp_wifi.h"
#include "esp_netif.h"
#include <stdbool.h>
#include "battery_0.h"
#include "battery_25.h"
#include "battery_50.h"
#include "battery_75.h"
#include "battery_100.h"
#include "battery_charging.h"
#include "wifi_off.h"
#include "wifi_1.h"
#include "wifi_2.h"
#include "wifi_3.h"
#include "pmu_battery.h"



// Declaració de les icones de Wi-Fi i Bateria
LV_IMG_DECLARE(battery_0);
LV_IMG_DECLARE(battery_25);
LV_IMG_DECLARE(battery_50);
LV_IMG_DECLARE(battery_75);
LV_IMG_DECLARE(battery_100);
LV_IMG_DECLARE(battery_charging);

LV_IMG_DECLARE(wifi_off);
LV_IMG_DECLARE(wifi_1);
LV_IMG_DECLARE(wifi_2);
LV_IMG_DECLARE(wifi_3);


// Variables globals per etiquetes de la interfície gràfica (LVGL)
lv_obj_t *label_brightness;
//lv_obj_t *label_time;
//lv_obj_t *label_date;
lv_obj_t *label_cylinder_pressure;
lv_obj_t *label_cylinder_position = nullptr;
lv_obj_t *label_cylinder_load;
lv_obj_t *label_joystick_position_B;
lv_obj_t *label_output_joystick_B;
lv_obj_t *icon_battery = NULL;
lv_obj_t *icon_wifi = NULL;
lv_obj_t *label_batt_pct = NULL;


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
/*
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


static void system_status_cb(lv_timer_t *t)
{
    int pct = pmu_get_battery_percentage();
    bool chg = pmu_is_charging();

    system_update_battery(pct, chg);
    system_update_wifi(system_get_wifi_level());
}
*/

//---------------------------------------------------------
// Inicialització del sistema i lectura de recursos de maquinari
//---------------------------------------------------------
void system_init(void)
{
    // Aquí pots inicialitzar sensors, PMU, RTC si cal
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
    lv_label_set_text(lable, "TRIM TAB");
    lv_obj_align(lable, LV_ALIGN_TOP_MID, 0, 10);        // Col·loca el títol a la part superior i centrada horitzontalment (TOP_MID),
                                                        // amb un petit desplaçament vertical de 3 píxels.
    lv_obj_set_size(list, lv_pct(95), lv_pct(70));      // Dona a la llista una mida del 95% de l’amplada i 70% de l’alçada del contenidor pare (parent).
                                                        // Així ocupa gairebé tota la pantalla.
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 40);        // Col·loca la llista sota el títol, centrada a dalt, i amb 30 píxels de marge des del top.

    
    // Slider per controlar la brillantor
    lv_obj_t *slider = lv_slider_create(parent);
    lv_slider_set_range(slider, 1, 100);
    lv_slider_set_value(slider, 80, LV_ANIM_OFF);
    lv_obj_set_size(slider, lv_pct(50), lv_pct(5));
    lv_obj_align(slider, LV_ALIGN_BOTTOM_MID,75, -15);
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Elements de la llista amb diferents informacions del sistema
    lv_obj_t *list_item;

    list_item = lv_list_add_btn(list, NULL, "Brightness");
    label_brightness = lv_label_create(list_item);
    lv_label_set_text(label_brightness, "80 %");

    list_item = lv_list_add_btn(list, NULL, "Distance");
    label_cylinder_position = lv_label_create(list_item);
    lv_label_set_text(label_cylinder_position, "--- cm");

    list_item = lv_list_add_btn(list, NULL, "Pressure");
    label_cylinder_pressure = lv_label_create(list_item);
    lv_label_set_text(label_cylinder_pressure, "--- BAR");

    list_item = lv_list_add_btn(list, NULL, "Load");
    label_cylinder_load = lv_label_create(list_item);
    lv_label_set_text(label_cylinder_load, "--- Kg");

    list_item = lv_list_add_btn(list, NULL, "Output Joystick B");
    label_output_joystick_B = lv_label_create(list_item);
    lv_label_set_text(label_output_joystick_B, "--- mV");

    list_item = lv_list_add_btn(list, NULL, "Position Joystick B");
    label_joystick_position_B = lv_label_create(list_item);
    lv_label_set_text(label_joystick_position_B, "--- %");

    /*list_item = lv_list_add_btn(list, NULL, "Date");
    label_date = lv_label_create(list_item);
    lv_label_set_text(label_date, "XX-XX-XXXX");

    list_item = lv_list_add_btn(list, NULL, "Time");
    label_time = lv_label_create(list_item);
    lv_label_set_text(label_time, "12:00:00");*/

    system_init();                                     // Inicialitza el maquinari
    /*lv_timer_create(system_time_cb, 1000, NULL);       // Temporitzador per actualitzar hora/temp cada segon
    lv_timer_create(system_status_cb, 2000, NULL);*/

    // Icona bateria
    icon_battery = lv_img_create(parent);
    lv_obj_align(icon_battery, LV_ALIGN_TOP_RIGHT, -17, -10);

    // Percentatge de bateria
    label_batt_pct = lv_label_create(parent);
    lv_label_set_text(label_batt_pct, "");
    lv_obj_align(label_batt_pct, LV_ALIGN_TOP_RIGHT, -10, 12);
    lv_obj_set_style_text_font(label_batt_pct, &lv_font_montserrat_12, 0);

    // -------- CANVI: Actualitzar el label amb valor real de bateria si hi ha dades --------
    int pct = pmu_get_battery_percentage();
    bool chg = pmu_is_charging();
    system_update_battery(pct, chg);


    // Icona WiFi
    icon_wifi = lv_img_create(parent);
    lv_obj_align(icon_wifi, LV_ALIGN_TOP_RIGHT, -50, -10);

    lv_timer_create([](lv_timer_t *t){
    int pct = pmu_get_battery_percentage();
    bool chg = pmu_is_charging();

    system_update_battery(pct, chg);
    system_update_wifi(system_get_wifi_level());

    }, 2000, NULL);


}


// Actualitzar icona bateria segons percentatge i estat
void system_update_battery(int pct, bool charging)
{
    const lv_img_dsc_t *img;

    if (charging)
        img = &battery_charging;
    else if (pct > 80)
        img = &battery_100;
    else if (pct > 60)
        img = &battery_75;
    else if (pct > 40)
        img = &battery_50;
    else if (pct > 20)
        img = &battery_25;
    else
        img = &battery_0;

    lv_img_set_src(icon_battery, img);

    /* Escalar la imatge */
    lv_img_set_zoom(icon_battery, 128);  // 50% de la mida original
    
    // Actualitza el % al widget
    char buff[8];
    snprintf(buff, sizeof(buff), "%d%%", pct);
    lv_label_set_text(label_batt_pct, buff);

    // ---- Color del text ----
    if (charging)
    {
        lv_obj_set_style_text_color(label_batt_pct, lv_color_hex(0x3B82F6), 0);  // Blau
    }
    else if (pct > 50)
    {
        lv_obj_set_style_text_color(label_batt_pct, lv_color_hex(0x22C55E), 0);  // Verd
    }
    else if (pct > 20)
    {
        lv_obj_set_style_text_color(label_batt_pct, lv_color_hex(0xEAB308), 0);  // Groc
    }
    else
    {
        lv_obj_set_style_text_color(label_batt_pct, lv_color_hex(0xEF4444), 0);  // Vermell
    }

}


// Actualitzar icona Wi-Fi segons RSSI:
void system_update_wifi(int level)
{
    switch(level)
    {
        case 3: lv_img_set_src(icon_wifi, &wifi_3); break;
        case 2: lv_img_set_src(icon_wifi, &wifi_2); break;
        case 1: lv_img_set_src(icon_wifi, &wifi_1); break;
        default:
            lv_img_set_src(icon_wifi, &wifi_off);
            break;
    }
        /* Escalar la icona Wi-Fi */
    lv_img_set_zoom(icon_wifi, 128);  // Ajusta la mida de la icona
}


// Lector RSSI ---> Nivell Wi-Fi
int system_get_wifi_level()
{
    wifi_ap_record_t info;

    if (esp_wifi_sta_get_ap_info(&info) != ESP_OK)
        return 0;

    int rssi = info.rssi;

    if (rssi > -55) return 3;
    if (rssi > -65) return 2;
    if (rssi > -75) return 1;
    return 0;
}
