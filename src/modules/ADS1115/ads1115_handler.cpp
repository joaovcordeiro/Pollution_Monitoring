#include "ads1115_handler.h"
#include <Wire.h>     
#include <Adafruit_ADS1X15.h>

Adafruit_ADS1115 ads; 


// O seu módulo MICS é alimentado por 5V. As saídas analógicas
// dele também podem chegar a 5V.
// O ADS1115 também está em 5V. Portanto, precisamos de um ganho (PGA)
// que possa ler com segurança até 5V (ou mais).
// GAIN_TWOTHIRDS cobre +/- 6.144V. É a escolha correta.
const adsGain_t ADC_GAIN = GAIN_TWOTHIRDS;

// Esta é a "constante mágica" para converter o valor RAW em Volts.
// É o V_max (6.144V) dividido pelo valor RAW máximo (32767).
// (6.144 / 32767.0 = 0.0001875)
const float VOLTAGE_MULTIPLIER_16BIT_6V = 0.0001875f;

/**
 * @brief Inicializa o sensor ADS1115. (Função pública do .h)
 */
bool ads1115_init(uint8_t i2c_address) {
    Serial.printf("ADS1115: Tentando inicializar no endereço I2C 0x%X...\n", i2c_address);
    
    // Passamos o ponteiro &Wire para a biblioteca
    if (!ads.begin(i2c_address, &Wire)) {
        Serial.println("ADS1115: FALHA CRÍTICA - Não foi possível encontrar o ADC.");
        return false;
    }

    // Define o ganho que escolhemos
    ads.setGain(ADC_GAIN);
    
    // Opcional: Aumentar a taxa de dados. 860 amostras por segundo
    // torna nossa leitura de 32 amostras (próxima função) mais rápida.
    ads.setDataRate(RATE_ADS1115_860SPS);

    Serial.printf("ADS1115: Inicialização bem-sucedida no endereço 0x%X.\n", i2c_address);
    Serial.printf("ADS1115: Ganho de hardware definido para: 2/3 (FS +/-6.144V)\n");
    return true;
}

/**
 * @brief Lê um canal analógico de forma robusta. (Função pública do .h)
 */
int16_t ads1115_read_stable_raw_value(ads_channel_t channel) {
    
    // Esta é a lógica de "oversampling" que encontramos no driver profissional.
    // Ela filtra ruídos elétricos (ex: do aquecedor do MICS).
    const int NUM_SAMPLES = 32;
    
    // Usamos int32_t para o acumulador para evitar "estouro" (overflow)
    int32_t adc_sum = 0; 

    // 1. Lê o ADC 32 vezes o mais rápido possível
    for (int i = 0; i < NUM_SAMPLES; i++) {
        // A biblioteca da Adafruit mapeia 0, 1, 2, 3 para os canais
        adc_sum += (int32_t)ads.readADC_SingleEnded(channel);
    }

    // 2. Retorna a média.
    //    (adc_sum >> 5) é uma forma muito rápida (bit-shift) 
    //    de fazer (adc_sum / 32).
    return (int16_t)(adc_sum >> 5);
}

/**
 * @brief Converte um valor RAW para Volts. (Função pública do .h)
 */
float ads1115_convert_to_voltage(int16_t raw_adc_value) {
    // Apenas para fins de log e publicação, se necessário.
    // O módulo MICS6814 usará o valor RAW.
    return (float)raw_adc_value * VOLTAGE_MULTIPLIER_16BIT_6V;
}