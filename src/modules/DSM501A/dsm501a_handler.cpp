#include "dsm501a_handler.h"

// ===================================================================
// --- Variáveis Globais para as Interrupções (ISR) ---
// Precisamos de 'volatile' para dizer ao compilador que estas
// variáveis podem mudar a qualquer momento (pela interrupção).
// ===================================================================

// Variáveis para o pino PM2.5
static volatile unsigned long g_dsm_pm25_low_start_time_us = 0;
static volatile unsigned long g_dsm_pm25_total_low_time_us = 0;

// Variáveis para o pino PM10
static volatile unsigned long g_dsm_pm10_low_start_time_us = 0;
static volatile unsigned long g_dsm_pm10_total_low_time_us = 0;


// ===================================================================
// --- Funções ISR (Interrupt Service Routines) ---
// Estas são as funções "mágicas" chamadas pelo hardware.
// Elas devem ser o mais RÁPIDAS possível. Não use Serial.print aqui.
// ===================================================================

/**
 * @brief ISR para o pino PM2.5. Chamada em CADA mudança de borda (HIGH/LOW).
 */
void IRAM_ATTR dsm_pm25_isr() {
    // 1. Lê o estado ATUAL do pino
    bool pin_state_is_low = (digitalRead(DSM501A_PM25_PIN) == LOW);

    if (pin_state_is_low) {
        // Borda de SUBIDA para BAIXO (HIGH -> LOW)
        // O pulso LOW acabou de começar. Salve a hora atual.
        g_dsm_pm25_low_start_time_us = micros();
    } else {
        // Borda de SUBIDA (LOW -> HIGH)
        // O pulso LOW acabou de terminar.
        // Se já tínhamos um tempo de início, calcule a duração e some ao total.
        if (g_dsm_pm25_low_start_time_us > 0) {
            g_dsm_pm25_total_low_time_us += (micros() - g_dsm_pm25_low_start_time_us);
            g_dsm_pm25_low_start_time_us = 0; // Reseta para a próxima
        }
    }
}

/**
 * @brief ISR para o pino PM10.
 */
void IRAM_ATTR dsm_pm10_isr() {
    bool pin_state_is_low = (digitalRead(DSM501A_PM10_PIN) == LOW);
    if (pin_state_is_low) {
        g_dsm_pm10_low_start_time_us = micros();
    } else {
        if (g_dsm_pm10_low_start_time_us > 0) {
            g_dsm_pm10_total_low_time_us += (micros() - g_dsm_pm10_low_start_time_us);
            g_dsm_pm10_low_start_time_us = 0;
        }
    }
}


// ===================================================================
// --- Funções Públicas ---
// ===================================================================

/**
 * @brief Inicializa os pinos GPIO para leitura do(s) sensor(es) DSM501A.
 */
void dsm501a_init() {
    // Configura os pinos como INPUT (com pull-up interno desabilitado,
    // já que temos nosso divisor de tensão externo que age como pull-up)
    pinMode(DSM501A_PM25_PIN, INPUT);
    pinMode(DSM501A_PM10_PIN, INPUT);
    
    Serial.printf("DSM501A: Pino PM2.5 (GPIO %d) configurado como ENTRADA.\n", DSM501A_PM25_PIN);
    Serial.printf("DSM501A: Pino PM10 (GPIO %d) configurado como ENTRADA.\n", DSM501A_PM10_PIN);
    Serial.println("DSM501A: Handler (Interrupt-based) inicializado.");
    Serial.println("DSM501A: AVISO - Sensor requer 1 minuto de aquecimento (warm-up) após ligar.");
}

/**
 * @brief Realiza a leitura dos dados do sensor DSM501A usando interrupções.
 */
bool dsm501a_read_data(DSM501A_Data &data, unsigned long sample_time_ms) {
    data.isValid = false; 

    // O datasheet do DSM501A recomenda 30 segundos de amostragem.
    if (sample_time_ms < 10000) { // Mínimo de 10s
        Serial.printf("DSM501A: AVISO - Tempo de amostragem (%lu ms) é muito baixo. Recomendado: 30000 ms.\n", sample_time_ms);
    }
    if (sample_time_ms == 0) return false;

    Serial.printf("DSM501A: Iniciando amostragem por %lu ms (usando interrupções)...\n", sample_time_ms);

    // 1. Zera os contadores globais
    g_dsm_pm25_total_low_time_us = 0;
    g_dsm_pm25_low_start_time_us = 0;
    g_dsm_pm10_total_low_time_us = 0;
    g_dsm_pm10_low_start_time_us = 0;

    // 2. Anexa as interrupções (liga os "ouvidos")
    //    digitalPinToInterrupt() é a forma correta de mapear GPIO 19 -> ID da Interrupção
    //    CHANGE = Dispara a ISR em CADA mudança (subida ou descida)
    attachInterrupt(digitalPinToInterrupt(DSM501A_PM25_PIN), dsm_pm25_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(DSM501A_PM10_PIN), dsm_pm10_isr, CHANGE);

    // 3. Dorme (bloqueia) pelo tempo de amostragem
    //    Enquanto o 'delay' roda, as ISRs 'dsm_pm25_isr' e 'dsm_pm10_isr'
    //    estão rodando em segundo plano, contando os pulsos.
    delay(sample_time_ms);

    // 4. Desanexa as interrupções (desliga os "ouvidos")
    detachInterrupt(digitalPinToInterrupt(DSM501A_PM25_PIN));
    detachInterrupt(digitalPinToInterrupt(DSM501A_PM10_PIN));

    Serial.println("DSM501A: Amostragem concluída. Calculando LOP Ratio...");

    // 5. Calcula os resultados
    float sample_time_us = (float)(sample_time_ms * 1000.0f);

    // PM2.5
    data.low_pulse_occupancy_ratio_pm25 = ((float)g_dsm_pm25_total_low_time_us / sample_time_us) * 100.0f;
    Serial.printf("DSM501A: PM2.5 Tempo total em BAIXO: %lu us\n", (unsigned long)g_dsm_pm25_total_low_time_us);
    Serial.printf("DSM501A: PM2.5 LOP Ratio: %.2f %%\n", data.low_pulse_occupancy_ratio_pm25);

    // PM10
    data.low_pulse_occupancy_ratio_pm10 = ((float)g_dsm_pm10_total_low_time_us / sample_time_us) * 100.0f;
    Serial.printf("DSM501A: PM10 Tempo total em BAIXO: %lu us\n", (unsigned long)g_dsm_pm10_total_low_time_us);
    Serial.printf("DSM501A: PM10 LOP Ratio: %.2f %%\n", data.low_pulse_occupancy_ratio_pm10);

    // Se qualquer leitura for > 0, consideramos o sensor válido
    if (g_dsm_pm25_total_low_time_us > 0 || g_dsm_pm10_total_low_time_us > 0) {
        data.isValid = true; 
    } else {
        Serial.println("DSM501A: AVISO - Leituras de pulso ainda são 0 us. Verifique o warm-up e a fiação.");
    }

    return true; 
}