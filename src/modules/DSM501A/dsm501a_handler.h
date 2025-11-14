#ifndef DSM501A_HANDLER_H
#define DSM501A_HANDLER_H

#include <Arduino.h>
#include "config.h"

// O datasheet recomenda uma amostragem de 30 segundos (30.000 ms)
// para leituras estáveis. 300ms é muito rápido e pode dar 0.
const unsigned long DEFAULT_DSM501A_SAMPLE_TIME_MS = 30000; // 30 segundos

// Estrutura para armazenar os dados lidos do DSM501A
struct DSM501A_Data {
    float low_pulse_occupancy_ratio_pm25; // LOP ratio para PM2.5 (%)
    float low_pulse_occupancy_ratio_pm10; // LOP ratio para PM10 (%)
    bool isValid; 
};

/**
 * @brief Inicializa os pinos GPIO para leitura do(s) sensor(es) DSM501A.
 */
void dsm501a_init();

/**
 * @brief Realiza a leitura dos dados do sensor DSM501A usando interrupções.
 * Esta função é BLOQUEANTE e irá pausar pelo 'sample_time_ms'.
 * * @param data Referência para a estrutura DSM501A_Data onde os dados serão armazenados.
 * @param sample_time_ms O tempo total de amostragem (Recomendado: 30000 ms).
 * @return true se a leitura foi bem-sucedida, false caso contrário.
 */
bool dsm501a_read_data(DSM501A_Data &data, unsigned long sample_time_ms = DEFAULT_DSM501A_SAMPLE_TIME_MS);

#endif // DSM501A_HANDLER_H