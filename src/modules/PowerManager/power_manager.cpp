#include "power_manager.h"
#include "config.h" 
#include <esp_adc_cal.h>

// Definição do pino de leitura da bateria (Hardware LilyGo T-SIM7000G)
#define BATTERY_ADC_PIN 35

// Fatores de conversão para o tempo de sleep
#define uS_TO_S_FACTOR 1000000ULL
#define MINUTES_TO_uS_FACTOR (60ULL * uS_TO_S_FACTOR)

esp_adc_cal_characteristics_t *adc_chars;

void setup_sensor_power() {
    pinMode(SENSOR_POWER_CTRL_PIN, OUTPUT);
    // Garante que os sensores comecem desligados
    // Para MOSFET Canal N (low-side), NÍVEL BAIXO desliga
    digitalWrite(SENSOR_POWER_CTRL_PIN, LOW); 

    //Configura o pino do ADC
    pinMode(BATTERY_ADC_PIN, INPUT);

    // Aloca memória para a estrutura de calibração
    adc_chars = (esp_adc_cal_characteristics_t*) calloc(1, sizeof(esp_adc_cal_characteristics_t));

    // Lê o eFuse do ESP32 para configurar a curva de correção correta.
    // Usamos atenuação de 11dB para ler até 3.3V (embora o pino receba ~2V).
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, adc_chars);
    
    // Configura a atenuação no hardware
    analogSetAttenuation(ADC_11db);
}

void power_sensors_on() {
    if (Serial) {
        Serial.println("PowerManager: Powering sensors ON...");
    }
    // Para MOSFET Canal N (low-side), NÍVEL ALTO liga
    digitalWrite(SENSOR_POWER_CTRL_PIN, HIGH);
    
    if (Serial) {
        Serial.println("PowerManager: Sensors powered ON.");
    }
}

void power_sensors_off() {
    if (Serial) {
        Serial.println("PowerManager: Powering sensors OFF...");
    }
    // Para MOSFET Canal N (low-side), NÍVEL BAIXO desliga
    digitalWrite(SENSOR_POWER_CTRL_PIN, LOW);
    delay(100); // Pequeno delay para garantir o corte total
    if (Serial) {
        Serial.println("PowerManager: Sensors OFF.");
    }
}

/**
 * @brief Mapeia voltagem para porcentagem (Curva Li-Ion aproximada)
 */
float voltage_to_percentage(float voltage) {
    if (voltage >= 4.15) return 100.0;
    if (voltage >= 4.05) return 90.0;
    if (voltage >= 3.95) return 80.0;
    if (voltage >= 3.85) return 70.0;
    if (voltage >= 3.75) return 60.0;
    if (voltage >= 3.70) return 50.0; // Tensão nominal
    if (voltage >= 3.65) return 40.0;
    if (voltage >= 3.60) return 30.0;
    if (voltage >= 3.50) return 15.0;
    if (voltage >= 3.40) return 5.0;
    return 0.0;
}

void get_battery_status(Battery_Data &data) {
    // 1. LEITURAS DE DESCARTE (Para vencer a impedância de 100k)
    // Isso continua sendo necessário para carregar o capacitor interno
    for (int i = 0; i < 10; i++) {
        analogRead(BATTERY_ADC_PIN); 
        delay(2);
    }

    // 2. LEITURA COM OVERSAMPLING
    uint32_t raw_sum = 0;
    const int SAMPLES = 20;

    for (int i = 0; i < SAMPLES; i++) {
        raw_sum += analogRead(BATTERY_ADC_PIN);
        delay(5); 
    }
    uint16_t raw_average = raw_sum / SAMPLES;

    // 3. CÁLCULO AUTOMÁTICO (Sem números mágicos)
    // Esta função da biblioteca usa a calibração de fábrica para converter
    // o RAW em milivolts REAIS no pino 35.
    uint32_t voltage_at_pin_mv = esp_adc_cal_raw_to_voltage(raw_average, adc_chars);
    
    // Como a placa tem um divisor de tensão físico de 2:1 (100k/100k),
    // a bateria é exatamente o dobro do que chega no pino.
    float voltage_batt = (float)voltage_at_pin_mv * 2.0f / 1000.0f;

    data.voltage = voltage_batt;

    // Log para conferência
    if (Serial) {
        Serial.printf("PowerManager: RAW=%d | Pino=%.0fmV | Bateria=%.2fV\n", 
                      raw_average, (float)voltage_at_pin_mv, data.voltage);
    }

    // 4. Lógica USB vs Bateria
    if (data.voltage < 1.0) {
        data.is_usb_connected = true;
        data.percentage = 0.0; 
        data.days_remaining = 0.0;
        if (Serial) Serial.println("PowerManager: Tensão < 1V. Assumindo USB.");
    } else {
        data.is_usb_connected = false;
        data.percentage = voltage_to_percentage(data.voltage);
        
        float remaining_capacity_mah = BATTERY_CAPACITY_MAH * (data.percentage / 100.0f);
        float daily_consumption_mah = SYSTEM_AVG_CONSUMPTION_MA * 24.0f;
        
        if (daily_consumption_mah > 0) {
            data.days_remaining = remaining_capacity_mah / daily_consumption_mah;
        } else {
            data.days_remaining = 0.0;
        }
    }
}

void enter_deep_sleep() {
    uint64_t sleep_time_us = TIME_TO_SLEEP_INTERVAL_MINUTES * MINUTES_TO_uS_FACTOR;

    if (Serial) {
        Serial.printf("PowerManager: Entering Deep Sleep for %llu seconds (%d minutes)...\n", sleep_time_us / uS_TO_S_FACTOR, TIME_TO_SLEEP_INTERVAL_MINUTES);
        Serial.flush(); // Garante que a mensagem serial seja enviada antes de dormir
    }
    
    // Configura a fonte de despertar (Timer)
    esp_sleep_enable_timer_wakeup(sleep_time_us);
    
    // Opcional: Adicionar mais configurações de sleep se necessário, como desligar domínios RTC
    // esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
    // etc.

    // Inicia o Deep Sleep
    esp_deep_sleep_start();
    
    // O código abaixo desta linha não será executado, pois o ESP32 reinicia.
}