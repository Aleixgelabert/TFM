#include "lvgl_ui.h"              // Inclou el fitxer d'encapçalament principal de la interfície gràfica
#include "tileview/system_tile.h" // Inclou la definició i inicialització del "system tile"
/*
#include "tileview/qmi8658_tile.h"   // Inclou el tile relacionat amb el sensor QMI8658 (acceleròmetre/giroscopi)
#include "tileview/rgb_tile.h"       // Inclou el tile amb controls o visualitzacions RGB (colors o llums)
#include "tileview/image_tile.h"     // Inclou el tile destinat a mostrar imatges
#include "tileview/camera_tile.h"    // Inclou el tile amb la interfície de càmera
#include "tileview/axp2101_tile.h"   // Inclou el tile per gestionar el xip d’alimentació AXP2101
#include "tileview/wifi_tile.h"      // Inclou el tile encarregat de la configuració o estat del Wi-Fi
*/

/**
 * @brief Inicialitza la interfície gràfica principal amb diverses pantalles (tiles)
 * 
 * Aquesta funció crea un objecte `lv_tileview` (una vista amb múltiples pantalles lliscables)
 * i hi afegeix diversos "tiles" (pantalles individuals) per a diferents funcions del sistema.
 */
void lvgl_ui_init(void)
{
    // Crea la vista principal (tileview) sobre la pantalla activa
    lv_obj_t *tileview = lv_tileview_create(lv_scr_act());

    // Fa transparent el fons de la barra de desplaçament en estat per defecte
    lv_obj_set_style_bg_opa(tileview, LV_OPA_0, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);

    // Fa transparent el fons de la barra de desplaçament quan està desplaçada
    lv_obj_set_style_bg_opa(tileview, LV_OPA_0, LV_PART_SCROLLBAR | LV_STATE_SCROLLED);


    /*==============================
     *   TILE 1: Pantalla inicial
     *==============================*/

    /*
    // Afegeix un tile per a la interfície RGB a la posició (0, 0)
    // Es pot lliscar cap a la dreta per anar al següent tile
    lv_obj_t *rgb_tile = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_RIGHT);
    rgb_tile_init(rgb_tile);  // Inicialitza el contingut del tile RGB

    // Afegeix un tile del sistema a la posició (1, 0)
    // Es pot lliscar tant a l’esquerra com a la dreta
    lv_obj_t *system_tile = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    system_tile_init(system_tile);   // Inicialitza el tile del sistema
    
    // Afegeix el tile per gestionar el xip AXP2101 (alimentació)
    // També es pot desplaçar a esquerra/dreta
    lv_obj_t *axp2101_tile = lv_tileview_add_tile(tileview, 2, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    axp2101_tile_init(axp2101_tile); // Inicialitza el tile d’energia
    */

    // (Versió actual simplificada) Crea només el tile del sistema com a primer tile
    lv_obj_t *system_tile = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_RIGHT);
    system_tile_init(system_tile);   // Inicialitza la interfície del sistema
    

    /*==============================
     *   TILE 2: Altres funcions
     *==============================*/

    /*
    // Tile per al sensor QMI8658 (moviment/orientació)
    lv_obj_t *qmi8658_tile = lv_tileview_add_tile(tileview, 3, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    qmi8658_tile_init(qmi8658_tile);  // Inicialitza el contingut del tile QMI8658

    // Tile per la càmera — permet veure la imatge capturada
    lv_obj_t *camera_tile = lv_tileview_add_tile(tileview, 4, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    camera_tile_init(camera_tile);    // Inicialitza la interfície de càmera

    // Tile per al Wi-Fi — permet configurar o mostrar l’estat de la connexió
    lv_obj_t *wifi_tile = lv_tileview_add_tile(tileview, 5, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    wifi_tile_init(wifi_tile);        // Inicialitza la interfície Wi-Fi
    */
    
    /*
    // (Versió actual) Afegeix només el tile del Wi-Fi com a segona pantalla
    // Es pot lliscar cap a l’esquerra des del "system tile" per accedir-hi
    lv_obj_t *wifi_tile = lv_tileview_add_tile(tileview, 2, 0, LV_DIR_LEFT);
    wifi_tile_init(wifi_tile);  // Inicialitza el tile del Wi-Fi
    */
}
