#include "ads1115_handler.h"
#include <Wire.h>     
#include <Adafruit_ADS1X15.h>
#include <vector>
#include <algorithm>

#define BATTERY_CORRECTION_FACTOR  1.0033f 

Adafruit_ADS1115 ads; 

// Configuração de Ganho: GAIN_TWOTHIRDS (+/- 6.144V)
// Necessário para ler Bateria (4.2V) e Sensores 5V sem saturar.
const adsGain_t ADC_GAIN = GAIN_TWOTHIRDS;


// Fator de Conversão: 6.144V / 32767 = 0.0001875 V/bit
const float VOLTAGE_MULTIPLIER = 0.0001875f;

bool _check_ads_connection(uint8_t addr) {
    Wire.beginTransmission(addr);
    uint8_t error = Wire.endTransmission();
    if (error == 0) return true; // Sucesso (ACK)
    
    // Se error != 0 (NACK, Timeout, etc)
    Serial.printf("[ADS] Check falhou. Wire.endTransmission = %d\n", error);
    return false;
}

// --- FUNÇÃO AUXILIAR DE RESET DO BARRAMENTO (Privada) ---
void _reset_i2c_bus() {
    Serial.println("[ADS] ! Erro I2C detectado. Resetando barramento...");
    Wire.end();
    delay(50);
    Wire.begin();
    Wire.setClock(10000);   // Clock conservador 10kHz
    Wire.setTimeOut(3000);  // Timeout longo
    delay(100);
}

bool ads1115_init(uint8_t i2c_address) {
    Serial.printf("[ADS] Iniciando no endereço 0x%X...\n", i2c_address);
    
    if (!_check_ads_connection(i2c_address)) {
            Serial.println("[ADS] Sensor não detectado no teste preliminar.");
            _reset_i2c_bus();
        }

        if (!ads.begin(i2c_address, &Wire)) {
            Serial.println("[ADS] Falha no ads.begin().");
            return false;
        }

        ads.setGain(ADC_GAIN);
        ads.setDataRate(RATE_ADS1115_860SPS);
        return true;
}

float ads1115_read_median_voltage(int channel) {
    const int NUM_SAMPLES = 11; 
    const uint8_t ADS_ADDR = 0x48;
    
    if (!_check_ads_connection(ADS_ADDR)) {
        Serial.println("[ADS] Erro: Sensor desconectado antes da leitura.");
        _reset_i2c_bus();
        return -1.0;
    }

    std::vector<float> samples;
    
    for (int i = 0; i < NUM_SAMPLES; i++) {
        int16_t raw = ads.readADC_SingleEnded(channel);
        
        // Converte para tensão imediatamente para facilitar debug
        // Assumindo ganho padrão +/- 4.096V (1 bit = 0.125mV) ou +/- 6.144V (1 bit = 0.1875mV)
        // Verifique o Ganho configurado no seu setup!
        // Exemplo para GAIN_ONE (+/- 4.096V): multiplier = 0.000125
        float sample_volts = ads1115_convert_to_voltage(raw); 

        if (sample_volts > 0.5) { 
            samples.push_back(sample_volts);
        }
        delay(2); 
    }

    if (samples.empty()) return -1.0; // Falha total

    // 2. Ordena o vetor (Bubble sort ou std::sort)
    std::sort(samples.begin(), samples.end());

    // 3. Pega o valor do meio (Mediana)
    int middle_index = samples.size() / 2;
    float median_value = samples[middle_index];

    return median_value;
}

float ads1115_convert_to_voltage(int16_t raw_adc_value) {
    return (float)raw_adc_value * VOLTAGE_MULTIPLIER;
}

float ads1115_read_battery_voltage() {
    float voltage = ads1115_read_median_voltage(ADS_CHANNEL_BATTERY);
    
    if (voltage < 0) return 0.0f; // Erro

    return voltage * BATTERY_CORRECTION_FACTOR;
}

int16_t ads1115_read_robust(ads_channel_t channel) { // ou ads_channel_t se for enum
    const int NUM_SAMPLES = 11; 
    std::vector<int16_t> samples;
    
    // Coleta 11 amostras rápidas
    for (int i = 0; i < NUM_SAMPLES; i++) {
        int16_t val = ads.readADC_SingleEnded(channel);
        if (val > 0) { // Ignora falhas bizarras de leitura
            samples.push_back(val);
        }
        delay(2);
    }

    if (samples.empty()) {
        return -1; // Retorna erro se falhar todas
    }

    // Ordena e pega a mediana (ignora picos de ruído)
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];
}