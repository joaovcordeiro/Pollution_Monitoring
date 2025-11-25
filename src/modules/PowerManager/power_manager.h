#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>     

// --- Configurações da Bateria ---
#define BATTERY_CAPACITY_MAH 2200.0 
#define SYSTEM_AVG_CONSUMPTION_MA 15.0

struct Battery_Data {
    float voltage;          // Tensão (Volts)
    float percentage;       // Carga Estimada (%)
    float days_remaining;   // Previsão de duração (Dias)
    bool is_usb_connected;  // Indica se está ligado no USB
};

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

void get_battery_status(Battery_Data &data);

/**
 * @brief Configura o ESP32 para entrar em Deep Sleep por um período definido.
 * O período é configurado pela constante TIME_TO_SLEEP_INTERVAL_MINUTES em config.h (ou similar).
 * O ESP32 reiniciará após o período de sleep.
 */
void enter_deep_sleep();

#endif // POWER_MANAGER_H