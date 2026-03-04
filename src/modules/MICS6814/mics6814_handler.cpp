#include "mics6814_handler.h"
#include <math.h> // Para a função pow()

// ===================================================================
// --- Variáveis Globais (Privadas) do Módulo ---
// ===================================================================

// Armazenam os valores de calibração (R0 - resistência em ar limpo)
// Estes são os valores que a mics6814_init() irá configurar.
static int16_t g_r0_co = 0;
static int16_t g_r0_no2 = 0;
static int16_t g_r0_nh3 = 0;


// CORREÇÃO CRÍTICA: 26666 representa 5V no ADS1115 com GAIN_TWOTHIRDS (+/- 6.144V)
// O MICS é alimentado por 5V, então este é o topo real do divisor de tensão.
const float ADC_RAW_5V = 26666.0f;

static float calculate_ratio(int16_t rs_raw, int16_t r0_raw) {
    // Proteção contra zero e leituras de erro negativas do ADS
    if (r0_raw <= 0 || rs_raw <= 0) {
        return 0; 
    }
    
    float rs_f = (float)rs_raw;
    float r0_f = (float)r0_raw;

    // Fórmula corrigida com o topo de 5V real
    float ratio = (rs_f / r0_f) * (ADC_RAW_5V - r0_f) / (ADC_RAW_5V - rs_f);
    
    // Trava de segurança: Ratio não pode ser negativo (evita falha na função pow)
    if (ratio < 0.01f) ratio = 0.01f; 
    
    return ratio;
}


/**
 * @brief Inicializa o handler, armazenando os valores de calibração.
 */
void mics6814_init(int16_t r0_co, int16_t r0_no2, int16_t r0_nh3) {
    // Armazena os valores de calibração (R0) lidos do "ar limpo"
    // para serem usados pela função de leitura.
    g_r0_co = r0_co;
    g_r0_no2 = r0_no2;
    g_r0_nh3 = r0_nh3;
    
    Serial.println("MICS6814: Handler inicializado.");
    Serial.printf("MICS6814: R0 -> CO: %d | NO2: %d | NH3: %d\n", g_r0_co, g_r0_no2, g_r0_nh3);
}

/**
 * @brief CALIBRA o sensor para encontrar os valores de R0.
 */
void mics6814_calibrate(int16_t& out_r0_co, int16_t& out_r0_no2, int16_t& out_r0_nh3) {
    Serial.println("MICS6814: *** INICIANDO CALIBRAÇÃO DE R0 ***");
    Serial.println("MICS6814: AVISO - O MOSFET DEVE estar ligado há pelo menos 3 minutos antes de rodar isso!");
    
    int32_t sum_co = 0, sum_no2 = 0, sum_nh3 = 0;
    int valid_samples = 0;
    const int NUM_SAMPLES = 10;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        int16_t val_co = ads1115_read_robust(ADS_CHANNEL_MICS_CO);
        int16_t val_no2 = ads1115_read_robust(ADS_CHANNEL_MICS_NO2);
        int16_t val_nh3 = ads1115_read_robust(ADS_CHANNEL_MICS_NH3);

        // Só soma se a leitura for válida (maior que 0)
        if (val_co > 0 && val_no2 > 0 && val_nh3 > 0) {
            sum_co += val_co;
            sum_no2 += val_no2;
            sum_nh3 += val_nh3;
            valid_samples++;
            Serial.print(".");
        } else {
            Serial.print("x"); // Erro de leitura do ADS
        }
        delay(1000); 
    }
    Serial.println();

    if (valid_samples > 0) {
        out_r0_co = (int16_t)(sum_co / valid_samples);
        out_r0_no2 = (int16_t)(sum_no2 / valid_samples);
        out_r0_nh3 = (int16_t)(sum_nh3 / valid_samples);
        Serial.println("MICS6814: Calibração Concluída com Sucesso.");
    } else {
        Serial.println("MICS6814: FALHA NA CALIBRAÇÃO (Sem amostras válidas do ADS).");
        out_r0_co = 0; out_r0_no2 = 0; out_r0_nh3 = 0;
    }
}

/**
 * @brief Lê os valores atuais do sensor (Rs) e os converte para PPM.
 */
bool mics6814_read_data(MICS6814_Data &data) {
    data.isValid = false;

    if (g_r0_co <= 0 || g_r0_no2 <= 0 || g_r0_nh3 <= 0) {
        Serial.println("MICS6814: ERRO - Valores R0 inválidos. Calibre o sensor.");
        return false;
    }

    data.raw_co = ads1115_read_robust(ADS_CHANNEL_MICS_CO);
    data.raw_no2 = ads1115_read_robust(ADS_CHANNEL_MICS_NO2);
    data.raw_nh3 = ads1115_read_robust(ADS_CHANNEL_MICS_NH3);

    // Verifica erro do ADS (valor negativo)
    if (data.raw_co <= 0 || data.raw_no2 <= 0 || data.raw_nh3 <= 0) {
        Serial.println("MICS6814: ERRO de leitura no ADS1115 (Valor negativo ou zero).");
        return false;
    }

    float ratio_co = calculate_ratio(data.raw_co, g_r0_co);
    float ratio_no2 = calculate_ratio(data.raw_no2, g_r0_no2);
    float ratio_nh3 = calculate_ratio(data.raw_nh3, g_r0_nh3);

    // Fórmulas exponenciais de conversão para PPM
    data.ppm_co = pow(ratio_co, -1.179f) * 4.385f;
    data.ppm_no2 = pow(ratio_no2, 1.007f) / 6.855f;
    data.ppm_nh3 = pow(ratio_nh3, -1.67f) / 1.47f;
    
    data.isValid = true;
    
    Serial.printf("MICS6814: PPM -> CO: %.2f | NO2: %.2f | NH3: %.2f\n", data.ppm_co, data.ppm_no2, data.ppm_nh3);

    return true;
}