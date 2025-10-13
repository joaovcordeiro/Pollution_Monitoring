#ifndef DSM501A_HANDLER_H
#define DSM501A_HANDLER_H

#include <Arduino.h>
#include "config.h"

const unsigned long DEFAULT_DSM501A_SAMPLE_TIME_MS = 300; //Ajuste posterior

// Estrutura para armazenar os dados lidos do DSM501A
struct DSM501A_Data {
    float low_pulse_occupancy_ratio_pm25; // LOP ratio para PM2.5 em percentagem (%)
    float low_pulse_occupancy_ratio_pm10; // Descomente se for ler PM10
    
    // float pm25_concentration;
    // float pm10_concentration;
    
    bool isValid; 
};

/**
 * @brief Inicializa os pinos GPIO para leitura do(s) sensor(es) DSM501A.
 * Configura os pinos definidos como ENTRADA.
 */
void dsm501a_init();

/**
 * @brief Realiza a leitura dos dados do sensor DSM501A.
 * Mede a "Low Pulse Occupancy" (LOP) ratio durante um período de amostragem.
 * @param data Referência para a estrutura DSM501A_Data onde os dados serão armazenados.
 * @param sample_time_ms O tempo total de amostragem em milissegundos (padrão: DEFAULT_DSM501A_SAMPLE_TIME_MS).
 * @return true se a leitura (cálculo da LOP ratio) for bem-sucedida, false caso contrário.
 */
bool dsm501a_read_data(DSM501A_Data &data, unsigned long sample_time_ms = DEFAULT_DSM501A_SAMPLE_TIME_MS);

#endif // DSM501A_HANDLER_H