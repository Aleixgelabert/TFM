//---------------------------------------------------------
// FITXER comandament_V4.cpp
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
#include "driver/adc.h"

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

// Calcula la mida del buffer per dibuixar
#define LCD_BUFFER_SIZE EXAMPLE_LCD_H_RES *EXAMPLE_LCD_V_RES / 8

// Defineix el número del port de I2C (pot ser 0 o 1)
#define I2C_PORT_NUM 0


// Etiqueta que surt als missatges de log
static const char *TAG = "COMANDAMENT_V4"; 
static const char *TAG_UDP = "udp_receiver"; 

#define UDP_PORT 3333                    // Port UDP on rebrem els missatges
#define RECV_BUF_SIZE 256                // Mida del buffer per llegir dades rebudes


// Definició dels pins del Joystick i dels botons Confirm i E-stop
#define JOY_X ADC1_CHANNEL_8
#define JOY_Y ADC1_CHANNEL_9
#define BUTTON GPIO_NUM_38
#define ESTOP_GPIO GPIO_NUM_21

#define DEADZONE 100 // Deadzone ajustada per calibració


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

// -----------------------------------------------------------------------------
// CONTROL DE PAQUETS (COM ↔ ESP32)
// -----------------------------------------------------------------------------

// TX COM → RX ESP (el que envia el comandament)
static uint32_t seq_com = 0;
static uint32_t tx_com = 0;

// TX ESP → RX COM (el que rep de l'ESP32)
static uint32_t seq_esp = 0;            // Número de seqüència actual del paquet rebut
static uint32_t last_seq_esp = 0;       // Darrer nñumero de seqüència rebut correctament
static uint32_t rx_esp = 0;             // Comptador de paquets rebuts
static uint32_t lost_esp = 0;           // Comptador de paquets perduts 
static float loss_esp = 0.0f;           // Percentatge de pèrdua de paquets

// Funció per actualitzar estadístiques de paquets rebuts
static void update_rx_esp(uint32_t seq)
{
    rx_esp++;

    if (rx_esp > 1 && seq > last_seq_esp + 1) {         // Detecta la pèrdua de paquets
        lost_esp += (seq - last_seq_esp - 1);           // Acumulador de paquets perduts
    }

    last_seq_esp = seq;     // Actualitzar últim paquet
    seq_esp = seq;          // Actualitzar últim paquet

    uint32_t total = rx_esp + lost_esp;                             // Paquets que hurien d'haver arribat
    loss_esp = (total > 0) ? (100.0f * lost_esp / total) : 0.0f;    // Càlcul del percentatge de pèrdua
}


// -----------------------------------------------------------------------------
// Tasca UDP per rebre dades i mostra el seu contingut per log.
// -----------------------------------------------------------------------------
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

            uint32_t seq = 0;
            float dist = 0.0f;

            sscanf(rx_buffer, "SEQ=%lu", &seq);

            char *p = strstr(rx_buffer, "DIST=");
            if (p) {
                dist = atof(p + 5);
            }

            // actualitzar stats de recepció
            update_rx_esp(seq);

            // Log paquets enviats i rebuts (2 línies)
            ESP_LOGI(TAG,
               "Tx_COM: SEQ_COM=%lu TX_COM=%lu",
                seq_com, tx_com);

            ESP_LOGI(TAG,
                "Rx_ESP: SEQ_ESP=%lu RX_ESP=%lu LOST_ESP=%lu LOSS_ESP=%.2f%%",
                seq, rx_esp, lost_esp, loss_esp);

            // Bloqueig LVGL per actualitzar label de forma segura
            if (lvgl_port_lock(10)) {  // Espera màxim 10 ticks
                if (label_sensor_position)
                {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.1f cm", dist);
                    lv_label_set_text(label_sensor_position, buf);
                }
                lvgl_port_unlock();
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Tasca Joystick (amb calibració, deadzone i mitjana mòbil)
// -----------------------------------------------------------------------------
static void joystick_task(void *arg)
{
    const char *dest_ip = "192.168.4.1";   // IP del receptor
    const int dest_port = 3333;            // Port UDP
    struct sockaddr_in dest_addr;
    char msg[64];

    int Vx_raw, Vy_raw;        // Lectura ADC directa
    int Vx_center, Vy_center;  // Valors de centre del joystick
    int Vx_mapped, Vy_mapped;  // Valors finals mapejats 0..4095

    const int deadzone = 400;  // Zona morta al voltant del centre

    // --- Configuració socket UDP ---
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG_UDP, "joystick_task: no es pot crear socket");
        vTaskDelete(NULL);
        return;
    }

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dest_port);
    dest_addr.sin_addr.s_addr = inet_addr(dest_ip);
    if (dest_addr.sin_addr.s_addr == INADDR_NONE) {
        ESP_LOGW(TAG_UDP, "joystick_task: dest_ip invalida, enviament desactivat");
    }
    
    // --- Calibració del centre ---
    ESP_LOGI("JOYSTICK", "Calibrant joystick, mantingueu-lo al centre...");
    Vx_center = 0;
    Vy_center = 0;
    const int N_CAL = 50; // Nombre de mostres per calcular el centre
    
    for (int i = 0; i < N_CAL; i++) {
        Vx_center += adc1_get_raw(JOY_X);
        Vy_center += adc1_get_raw(JOY_Y);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    Vx_center /= N_CAL;
    Vy_center /= N_CAL;

    ESP_LOGI("JOYSTICK", "Centre calibrat: Vx=%d, Vy=%d", Vx_center, Vy_center);

    // ------  Obtenir extrems del joystick ------
    // Llegim valors màxim i mínim als extrems per mapatge lineal
    const int EXT_SAMPLES = 50;
    int Vx_left=0, Vx_right=0, Vy_up=0, Vy_down=0;

    ESP_LOGI("JOYSTICK", "Calibrant extrems del joystick... mou tot a esquerra, dreta, amunt, avall");

    // Per simplificar, assignem valors teòrics si no es fa calibració física
    // Això es pot substituir per llegir manualment els extrems
    Vx_left = 0;
    Vx_right = 4095;
    Vy_down = 0;
    Vy_up = 4095;


    // --- Bucle principal ---
    while (1)
    {
        bool estop_active = gpio_get_level(ESTOP_GPIO);  // 1 = premut (e-stop actiu)

        system_set_estop(estop_active);

        if (!estop_active) {
            // Llegeix ADC (0..4095)
            Vx_raw = adc1_get_raw(JOY_X);
            Vy_raw = adc1_get_raw(JOY_Y);

            // Restem centre per obtenir desviació
            int Vx_dev = Vx_raw - Vx_center;
            int Vy_dev = Vy_raw - Vy_center;

            // Aplicar deadzone
            if (abs(Vx_dev) < deadzone) Vx_dev = 0;
            if (abs(Vy_dev) < deadzone) Vy_dev = 0;

            // Funció de mapatge lineal
            auto map_range = [](int val, int in_min, int in_max, int out_min, int out_max) {
                if (val <= in_min) return out_min;
                if (val >= in_max) return out_max;
                return (val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
            };

            // Mapegem Vx: esquerra=0, dreta=4095
            if (Vx_dev >= 0)
                Vx_mapped = map_range(Vx_dev, 0, Vx_right - Vx_center, 2048, 4095);
            else
                Vx_mapped = map_range(Vx_dev, Vx_left - Vx_center, 0, 0, 2048);

            // Mapegem Vy: baix=0, amunt=4095
            if (Vy_dev >= 0)
                Vy_mapped = map_range(Vy_dev, 0, Vy_up - Vy_center, 2048, 4095);
            else
                Vy_mapped = map_range(Vy_dev, Vy_down - Vy_center, 0, 0, 2048);

            // Calculem la posició del joystick en percentatge
            int Vx_pct = ((Vx_mapped - 2048) * 100) / 2048; // -100..+100
            int Vy_pct = ((Vy_mapped - 2048) * 100) / 2048; // -100..+100

            // Llegir estat botó
            int btn = gpio_get_level(BUTTON);    // 1 = NO premut, 0 = premut


            // --- Missatge UDP ---
            int n = snprintf(msg,sizeof(msg),
                "SEQ=%lu;VX=%d;VY=%d;BTN=%d;ESTOP=%d",
               seq_com, Vx_mapped, Vy_mapped,
               (btn==0?1:0),
               (estop_active ? 1: 0));

            if (dest_addr.sin_addr.s_addr != INADDR_NONE) {
                sendto(sock,msg,n,0,(struct sockaddr*)&dest_addr,sizeof(dest_addr));

                seq_com++;   // incrementa seq
                tx_com++;    // incrementa enviats
            }
                
            // imprimir per terminal
            ESP_LOGI("JOYSTICK",
                     "VX=%d (%d%%)   VY=%d (%d%%)   BTN=%d   ESTOP=0",
                     Vx_mapped, Vx_pct, Vy_mapped, Vy_pct, (btn==0?1:0));

            // Actualitzar labels de la UI (crida modular a system_tile)
            if (lvgl_port_lock(10)) {
                system_set_joystick_vx(Vx_mapped);
                system_set_joystick_vy(Vy_mapped);
                system_set_joystick_vx_pct(Vx_pct);
                system_set_joystick_vy_pct(Vy_pct);
                system_set_confirm(btn == 0); // 0 = premut → ON, 1 = no premut → OFF
                lvgl_port_unlock();
            }
        } else {
            // --- E-Stop actiu ---
            Vx_mapped = Vy_mapped = 0;
            int btn = 0;

            int n = snprintf(msg,sizeof(msg),"VX=0;VY=0;BTN=0;ESTOP=1");
            if (dest_addr.sin_addr.s_addr != INADDR_NONE)
                sendto(sock,msg,n,0,(struct sockaddr*)&dest_addr,sizeof(dest_addr));

            ESP_LOGI("JOYSTICK","VX=0 (0%%)   VY=0 (0%%)   BTN=0   ESTOP=1");

            // Actualitzar UI
            if(lvgl_port_lock(10)) {
                system_set_joystick_vx(0);
                system_set_joystick_vy(0);
                system_set_joystick_vx_pct(0);
                system_set_joystick_vy_pct(0);
                lvgl_port_unlock();
            }
        }

        // Delay 20ms (50Hz)
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    // (No arribem aquí normalment) tanca socket
    close(sock);
    vTaskDelete(NULL);

}


// -----------------------------------------------------------------------------
// Funció principal app_main
// -----------------------------------------------------------------------------
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

    // --- Inicialitzar ADC per joystick ---
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(JOY_X, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(JOY_Y, ADC_ATTEN_DB_11);

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
        lvgl_ui_init(); // Funció personalitzada (no LVGL Oficial), inicialitza interfície d'usuari (botons, menús, pantalles,...)
        lvgl_port_unlock();
    }
    lvgl_port_lock(0);                 // Bloquejar LVGL
    system_tile_init(lv_scr_act());    // Crear la UI amb el label
    lvgl_port_unlock();                // Desbloquejar LVGL

    // Inicialitzar botó GPIO21 amb pull-up intern
    gpio_config_t estop_conf = {
    .pin_bit_mask = (1ULL << ESTOP_GPIO),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&estop_conf);

    // Inicialitzar botó GPIO38 amb pull-up intern
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,    // Activa pull-up intern
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_conf);

    // -----------------------------------------------------------------------------
    // Tasca Wi-Fi
    // -----------------------------------------------------------------------------
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
    }

    // Quan ja tenim IP, creem la tasca que escoltarà UDP
    xTaskCreate(udp_receive_task,                 // Nom de la funció
                "udp_receive_task",               // Nom de la tasca
                4096,                             // Mida de la pila (bytes)
                NULL,                             // Argument (no el fem servir)
                5,                                // Prioritat (5 = normal)
                NULL);                            // Handle (no necessari)

    xTaskCreate(joystick_task,
            "joystick_task",
            4096,
            NULL,
            5,
            NULL);

}

// -----------------------------------------------------------------------------
// Funcions I2C i expansor
// -----------------------------------------------------------------------------
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

