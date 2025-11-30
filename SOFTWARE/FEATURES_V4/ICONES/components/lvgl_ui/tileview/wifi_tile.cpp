//---------------------------------------------------------
// FITXER DE PROVES PER CANVIAR DADES PANTALLA
//---------------------------------------------------------
#include "wifi_tile.h"             // Header propi de la tile WiFi
#include "freertos/FreeRTOS.h"     // FreeRTOS bàsic
#include "freertos/task.h"         // Tasques de FreeRTOS
#include "esp_lvgl_port.h"         // Port d’integració LVGL <-> ESP-IDF
#include "esp_wifi_port.h"         // Port de control del WiFi (funcions esp_wifi_port_*)
#include "lvgl.h"
#include "esp_log.h"

static lv_obj_t *list;             // Llista on es mostraran les xarxes WiFi
lv_obj_t *lable_wifi_ip;           // Etiqueta per mostrar la IP actual del dispositiu

#define LIST_BTN_LEN_MAX 20        // Màxim nombre de xarxes que es mostraran
lv_obj_t *list_btns[LIST_BTN_LEN_MAX]; // Array amb els botons (un per cada xarxa trobada)
uint16_t list_item_count = 0;      // Comptador d’elements actuals a la llista

bool g_wifi_enable = true;         // Estat global del WiFi (actiu o no)

SemaphoreHandle_t wifi_scanf_semaphore; // Semàfor per sincronitzar escaneig entre UI i tasca

//---------------------------------------------------------
// Callback del botó "Scan"
//---------------------------------------------------------
static void btn_wifi_scan_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);     // Llegeix quin tipus d’event ha passat
    // lv_obj_t *obj = lv_event_get_target(e);       // (opcional) objecte que ha generat l’event

    if (code == LV_EVENT_CLICKED && g_wifi_enable)   // Només si s’ha fet clic i WiFi està actiu
    {
        for (int i = 0; i < list_item_count; i++)    // Esborra tots els elements de la llista actual
        {
            lv_obj_del(list_btns[i]);
        }
        list_item_count = 0;                         // Reinicia el comptador

        // Mostra un missatge temporal mentre s’escaneja
        list_btns[0] = lv_list_add_btn(list, NULL, "WiFi scanning underway!");

        // Dona el semàfor perquè la tasca lvgl_wifi_task comenci a escanejar
        xSemaphoreGive(wifi_scanf_semaphore);

        // Codi alternatiu (comentat) per fer l’escaneig directament des d’aquí:
        // app_wifi_scan((void*)wifi_infos, &list_item_count, 20);
        // (Afegiria els botons directament amb el nom i RSSI de cada xarxa)
    }
}

//---------------------------------------------------------
// Callback del switch ON/OFF Wifi
//---------------------------------------------------------
static void sw_wifi_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);     // Llegeix el tipus d’esdeveniment
    lv_obj_t *obj = lv_event_get_target(e);          // Obté el switch que ha generat l’event

    if (code == LV_EVENT_VALUE_CHANGED)              // Només quan canvia d’estat
    {
        if (lv_obj_has_state(obj, LV_STATE_CHECKED)) // Si el switch està activat
        {
            g_wifi_enable = true;                    // Marca WiFi com actiu
            esp_wifi_port_connect();                 // Connecta el mòdul WiFi
        }
        else                                         // Si el switch està apagat
        {
            g_wifi_enable = false;                   // Desactiva WiFi
            for (int i = 0; i < list_item_count; i++) // Esborra la llista actual
            {
                lv_obj_del(list_btns[i]);
            }
            list_item_count = 0;
            esp_wifi_port_disconnect();              // Desconnecta el mòdul WiFi
        }
    }
}


static void wifi_list_btn_event_handler(lv_event_t *e);
static void wifi_msgbox_event_handler(lv_event_t *e);

//---------------------------------------------------------
// Tasca FreeRTOS que gestiona l'escaneig i la IP
//---------------------------------------------------------
static void lvgl_wifi_task(void *arg)
{
    char str[50] = {0};                  // Buffer per text de la IP
    char str_wifi_ip[32] = {0};          // Buffer IP (només la IP)
    lv_obj_t *label;
    wifi_ap_record_t ap_info[LIST_BTN_LEN_MAX]; // Array per desar resultats de xarxes trobades

    while (1)
    {
        // Espera el semàfor amb timeout de 1 segon
        if (xSemaphoreTake(wifi_scanf_semaphore, pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            printf("wifi_scanf!!\r\n");               // Debug per consola
            memset(ap_info, 0, sizeof(ap_info));      // Neteja dades antigues

            // Fa l’escaneig real (esp_wifi_port_scan omple ap_info i retorna true si tot ok)
            if (esp_wifi_port_scan(ap_info, &list_item_count, LIST_BTN_LEN_MAX))
            {
                lv_obj_del(list_btns[0]);             // Esborra el missatge “Scanning underway”
                if (lvgl_port_lock(0))                // Bloqueja accés a LVGL (thread-safe)
                {
                    for (int i = 0; i < list_item_count && i < LIST_BTN_LEN_MAX; i++)
                    {
                        // Afegeix un botó amb el nom de la xarxa (SSID)
                        list_btns[i] = lv_list_add_btn(list, NULL, (char *)ap_info[i].ssid);
                        lv_obj_add_event_cb(list_btns[i], wifi_list_btn_event_handler, LV_EVENT_CLICKED, NULL);
                        label = lv_label_create(list_btns[i]); // Afegeix etiqueta RSSI dins el botó
                        lv_label_set_text_fmt(label, "%d db", ap_info[i].rssi); // Mostra senyal
                    }
                    lvgl_port_unlock();               // Allibera el lock LVGL
                }
            }
        }

        // Actualitza contínuament la IP cada iteració
        esp_wifi_port_get_ip(str_wifi_ip);            // Obté IP actual
        sprintf(str, "IP: %s", str_wifi_ip);          // Prepara el text per mostrar

        if (lvgl_port_lock(0))                        // Bloqueja LVGL abans de modificar UI
        {
            lv_label_set_text(lable_wifi_ip, str);    // Actualitza etiqueta de la IP
            lvgl_port_unlock();                       // Desbloqueja LVGL
        }
    }
}

//---------------------------------------------------------
// Inicialització de la pantalla Wifi (tile)
//---------------------------------------------------------
void wifi_tile_init(lv_obj_t *parent)
{
    /*Crea la llista principal per mostrar les xarxes*/
    list = lv_list_create(parent);

    /*Crea el semàfor binari per a la sincronització amb la tasca WiFi*/
    wifi_scanf_semaphore = xSemaphoreCreateBinary();

    /*Títol “WiFi” a la part superior*/
    lv_obj_t *lable = lv_label_create(parent);
    lv_obj_set_style_text_font(lable, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_label_set_text(lable, "WiFi");
    lv_obj_align(lable, LV_ALIGN_TOP_MID, 0, 3);

    /*Etiqueta per mostrar la IP actual*/
    lable_wifi_ip = lv_label_create(parent);
    lv_label_set_text(lable_wifi_ip, "IP: 0.0.0.0");
    lv_obj_align(lable_wifi_ip, LV_ALIGN_TOP_MID, 0, 30);

    /*Botó “Scan” per iniciar escaneig*/
    lv_obj_t *btn = lv_btn_create(parent);
    lable = lv_label_create(btn);
    lv_label_set_text(lable, "Scan");
    lv_obj_center(lable);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 20, 5);

    /*Assigna callback d’esdeveniment al botó “Scan”*/
    lv_obj_add_event_cb(btn, btn_wifi_scan_event_handler, LV_EVENT_CLICKED, NULL);

    /*Switch ON/OFF per activar o desactivar WiFi*/
    lv_obj_t *sw = lv_switch_create(parent);
    lv_obj_align(sw, LV_ALIGN_TOP_RIGHT, -20, 10);
    lv_obj_add_event_cb(sw, sw_wifi_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_state(sw, LV_STATE_CHECKED); // Per defecte, activat

    /*Configura mida i posició de la llista*/
    lv_obj_set_size(list, lv_pct(95), lv_pct(85));
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 50);

    /*Crea la tasca FreeRTOS que gestionarà el WiFi*/
    xTaskCreate(lvgl_wifi_task, "lvgl_wifi_task", 1024 * 10, NULL, 0, NULL);
}

static void wifi_list_btn_event_handler(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    const char *ssid = lv_list_get_btn_text(list, btn);
    printf("Selected WiFi: %s\n", ssid);

    static char selected_ssid[33];
    strncpy(selected_ssid, ssid, sizeof(selected_ssid));

    // Crea el popup amb botons
    static const char *btns[] = {"Connect", "Cancel", ""};
    lv_obj_t *msgbox = lv_msgbox_create(NULL, "WiFi", "Introdueix la contrasenya:", btns, false);
    lv_obj_center(msgbox);

    // Crea el textarea per escriure la contrasenya
    lv_obj_t *ta = lv_textarea_create(msgbox);
    lv_obj_set_width(ta, 200);
    lv_textarea_set_password_mode(ta, true);
    lv_textarea_set_placeholder_text(ta, "Password");
    lv_obj_align(ta, LV_ALIGN_CENTER, 0, 10);

    // Focus automàtic al textarea
    lv_group_t *g = lv_group_get_default();
    if (g) lv_group_focus_obj(ta);

    // (Opcional) teclat virtual
    lv_obj_t *kb = lv_keyboard_create(lv_scr_act());
    lv_keyboard_set_textarea(kb, ta);

    // Desa SSID dins user_data del msgbox
    lv_obj_set_user_data(msgbox, (void *)strdup(selected_ssid));

    // Assigna callback de resultats
    lv_obj_add_event_cb(msgbox, wifi_msgbox_event_handler, LV_EVENT_VALUE_CHANGED, ta);
}

static void wifi_msgbox_event_handler(lv_event_t *e)
{
    lv_obj_t *msgbox = lv_event_get_target(e);
    const char *btn_txt = lv_msgbox_get_active_btn_text(msgbox);
    lv_obj_t *ta = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
    const char *ssid = (const char *)lv_obj_get_user_data(msgbox);

    if (strcmp(btn_txt, "Connect") == 0)
    {
        const char *password = lv_textarea_get_text(ta);
        printf("Connecting to %s with password %s\n", ssid, password);

        // (Bloqueja LVGL si cal)
        if (lvgl_port_lock(0))
        {
            lv_label_set_text(lable_wifi_ip, "Connecting...");
            lvgl_port_unlock();
        }

        // Crida la funció del teu port WiFi per connectar
        esp_err_t ret = esp_wifi_connect();
        if (ret == ESP_OK) {
        ESP_LOGI("wifi", "Connexió WiFi iniciada correctament");
        } else {
        ESP_LOGE("wifi", "Error connectant al WiFi: %s", esp_err_to_name(ret));
        }

    }

    // Neteja memòria i tanca msgbox
    free((void *)ssid);
    lv_obj_del(msgbox);
}

