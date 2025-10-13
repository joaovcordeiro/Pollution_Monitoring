#include "ads1115_handler.h"
#include <Wire.h>     
#include <limits.h>   

Adafruit_ADS1115 ads; 
// static adsGain_t current_ads_gain_setting = GAIN_TWOTHIRDS; // Para rastrear o que está configurado no hardware

bool ads1115_init(uint8_t i2c_address) {
    Serial.printf("ADS1115: Attempting to initialize at I2C address 0x%02X...\n", i2c_address);
    
    // Configura um ganho padrão inicial. Este é o ganho que o hardware usará para as leituras.
    ads.setGain(GAIN_TWOTHIRDS); 
    // current_ads_gain_setting = GAIN_TWOTHIRDS;

    if (!ads.begin(i2c_address)) {
        Serial.printf("ADS1115: Failed to initialize. Check wiring and I2C address 0x%02X.\n", i2c_address);
        return false;
    }
    
    Serial.printf("ADS1115: Initialization successful at address 0x%02X.\n", i2c_address);
    Serial.print("ADS1115: Initial Hardware Gain Set To: ");
    switch (ads.getGain()) { 
        case GAIN_TWOTHIRDS: Serial.println("2/3 (FS +/-6.144V)"); break;
        case GAIN_ONE:       Serial.println("1   (FS +/-4.096V)"); break;
        // ... (outros casos de ganho)
        default:             Serial.println("Unknown"); break;
    }
    return true;
}

void ads1115_set_gain(adsGain_t new_gain) {
    // Esta função configura o GANHO NO HARDWARE (objeto ads)
    Serial.print("ADS1115: Setting hardware gain to: ");
    switch (new_gain) {
        case GAIN_TWOTHIRDS: Serial.println("2/3"); break;
        case GAIN_ONE:       Serial.println("1"); break;
        // ... (outros casos)
        default:             Serial.println("Unknown Gain Value"); break;
    }
    ads.setGain(new_gain);
    // current_ads_gain_setting = new_gain;
}

int16_t ads1115_read_adc_single_ended(uint8_t channel) {
    // Esta função lê o ADC usando o ganho que está ATUALMENTE CONFIGURADO no objeto 'ads'
    if (channel > 3) {
        Serial.printf("ADS1115: Error - Invalid channel %d requested.\n", channel);
        return INT16_MIN; 
    }
    return ads.readADC_SingleEnded(channel);
}

/**
 * @brief Converte a leitura bruta do ADC para tensão (em Volts).
 * Esta função calcula a tensão baseada no valor ADC e no ganho ESPECIFICADO
 * que FOI USADO durante a aquisição daquele valor ADC.
 * Ela NÃO depende do ganho atualmente configurado no objeto 'ads' para o cálculo.
 */
float ads1115_compute_voltage(int16_t adc_value, adsGain_t gain_used_for_reading) {
    float fs_voltage; // Tensão de fundo de escala (Full-Scale) para o ganho especificado

    switch (gain_used_for_reading) {
        case GAIN_TWOTHIRDS: // +/- 6.144V
            fs_voltage = 6.144f;
            break;
        case GAIN_ONE:       // +/- 4.096V
            fs_voltage = 4.096f;
            break;
        case GAIN_TWO:       // +/- 2.048V
            fs_voltage = 2.048f;
            break;
        case GAIN_FOUR:      // +/- 1.024V
            fs_voltage = 1.024f;
            break;
        case GAIN_EIGHT:     // +/- 0.512V
            fs_voltage = 0.512f;
            break;
        case GAIN_SIXTEEN:   // +/- 0.256V
            fs_voltage = 0.256f;
            break;
        default:
            Serial.println("ADS1115: Error - Unknown gain for voltage computation.");
            return 0.0f; // Ou NAN
    }

    // O ADS1115 é um ADC de 16 bits. Para leituras single-ended, os valores positivos
    // vão de 0 a 32767, correspondendo de 0V à tensão de fundo de escala positiva (fs_voltage).
    // Voltagem = (valor_adc_bruto / 32767.0) * fs_voltage_positiva
    // No caso da biblioteca Adafruit, eles usam fs_voltage como a faixa total (+/-),
    // então a conversão é counts * (fs_voltage_para_o_pga / 32767.0)
    // Para single-ended, a faixa efetiva é de 0 a +fs_voltage (se a entrada for positiva).
    // A função ads.computeVolts() faz isso, mas ela usa o m_gain interno.
    // Reimplementando a lógica:
    return (float)adc_value * (fs_voltage / 32767.0f);
}
