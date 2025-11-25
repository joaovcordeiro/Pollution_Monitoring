#ifndef SPS30_HANDLER_H
#define SPS30_HANDLER_H

#include <Arduino.h>
#include <SensirionI2cSps30.h> 

struct SPS30_Data {
    // Concentrações de Massa (µg/m³)
    float pm1_0;
    float pm2_5;
    float pm4_0;
    float pm10_0;

    // Outros dados
    float typical_particle_size; 
    
    bool isValid; 
};

/**
 * @brief Inicializa o sensor SPS30 (laser particle sensor).
 * * Tenta estabelecer comunicação com o sensor e inicia o modo de medição periódica.
 * @return true se a inicialização for bem-sucedida, false caso contrário.
 */
bool sps30_init();

/**
 * @brief Realiza a leitura dos dados do sensor SPS30.
 * * Faz polling para garantir que os dados de PM estão disponíveis (sensor estabilizado).
 * @param data Referência para a estrutura SPS30_Data onde os dados serão armazenados.
 * @return true se novos dados foram lidos com sucesso, false caso contrário.
 */
bool sps30_read_measurements(SPS30_Data &data);

#endif // SPS30_HANDLER_H