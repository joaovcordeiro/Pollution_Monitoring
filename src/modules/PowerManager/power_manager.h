#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>     
#include "esp_sleep.h"   

/**
 * @brief Configura o pino de controle de energia dos sensores.
 * Deve ser chamado uma vez no setup principal.
 */
void setup_sensor_power();

/**
 * @brief Liga a alimentação dos sensores através do MOSFET.
 * Inclui um delay para a estabilização dos sensores.
 * O tempo de delay deve ser ajustado conforme o sensor mais lento para estabilizar.
 */
void power_sensors_on();

/**
 * @brief Desliga a alimentação dos sensores através do MOSFET.
 */
void power_sensors_off();

/**
 * @brief Configura o ESP32 para entrar em Deep Sleep por um período definido.
 * O período é configurado pela constante TIME_TO_SLEEP_INTERVAL_MINUTES em config.h (ou similar).
 * O ESP32 reiniciará após o período de sleep.
 */
void enter_deep_sleep();

#endif // POWER_MANAGER_H