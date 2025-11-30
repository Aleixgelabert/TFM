//---------------------------------------------------------
// FITXER exemple_03.cpp DE PROVES PER CANVIAR DADES PANTALLA
//---------------------------------------------------------
#include <stdio.h>  // Standard Input/Output Header defineix funcions i macros amb l'entrada i sortida standard
#include <string.h>  // Per treballar amb cadenes (strncpy, strlen, etc.)
#include <sys/param.h>  // Utilitats del sistema (per exemple, MIN, MAX)
#include "nvs_flash.h"  // Inicialitzar memòria no volàtil

// Tasques, delays i semàfors
#include "freertos/FreeRTOS.h"  // FreeRTOS bàsic
#include "freertos/task.h"      // Tasques de FreeRTOS
#include "freertos/semphr.h"    // Semàfors de FreeRTOS

// Drivers perifèrics
#include "driver/gpio.h"
#include "driver/i2c_master.h"

// Driver (controlador) TCA9554, expansor de input/output que comunica normalment per I2C
#include "esp_io_expander_tca9554.h"

// Llibreria gràfica per a sistemes embeguts, relacionada amb LVGL
#include "demos/lv_demos.h"

// Light and Versatile Grahics Library
#include "lvgl.h"
#include "lvgl_ui.h"

// Utilitats internes ESP, defineix macros per verificar errors i simplificar el maneig de retorns
#include "esp_check.h"

// Logging amb ESP_LOGI/W/E
#include "esp_log.h"

// Funcions generals del sistema ESP (reiniciar, etc.)
#include "esp_system.h"

// Defineix la interficie principal per crear, configurar i manejar botons.
#include "iot_button.h"

// Conté l'implementació específica per botons que usen GPIO. (iot_button.h depèn d'aquest arxiu directament)
#include "button_gpio.h"

#include "lwip/sockets.h" // API per crear i utilitzar sockets (UDP/TCP)
#include "lwip/netdb.h"   // Funcions per a resolució de noms i adreces IP

// Ports/abstraccions per pantalla, càmera, audio, PMU, etc.
// Són API específiques del kit
#include "esp_lvgl_port.h"
#include "esp_axp2101_port.h"
#include "esp_pcf85063_port.h"
#include "esp_qmi8658_port.h"
#include "esp_wifi_port.h"
#include "esp_3inch5_lcd_port.h"

#include "wifi_manager.h"   // El nostre gestor Wi-Fi personalitzat (fitxer separat)
#include "tileview/system_tile.h"    // Configuració personal de la pantalla


// Definició pins I2C
#define EXAMPLE_PIN_I2C_SDA GPIO_NUM_8
#define EXAMPLE_PIN_I2C_SCL GPIO_NUM_7

// Definicó pin botó BOOT
#define EXAMPLE_PIN_BUTTON GPIO_NUM_0

// Controla la rotació de la pantalla
#define EXAMPLE_DISPLAY_ROTATION 270

// Resolució de la pantalla segons la rotació
#if EXAMPLE_DISPLAY_ROTATION == 90 || EXAMPLE_DISPLAY_ROTATION == 270
#define EXAMPLE_LCD_H_RES 480
#define EXAMPLE_LCD_V_RES 320
#else
#define EXAMPLE_LCD_H_RES 320
#define EXAMPLE_LCD_V_RES 480
#endif

// Calcula la mida del buffer que usaràs per dibuixar
#define LCD_BUFFER_SIZE EXAMPLE_LCD_H_RES *EXAMPLE_LCD_V_RES / 8

// Defineix el número del port de I2C (pot ser 0 o 1)
#define I2C_PORT_NUM 0


static const char *TAG_UDP = "udp_receiver"; // Etiqueta que surt als missatges de log

#define UDP_PORT 3333                    // Port UDP on rebrem els missatges
#define RECV_BUF_SIZE 256                // Mida del buffer per llegir dades rebudes


// Variables per a bus I2C, controladors de pantalla, tàctils, expansor, LVGL
static const char *TAG_LVGL = "lvgl_system";
i2c_master_bus_handle_t i2c_bus_handle;
esp_lcd_panel_io_handle_t io_handle = NULL;
esp_lcd_panel_handle_t panel_handle = NULL;
esp_io_expander_handle_t expander_handle = NULL;
esp_lcd_touch_handle_t touch_handle = NULL;
lv_disp_drv_t disp_drv;
lv_display_t *lvgl_disp = NULL;
lv_indev_t *lvgl_touch_indev = NULL;
bool touch_test_done = false;


// Declaració de funcions definides més endavant
void i2c_bus_init(void);
void io_expander_init(void);
void lv_port_init(void);


/**
 * Tasca FreeRTOS que escolta paquets UDP i mostra el seu contingut per log.
 */
static void udp_receive_task(void *arg)
{
    (void)arg;
    struct sockaddr_in server_addr, source_addr;
    socklen_t socklen = sizeof(source_addr);
    char rx_buffer[RECV_BUF_SIZE];

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) { vTaskDelete(NULL); return; }

    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_PORT);
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sock); vTaskDelete(NULL); return;
    }

    while (1) {
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
                           (struct sockaddr *)&source_addr, &socklen);

        if (len > 0) {
            rx_buffer[len] = '\0';

            // Bloqueig LVGL per actualitzar label de forma segura
            if (lvgl_port_lock(10)) {  // Espera màxim 10 ticks
                lv_label_set_text_fmt(label_cylinder_position, "%.5s %%", rx_buffer);
                // lv_label_set_text(label_cylinder_position, rx_buffer);
                lvgl_port_unlock();
            }
        }
    }
}


// Funció principal
extern "C" void app_main(void)
{
   // Inicialització de NVS (memòria no volàtil)
    // Si detecta incompatibilitat de NVS esborra i reinicialitza
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);


    // Inicialització del bus I2C i del TCA9554 (expansor I/O)
    i2c_bus_init();
    io_expander_init();

    // Inicialització de la pantalla i del tàctil
    // Aquests *_port_init són funcions d'ajuda del fabricant per encendre
    // i inicialitzar el panell (SPI) i el controlador tàctil (I2C).
    esp_3inch5_display_port_init(&io_handle, &panel_handle, LCD_BUFFER_SIZE);
    esp_3inch5_touch_port_init(&touch_handle, i2c_bus_handle, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, EXAMPLE_DISPLAY_ROTATION);
   
    esp_axp2101_port_init(i2c_bus_handle);      // Inicialització del PMU (Power Management Unit) AXP2101 i altres perifèrics I2C
    vTaskDelay(pdMS_TO_TICKS(100));             // Important: sovint cal un petit delay perquè els dispositius s’engeguin.
    // esp_qmi8658_port_init(i2c_bus_handle);   // IMU (Inertial Mesurement Unit) (QMI8658)*/
    esp_pcf85063_port_init(i2c_bus_handle);     // RTC (Real-Time Clock) (PCF85063)... Tots mitjançant l’I2C. 
    
   
    // Inicialització del control de brillantor i el port d'integració LVGL
    esp_3inch5_brightness_port_init();
    esp_3inch5_brightness_port_set(80);
    lv_port_init();
    

    // Inicia la UI LVGL si pots agafar el lock.
    // lvgl_port_lock(0) demana exclusivitat per manipular LVGL
    // El bloqueig protegeix la pantalla mentre crees la teva interfície
    // i després la desbloqueges perquè funcioni normalment
    if (lvgl_port_lock(0))
    {
        // lv_demo_benchmark();
        // lv_demo_music();
        // lv_demo_widgets();
        lvgl_ui_init(); // Funció personalitzada (no LVGL Oficial), inicialitza interfície d'usuari (botons, menús, pantalles,...)
        lvgl_port_unlock();
    }
    lvgl_port_lock(0);                 // Bloquejar LVGL
    system_tile_init(lv_scr_act());    // Crear la UI amb el label
    lvgl_port_unlock();                // Desbloquejar LVGL


    /*
 * Tasca Wi-Fi.
 */

    ESP_LOGI(TAG_UDP, "Iniciant receptor UDP...");    // Missatge inicial al log

    // Credencials del Wi-Fi del primer ESP32 (AP)
    const char *ssid = "WIFI_ESP32";              // Nom del Wi-Fi creat pel primer ESP32
    const char *pass = "12345678";                // Contrasenya

    // Inicialitza i comença la connexió Wi-Fi (mode STA)
    if (wifi_init_sta(ssid, pass) != ESP_OK) {    // Si falla la inicialització
        ESP_LOGE(TAG_UDP, "wifi_init_sta ha fallat");
     //   return;                                   // Sortim del programa
    }

    // Esperem fins a tenir IP (màxim 10 segons)
    const TickType_t wait_ticks = pdMS_TO_TICKS(10000);  // 10000 ms = 10 segons
    if (!wifi_wait_connected(wait_ticks)) {       // Si no tenim IP després de 10s
        ESP_LOGE(TAG_UDP, "No s'ha obtingut IP en %d ms", 10000);
       // return;                                   // Sortim o podríem reiniciar
    }

    // Quan ja tenim IP, creem la tasca que escoltarà UDP
    xTaskCreate(udp_receive_task,                 // Nom de la funció
                "udp_receive_task",               // Nom de la tasca
                4096,                             // Mida de la pila (bytes)
                NULL,                             // Argument (no el fem servir)
                5,                                // Prioritat (5 = normal)
                NULL);                            // Handle (no necessari)

}

// Configuració I2C
void i2c_bus_init(void)
{
    i2c_master_bus_config_t i2c_mst_config = {};
    i2c_mst_config.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_mst_config.i2c_port = (i2c_port_num_t)I2C_PORT_NUM;
    i2c_mst_config.scl_io_num = EXAMPLE_PIN_I2C_SCL;
    i2c_mst_config.sda_io_num = EXAMPLE_PIN_I2C_SDA;
    i2c_mst_config.glitch_ignore_cnt = 7;               // Debouncing/hardening de línia (evita falsos esclats)
    i2c_mst_config.flags.enable_internal_pullup = 1;    // Activa resistències pull-up internes. Accepta pull-up externes si cal, (si hi ha problemes amb SD o Touch comprova si calen pull-ups externes, mols perifèrics I2C funcionen millor amb pullúp externes)
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_bus_handle));
}

// Configuració TCA9554 (IO Expander)
// Crea l’objecte TCA9554 i configura el pin 1 com a sortida;
// fa un toggle (0 → 1) amb retards. Sovint s’usa per reiniciar
// o habilitar algun alimentador/level-shifter.
void io_expander_init(void)
{
    ESP_ERROR_CHECK(esp_io_expander_new_i2c_tca9554(i2c_bus_handle, ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000, &expander_handle));
    ESP_ERROR_CHECK(esp_io_expander_set_dir(expander_handle,  IO_EXPANDER_PIN_NUM_1, IO_EXPANDER_OUTPUT));
    ESP_ERROR_CHECK(esp_io_expander_set_level(expander_handle, IO_EXPANDER_PIN_NUM_1, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(esp_io_expander_set_level(expander_handle, IO_EXPANDER_PIN_NUM_1, 1));
    vTaskDelay(pdMS_TO_TICKS(100));
}

// Afegir pantalla i tàctil a LVGL
// Aquesta estructura diu a LVGL “aquesta és la meva pantalla, així està connectada, així serà el buffer, i així cal dibuixar el que vulgui mostrar”
void lv_port_init(void)
{
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&port_cfg);
    ESP_LOGI(TAG_LVGL, "Adding LCD screen");
    lvgl_port_display_cfg_t display_cfg = {     // Configuració de display_cfg
        .io_handle = io_handle,                 // Control dels pins del pantalla
        .panel_handle = panel_handle,           // Handle de pantalla LCD/TFT, per dibuixar i enviar dades
        .control_handle = NULL,                 // Opcional per un controlador addicional (no s'utilitza)
        .buffer_size = LCD_BUFFER_SIZE,         // Mida del buffer de memòria per envir dades a la pantalla
        .double_buffer = true,                  // Activa doble buffer per evitar parpellejos en actualitzacions.
        .trans_size = 0,                        // Si és 0, LVGL usa tot el buffer per transmissió (mida per transferència).
        .hres = EXAMPLE_LCD_H_RES,              // Resolució horitzontal de la pantalla.
        .vres = EXAMPLE_LCD_V_RES,              // Resolució vertical de la pantalla.
        .monochrome = false,                    // Indica que la pantalla és a color (no monocrom).
        .rotation = {                           // Configuració de la rotació
            .swap_xy = 0,                       // Si cal intercanviar eixos X i Y.
            .mirror_x = 1,                      // Si cal girar l’eix X (reflectir).
            .mirror_y = 0,                      // Si cal girar l’eix Y.
        },
        .flags = {                              // Flags addicionals
            .buff_dma = 0,                      // Utilitza DMA per transferir el buffer (0 = no).
            .buff_spiram = 1,                   // Usa SPI RAM externa per emmagatzemar el buffer (1 = sí), situa buffers a PSRAM per estalviar DRAM.
            .sw_rotate = 1,                     // Fa la rotació per software (1 = sí), la rotació per software pot consumir CPU.
            .full_refresh = 0,                  // Actualitzar tota la pantalla cada cop (0 = només zones canviades).
            .direct_mode = 0,                   // Mode directe, sense buffers (0 = no).
        },
    };

#if EXAMPLE_DISPLAY_ROTATION == 90
    display_cfg.rotation.swap_xy = 1;
    display_cfg.rotation.mirror_x = 1;
    display_cfg.rotation.mirror_y = 1;
#elif EXAMPLE_DISPLAY_ROTATION == 180
    display_cfg.rotation.swap_xy = 0;
    display_cfg.rotation.mirror_x = 0;
    display_cfg.rotation.mirror_y = 1;

#elif EXAMPLE_DISPLAY_ROTATION == 270
    display_cfg.rotation.swap_xy = 1;
    display_cfg.rotation.mirror_x = 0;
    display_cfg.rotation.mirror_y = 0;
#endif

    lvgl_disp = lvgl_port_add_disp(&display_cfg);   // Afegir la pantalla
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = lvgl_disp,
        .handle = touch_handle,
    };
    lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg); // Afegir el dispositu tàctil

}

