#include "mics6814_handler.h"
#include <math.h> // Para a função pow()

// Valor do resistor de carga (RL) no seu módulo em kOhm.
const float MICS_RL_VALUE_KOHM = 10.0; 

// Constantes da curva de sensibilidade (PPM = a * (Rs/R0)^b)
// Estes valores são baseados em exemplos de datasheet e podem precisar de ajuste fino.
const float CO_CURVE_A = 605.18;  const float CO_CURVE_B = -3.937;
const float NO2_CURVE_A = 1.1;     const float NO2_CURVE_B = 1.007;
const float NH3_CURVE_A = 102.2;   const float NH3_CURVE_B = -2.473;

// Função para calcular Rs (em kOhm) a partir da tensão medida
static float calculate_rs_kohm(float v_out) {
    const float VCC = 5.0; // Tensão de alimentação do sensor
    if (v_out <= 0.01 || v_out >= VCC) { // Evita divisão por zero ou valores inválidos
        return 0; 
    }
    return ((VCC * MICS_RL_VALUE_KOHM) / v_out) - MICS_RL_VALUE_KOHM;
}

// Implementação da nova função de cálculo de PPM
void mics6814_calculate_ppm(MICS6814_Data &data) {
    // Zera os valores de PPM antes de calcular
    data.ppm_co = 0.0f;
    data.ppm_no2 = 0.0f;
    data.ppm_nh3 = 0.0f;

    if (!data.isValid) {
        Serial.println(F("MICS6814: Dados de tensão inválidos, não é possível calcular PPM."));
        return;
    }

    float rs_co = calculate_rs_kohm(data.voltage_co);
    float rs_no2 = calculate_rs_kohm(data.voltage_no2);
    float rs_nh3 = calculate_rs_kohm(data.voltage_nh3);

    Serial.printf("MICS6814: Rs calculados (kOhm) -> CO: %.2f, NO2: %.2f, NH3: %.2f\n", rs_co, rs_no2, rs_nh3);
    Serial.printf("MICS6814: R0 de referência (kOhm) -> CO: %.2f, NO2: %.2f, NH3: %.2f\n", MICS_CO_R0, MICS_NO2_R0, MICS_NH3_R0);

    // Calcula PPM para CO se os valores forem válidos
    if (rs_co > 0 && MICS_CO_R0 > 0) {
        float ratio_co = rs_co / MICS_CO_R0;
        data.ppm_co = CO_CURVE_A * pow(ratio_co, CO_CURVE_B);
    }

    // Calcula PPM para NO2. A curva para gases oxidantes é diferente.
    if (rs_no2 > 0 && MICS_NO2_R0 > 0) {
        float ratio_no2 = rs_no2 / MICS_NO2_R0;
        data.ppm_no2 = NO2_CURVE_A * pow(ratio_no2, NO2_CURVE_B);
    }

    // Calcula PPM para NH3
    if (rs_nh3 > 0 && MICS_NH3_R0 > 0) {
        float ratio_nh3 = rs_nh3 / MICS_NH3_R0;
        data.ppm_nh3 = NH3_CURVE_A * pow(ratio_nh3, NH3_CURVE_B);
    }

    Serial.printf("MICS6814: PPM calculados -> CO: %.2f, NO2: %.2f, NH3: %.2f\n", data.ppm_co, data.ppm_no2, data.ppm_nh3);
}


static adsGain_t mics_adc_gain = GAIN_TWOTHIRDS; 

bool mics6814_init() {
    Serial.println("MICS6814: Handler initialized. (Pré-aquecimento deve ser garantido externamente)");
    return true;
}

bool mics6814_read_voltages(MICS6814_Data &data) {
    data.isValid = false; 

    Serial.println("MICS6814: Lendo tensões...");

    // Ler CO (Canal 0 do ADS1115)
    int16_t adc_co_raw = ads1115_read_adc_single_ended(MICS_CO_CHANNEL);
    if (adc_co_raw == INT16_MIN) { 
        Serial.println("MICS6814: Falha ao ler canal CO do ADS1115.");
        return false; 
    }
    data.voltage_co = ads1115_compute_voltage(adc_co_raw, mics_adc_gain);
    Serial.printf("MICS6814: CO Raw ADC: %d, Voltage: %.4f V\n", adc_co_raw, data.voltage_co);

    // Ler NO2 (Canal 1 do ADS1115)
    int16_t adc_no2_raw = ads1115_read_adc_single_ended(MICS_NO2_CHANNEL);
    if (adc_no2_raw == INT16_MIN) {
        Serial.println("MICS6814: Falha ao ler canal NO2 do ADS1115.");
        return false;
    }
    data.voltage_no2 = ads1115_compute_voltage(adc_no2_raw, mics_adc_gain);
    Serial.printf("MICS6814: NO2 Raw ADC: %d, Voltage: %.4f V\n", adc_no2_raw, data.voltage_no2);

    // Ler NH3 (Canal 2 do ADS1115)
    int16_t adc_nh3_raw = ads1115_read_adc_single_ended(MICS_NH3_CHANNEL);
    if (adc_nh3_raw == INT16_MIN) {
        Serial.println("MICS6814: Falha ao ler canal NH3 do ADS1115.");
        return false;
    }
    data.voltage_nh3 = ads1115_compute_voltage(adc_nh3_raw, mics_adc_gain);
    Serial.printf("MICS6814: NH3 Raw ADC: %d, Voltage: %.4f V\n", adc_nh3_raw, data.voltage_nh3);

    data.isValid = true; // Todas as leituras (ou as que não falharam) foram feitas
    return true;
}