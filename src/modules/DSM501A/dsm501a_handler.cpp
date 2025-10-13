#include "dsm501a_handler.h"

// Variáveis para o cálculo da LOP
static unsigned long low_pulse_start_time_pm25 = 0;
static unsigned long total_low_pulse_duration_pm25 = 0;
static bool in_low_pulse_pm25 = false;

// Descomente se for usar PM10
// static unsigned long low_pulse_start_time_pm10 = 0;
// static unsigned long total_low_pulse_duration_pm10 = 0;
// static bool in_low_pulse_pm10 = false;

void dsm501a_init() {
    pinMode(DSM501A_PM25_PIN, INPUT);
    Serial.printf("DSM501A: Pino PM2.5 (GPIO %d) configurado como ENTRADA.\n", DSM501A_PM25_PIN);

    pinMode(DSM501A_PM10_PIN, INPUT);
    Serial.printf("DSM501A: Pino PM10 (GPIO %d) configurado como ENTRADA.\n", DSM501A_PM10_PIN);
    
    Serial.println("DSM501A: Handler inicializado. ");
}

/**
 * @brief Mede a duração total dos pulsos baixos em um pino específico durante o tempo de amostragem.
 * Esta função é bloqueante durante o sample_time_ms.
 * @param pin O pino GPIO a ser lido.
 * @param sample_time_ms O tempo total de amostragem em milissegundos.
 * @return A duração total em microssegundos em que o pino esteve em nível BAIXO.
 */
static unsigned long measure_total_low_pulse_duration(uint8_t pin, unsigned long sample_time_ms) {
    unsigned long total_low_time_us = 0;
    unsigned long sample_start_time_ms = millis();
    unsigned long pulse_timeout_us = 2000000; 

    Serial.printf("DSM501A: Iniciando amostragem no pino %d por %lu ms...\n", pin, sample_time_ms);

    while (millis() - sample_start_time_ms < sample_time_ms) {
        // Mede a duração do próximo pulso BAIXO.
        unsigned long pulse_duration_us = pulseIn(pin, LOW, pulse_timeout_us);
        
        if (pulse_duration_us > 0) {
            total_low_time_us += pulse_duration_us;
        }
    }
    Serial.printf("DSM501A: Amostragem no pino %d concluída. Tempo total em BAIXO: %lu us.\n", pin, total_low_time_us);
    return total_low_time_us;
}

bool dsm501a_read_data(DSM501A_Data &data, unsigned long sample_time_ms) {
    data.isValid = false; // Assume inválido inicialmente

    // --- Leitura para PM2.5 ---
    unsigned long total_low_duration_us_pm25 = measure_total_low_pulse_duration(DSM501A_PM25_PIN, sample_time_ms);
    
    // Calcula a LOP (Low Pulse Occupancy) ratio em porcentagem
    // LOP = (tempo_total_em_baixo_us / tempo_total_amostragem_us) * 100
    if (sample_time_ms > 0) {
        data.low_pulse_occupancy_ratio_pm25 = ((float)total_low_duration_us_pm25 / (float)(sample_time_ms * 1000.0f)) * 100.0f;
    } else {
        data.low_pulse_occupancy_ratio_pm25 = 0.0f; // Evita divisão por zero
    }
    Serial.printf("DSM501A: PM2.5 LOP Ratio: %.2f %%\n", data.low_pulse_occupancy_ratio_pm25);

    // --- Leitura para PM10 (Descomente e adapte se estiver usando) ---
    unsigned long total_low_duration_us_pm10 = measure_total_low_pulse_duration(DSM501A_PM10_PIN, sample_time_ms);
    if (sample_time_ms > 0) {
        data.low_pulse_occupancy_ratio_pm10 = ((float)total_low_duration_us_pm10 / (float)(sample_time_ms * 1000.0f)) * 100.0f;
    } else {
        data.low_pulse_occupancy_ratio_pm10 = 0.0f;
    }
    Serial.printf("DSM501A: PM10 LOP Ratio: %.2f %%\n", data.low_pulse_occupancy_ratio_pm10);

    data.isValid = true; // Marca como válido após as leituras
    return true;
}
