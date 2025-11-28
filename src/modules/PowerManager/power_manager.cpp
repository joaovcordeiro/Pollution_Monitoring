#include "power_manager.h"
#include "config.h" 
#include <esp_adc_cal.h>
#include <driver/adc.h>

// Fatores de conversão para o tempo de sleep
#define uS_TO_S_FACTOR 1000000ULL
#define MINUTES_TO_uS_FACTOR (60ULL * uS_TO_S_FACTOR)

esp_adc_cal_characteristics_t *adc_chars = nullptr;

// Filtro de média móvel
static float voltage_history[MOVING_AVERAGE_SAMPLES] = {0};
static int history_index = 0;
static bool history_initialized = false;

// Calibração manual (ajustar após medição com multímetro)
// Ex: Se multímetro lê 3.87V mas ESP32 lê 3.75V, use offset = +0.12
static float calibration_offset = +0.054f;

/**
 * @brief Converte tensão para porcentagem usando curva não-linear 
 * 
 * @param voltage Tensão da bateria em Volts
 * @return Porcentagem de carga (0-100%)
 */
float voltage_to_percentage(float voltage) {
    // Proteção contra valores absurdos
    if (voltage < 2.5f) return 0.0f;
    if (voltage >= BATTERY_FULL_VOLTAGE) return 100.0f;
    
    const float curve[][2] = {
        {4.20, 100.0}, // Carga completa
        {4.15, 95.0},
        {4.11, 90.0},
        {4.08, 85.0},
        {4.02, 80.0},
        {3.98, 75.0},
        {3.95, 70.0},
        {3.91, 65.0},
        {3.87, 60.0},
        {3.85, 55.0},
        {3.84, 50.0},  // Tensão nominal
        {3.82, 45.0},
        {3.80, 40.0},
        {3.79, 35.0},
        {3.77, 30.0},
        {3.75, 25.0},
        {3.73, 20.0},
        {3.71, 15.0},
        {3.69, 10.0},  // Zona crítica
        {3.61, 5.0},
        {3.50, 2.0},
        {3.40, 1.0},
        {3.00, 0.0}    // Cutoff mínimo
    };
    
    const int num_points = sizeof(curve) / sizeof(curve[0]);
    
    // Interpolação linear entre pontos adjacentes
    for (int i = 0; i < num_points - 1; i++) {
        if (voltage >= curve[i + 1][0] && voltage <= curve[i][0]) {
            float v_range = curve[i][0] - curve[i + 1][0];
            float p_range = curve[i][1] - curve[i + 1][1];
            float v_offset = voltage - curve[i + 1][0];
            
            return curve[i + 1][1] + (v_offset / v_range) * p_range;
        }
    }
    
    return 0.0f; // Abaixo do mínimo
}

/**
 * @brief Aplica filtro de média móvel para suavizar leituras
 * Reduz oscilações causadas por ruído ou variações de carga
 * 
 * @param new_value Nova leitura de tensão
 * @return Tensão filtrada (média das últimas N leituras)
 */
float apply_moving_average(float new_value) {
    voltage_history[history_index] = new_value;
    history_index = (history_index + 1) % MOVING_AVERAGE_SAMPLES;
    
    // Primeira execução: preenche todo o buffer com o valor atual
    if (!history_initialized) {
        for (int i = 0; i < MOVING_AVERAGE_SAMPLES; i++) {
            voltage_history[i] = new_value;
        }
        history_initialized = true;
        return new_value;
    }
    
    // Calcula média de todas as amostras
    float sum = 0;
    for (int i = 0; i < MOVING_AVERAGE_SAMPLES; i++) {
        sum += voltage_history[i];
    }
    
    return sum / MOVING_AVERAGE_SAMPLES;
}

/**
 * @brief Lê tensão da bateria com oversampling e calibração
 * Otimizado para economia de energia (delays em microssegundos)
 * 
 * @return Tensão da bateria em Volts
 */
float read_battery_voltage_raw() {
    // 1. Leituras de descarte (reduzidas para 5 iterações)
    // Necessário para carregar o capacitor interno do ADC através da alta impedância
    for (int i = 0; i < 5; i++) {
        adc1_get_raw(BATTERY_ADC_CHANNEL);
        delayMicroseconds(100);  // 100µs (20x mais rápido que delay(2))
    }
    
    // 2. Oversampling com 32 amostras
    // √32 = 5.66x de redução de ruído (melhor que as 20 amostras anteriores)
    uint32_t raw_sum = 0;
    const int SAMPLES = 32;
    
    for (int i = 0; i < SAMPLES; i++) {
        raw_sum += adc1_get_raw(BATTERY_ADC_CHANNEL);
        delayMicroseconds(500);  // 500µs (10x mais rápido que delay(5))
    }
    
    uint16_t raw_average = raw_sum / SAMPLES;
    
    // 3. Conversão calibrada usando eFuse do ESP32
    uint32_t voltage_at_pin_mv = esp_adc_cal_raw_to_voltage(raw_average, adc_chars);
    
    // 4. Aplicar divisor de tensão (2:1 da placa) + calibração manual
    float voltage = ((float)voltage_at_pin_mv * VOLTAGE_DIVIDER_RATIO / 1000.0f) + calibration_offset;
    
    return voltage;
}

void setup_sensor_power() {
    // Configura pino de controle do MOSFET
    pinMode(SENSOR_POWER_CTRL_PIN, OUTPUT);
    // Para MOSFET Canal N (low-side), NÍVEL BAIXO desliga
    digitalWrite(SENSOR_POWER_CTRL_PIN, LOW); 

    // Configura pino do ADC
    pinMode(BATTERY_ADC_PIN, INPUT);

    // Configura ADC1 para leitura de bateria
    adc1_config_width(BATTERY_ADC_WIDTH);
    adc1_config_channel_atten(BATTERY_ADC_CHANNEL, BATTERY_ADC_ATTEN);

    // Aloca memória para a estrutura de calibração
    adc_chars = (esp_adc_cal_characteristics_t*) calloc(1, sizeof(esp_adc_cal_characteristics_t));

    if (adc_chars == nullptr) {
        if (Serial) Serial.println("❌ PowerManager: Falha ao alocar memória para ADC!");
        return;
    }

    
    // Caracteriza ADC usando eFuse do ESP32 (calibração de fábrica)
    esp_adc_cal_characterize(ADC_UNIT_1, BATTERY_ADC_ATTEN, BATTERY_ADC_WIDTH, 1100, adc_chars);
    
    if (Serial) {
        Serial.println("PowerManager: Sensor power e ADC inicializados");
        Serial.printf("Calibration offset: %.2fV\n", calibration_offset);
    }
}

void power_sensors_on() {
    if (Serial) {
        Serial.println("PowerManager: Ligando sensores...");
    }
    // Para MOSFET Canal N (low-side), NÍVEL ALTO liga
    digitalWrite(SENSOR_POWER_CTRL_PIN, HIGH);
    
    if (Serial) {
        Serial.println("PowerManager: Sensores ligados.");
    }
}

void power_sensors_off() {
    if (Serial) {
        Serial.println("PowerManager: Desligando sensores...");
    }
    // Para MOSFET Canal N (low-side), NÍVEL BAIXO desliga
    digitalWrite(SENSOR_POWER_CTRL_PIN, LOW);
    delay(100); // Pequeno delay para garantir o corte total
    if (Serial) {
        Serial.println("PowerManager: Sensores desligados.");
    }
}

void get_battery_status(Battery_Data &data) {
    // 1. Leitura bruta da tensão
    float voltage_raw = read_battery_voltage_raw();
    
    // 2. Aplicar filtro de média móvel
    float voltage_filtered = apply_moving_average(voltage_raw);
    
    data.voltage = voltage_filtered;
    
    // ==================== DETECÇÃO USB ====================
    if (voltage_filtered < USB_DETECTION_THRESHOLD) {
        data.is_usb_connected = true;
        data.percentage = 0.0f;
        data.days_remaining = 0.0f;
        data.is_critical = false;
        
        if (Serial) {
            Serial.println("   Status: 🔌 USB conectado (carregando ou sem bateria)");
        }
        return;
    }
    
    data.is_usb_connected = false;
    
    // ==================== PROTEÇÃO DEEP DISCHARGE ====================
    if (voltage_filtered < BATTERY_CUTOFF_VOLTAGE) {
        data.percentage = 0.0f;
        data.days_remaining = 0.0f;
        data.is_critical = true;
        
        if (Serial) {
            Serial.println("   ⚠️ CRÍTICO: Tensão abaixo do limite de segurança!");
            Serial.printf("   ⚠️ Tensão atual: %.2fV | Mínimo: %.2fV\n", 
                         voltage_filtered, BATTERY_CUTOFF_VOLTAGE);
            Serial.println("   ⚠️ Sistema entrará em deep sleep para proteger bateria.");
        }
        
        // OPCIONAL: Descomente para forçar deep sleep em tensão crítica
        enter_deep_sleep();
        return;
    }
    
    // ==================== CÁLCULO DE PORCENTAGEM ====================
    data.percentage = voltage_to_percentage(voltage_filtered);
    data.is_critical = (data.percentage < 10.0f);
    
    // ==================== CÁLCULO DE AUTONOMIA ====================
    float remaining_capacity_mah = BATTERY_CAPACITY_MAH * (data.percentage / 100.0f);
    float daily_consumption_mah = SYSTEM_AVG_CONSUMPTION_MA * 24.0f;
    
    if (daily_consumption_mah > 0) {
        data.days_remaining = remaining_capacity_mah / daily_consumption_mah;
    } else {
        data.days_remaining = 0.0f;
    }
}

void enter_deep_sleep() {
    uint64_t sleep_time_us = TIME_TO_SLEEP_INTERVAL_MINUTES * MINUTES_TO_uS_FACTOR;

    if (Serial) {
        Serial.printf("PowerManager: Entrando em Deep Sleep por %llu segundos (%d minutos)...\n", 
            sleep_time_us / uS_TO_S_FACTOR, TIME_TO_SLEEP_INTERVAL_MINUTES);
        Serial.flush(); // Garante que a mensagem serial seja enviada antes de dormir
    }
    
    // Configura a fonte de despertar (Timer)
    esp_sleep_enable_timer_wakeup(sleep_time_us);
    
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);

    // Inicia o Deep Sleep
    esp_deep_sleep_start();
}