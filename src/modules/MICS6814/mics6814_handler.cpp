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

const float ADC_MAX_VALUE = 32767.0f;

/**
 * @brief (Função Privada) Calcula o "ratio" (Rs/R0)
 *
 * Esta é a fórmula de "ratio" que encontramos no driver ESP-IDF,
 * mas corrigida para usar o valor de 16 bits (32767.0) 
 * do nosso ADS1115.
 *
 * @param rs_raw O valor ATUAL (bruto) lido do ADC.
 * @param r0_raw O valor de CALIBRAÇÃO (bruto) lido em ar limpo.
 * @return O ratio (float) para ser usado nas fórmulas de PPM.
 */
static float calculate_ratio(int16_t rs_raw, int16_t r0_raw) {
    if (r0_raw == 0 || rs_raw == 0) {
        return 0; // Evita divisão por zero
    }
    
    float rs_f = (float)rs_raw;
    float r0_f = (float)r0_raw;

    float ratio = (rs_f / r0_f) * (ADC_MAX_VALUE - r0_f) / (ADC_MAX_VALUE - rs_f);
    
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
    Serial.println("MICS6814: AVISO - Sensor requer 1-3 minutos de aquecimento (warm-up) após ligar.");
    Serial.printf("MICS6814: R0 (CO)  carregado: %d\n", g_r0_co);
    Serial.printf("MICS6814: R0 (NO2) carregado: %d\n", g_r0_no2);
    Serial.printf("MICS6814: R0 (NH3) carregado: %d\n", g_r0_nh3);
}

/**
 * @brief CALIBRA o sensor para encontrar os valores de R0.
 */
void mics6814_calibrate(int16_t& out_r0_co, int16_t& out_r0_no2, int16_t& out_r0_nh3) {
    Serial.println("MICS6814: *** INICIANDO CALIBRAÇÃO DE R0 ***");
    Serial.println("MICS6814: Certifique-se de que o sensor está em AR LIMPO.");
    Serial.println("MICS6814: Aguardando 10 segundos para estabilização da média...");

    int32_t sum_co = 0;
    int32_t sum_no2 = 0;
    int32_t sum_nh3 = 0;
    const int NUM_SAMPLES = 10;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        // Usamos nossa função de "oversampling" do ads1115_handler
        // para obter a leitura mais estável possível.
        sum_co += (int32_t)ads1115_read_stable_raw_value(ADS_CHANNEL_MICS_CO);
        sum_no2 += (int32_t)ads1115_read_stable_raw_value(ADS_CHANNEL_MICS_NO2);
        sum_nh3 += (int32_t)ads1115_read_stable_raw_value(ADS_CHANNEL_MICS_NH3);
        delay(1000); // Espera 1 segundo entre as médias
    }

    // Tira a média final
    out_r0_co = (int16_t)(sum_co / NUM_SAMPLES);
    out_r0_no2 = (int16_t)(sum_no2 / NUM_SAMPLES);
    out_r0_nh3 = (int16_t)(sum_nh3 / NUM_SAMPLES);

    Serial.println("MICS6814: *** CALIBRAÇÃO CONCLUÍDA ***");
    Serial.printf("MICS6814: R0 (CO)  calculado: %d\n", out_r0_co);
    Serial.printf("MICS6814: R0 (NO2) calculado: %d\n", out_r0_no2);
    Serial.printf("MICS6814: R0 (NH3) calculado: %d\n", out_r0_nh3);
    Serial.println("MICS6814: Copie estes valores e passe-os para a função mics6814_init().");
}

/**
 * @brief Lê os valores atuais do sensor (Rs) e os converte para PPM.
 */
bool mics6814_read_data(MICS6814_Data &data) {
    data.isValid = false;

    // 1. Verifica se a calibração foi carregada
    if (g_r0_co == 0 || g_r0_no2 == 0 || g_r0_nh3 == 0) {
        Serial.println("MICS6814: ERRO - Valores de R0 (calibração) são 0. Chame mics6814_init() primeiro.");
        return false;
    }

    // 2. Lê os valores ATUAIS (Rs - Resistência do Sensor)
    //    Usando nossa função de leitura estável
    data.raw_co = ads1115_read_stable_raw_value(ADS_CHANNEL_MICS_CO);
    data.raw_no2 = ads1115_read_stable_raw_value(ADS_CHANNEL_MICS_NO2);
    data.raw_nh3 = ads1115_read_stable_raw_value(ADS_CHANNEL_MICS_NH3);

    // 3. Calcula os Ratios (Rs/R0)
    float ratio_co = calculate_ratio(data.raw_co, g_r0_co);
    float ratio_no2 = calculate_ratio(data.raw_no2, g_r0_no2);
    float ratio_nh3 = calculate_ratio(data.raw_nh3, g_r0_nh3);

    // 4. Converte os Ratios para PPM (Fórmulas do driver ESP-IDF)
    //    As fórmulas podem precisar de ajuste fino, mas são um 
    //    excelente ponto de partida.
    
    // CO (Monóxido de Carbono) - 1 a 1000 ppm
    data.ppm_co = pow(ratio_co, -1.179f) * 4.385f;

    // NO2 (Dióxido de Nitrogênio) - 0.05 a 10 ppm
    data.ppm_no2 = pow(ratio_no2, 1.007f) / 6.855f;

    // NH3 (Amônia) - 1 a 500 ppm
    data.ppm_nh3 = pow(ratio_nh3, -1.67f) / 1.47f;
    
    data.isValid = true;
    
    // Log para depuração
    Serial.printf("MICS6814: Leituras Brutas (Rs): CO=%d, NO2=%d, NH3=%d\n", data.raw_co, data.raw_no2, data.raw_nh3);
    Serial.printf("MICS6814: Ratios (Rs/R0): CO=%.2f, NO2=%.2f, NH3=%.2f\n", ratio_co, ratio_no2, ratio_nh3);
    Serial.printf("MICS6814: PPM Calculados: CO=%.2f, NO2=%.2f, NH3=%.2f\n", data.ppm_co, data.ppm_no2, data.ppm_nh3);

    return true;
}