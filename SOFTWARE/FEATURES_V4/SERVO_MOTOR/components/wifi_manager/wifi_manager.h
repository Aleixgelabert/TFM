#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

/**
 * Inicia la WiFi en mode STA i intenta connectar a (ssid, pass).
 * Retorna ESP_OK si l'inicialització ha començat correctament.
 *
 * Nota: la connexió és asíncrona; pots esperar amb wifi_wait_connected().
 */
esp_err_t wifi_init_sta(const char *ssid, const char *pass);

/**
 * Espera fins que la WiFi obtingui IP o fins que 'ticks_to_wait' passi.
 * Retorna true si està connectat (té IP), false si expira.
 */
bool wifi_wait_connected(TickType_t ticks_to_wait);

/**
 * Para la WiFi i desregistra handlers.
 */
void wifi_stop(void);

#endif // WIFI_MANAGER_H
