#ifndef SPS30_HANDLER_H
#define SPS30_HANDLER_H

#include <Arduino.h>
#include <SensirionI2cSps30.h> 

// Definição de callback para checar bateria durante a operação
using PowerOkFn = bool (*)();

struct SPS30_Data {
    // Concentrações de Massa (µg/m³)
    float pm1_0;
    float pm2_5;
    float pm4_0;
    float pm10_0;
    float typical_particle_size; 
    bool isValid; 
};

struct SPS30_ReadConfig {
    uint8_t target_samples = 30;          // Quantas leituras válidas queremos
    uint16_t max_seconds = 45;            // Timeout total (em segundos)
    uint8_t min_valid_samples = 20;       // Mínimo aceitável
    bool stop_measurement_after_read = true; // OBRIGATÓRIO true para bateria
    PowerOkFn power_ok = nullptr;         // Função para abortar se bateria cair
};

/**
 * @brief Inicializa o SPS30, verifica conexão e coloca em DEEP SLEEP.
 * @return true se o sensor foi detectado.
 */
bool sps30_init();

/**
 * @brief Ciclo completo: Acorda -> Liga Fan -> Mede -> Para Fan -> Dorme.
 * @return true se obteve sucesso.
 */
bool sps30_read_measurements(SPS30_Data &data, const SPS30_ReadConfig &cfg);

// Funções auxiliares públicas para controle manual se necessário
void sps30_force_sleep();
void sps30_force_wakeup();

#endif // SPS30_HANDLER_H