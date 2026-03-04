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

ProfileConfig get_system_profile(float voltage) {
    ProfileConfig cfg;

    // 1. Determina o Perfil Baseado na Tensão
    if (voltage < 3.55f) {
        cfg.profile = PROFILE_ECONOMY;
    } else if (voltage < 3.80f) {
        cfg.profile = PROFILE_NORMAL;
    } else {
        cfg.profile = PROFILE_PERFORMANCE;
    }

    // 2. Preenche as regras de cada perfil
    switch (cfg.profile) {
        case PROFILE_PERFORMANCE: // > 3.80V
            cfg.run_sps30 = true;
            cfg.sps_samples = 30;     // Qualidade Max
            cfg.sps_timeout = 45;
            
            cfg.run_mics = true;
            cfg.mics_warmup_ms = 30000; // 30s Aquecimento
            
            cfg.run_scd40 = true;
            cfg.run_gps = true;         // GPS Ativo
            cfg.gps_timeout_ms = 60000;
            break;

        case PROFILE_NORMAL: // 3.55 - 3.80V
            cfg.run_sps30 = true;
            cfg.sps_samples = 15;     // Qualidade Média
            cfg.sps_timeout = 25;
            
            cfg.run_mics = true;
            cfg.mics_warmup_ms = 15000; // 15s (Mínimo aceitável)
            
            cfg.run_scd40 = true;
            cfg.run_gps = true;       // GPS ainda roda
            cfg.gps_timeout_ms = 30000; // Timeout menor
            break;

        case PROFILE_ECONOMY: // 3.40 - 3.55V
            cfg.run_sps30 = true;
            cfg.sps_samples = 5;      // Tiro Rápido (Snapshot)
            cfg.sps_timeout = 15;
            
            cfg.run_mics = false;     // CORTE: Economiza ~40mA
            cfg.mics_warmup_ms = 0;
            
            cfg.run_scd40 = true;     // SCD40 gasta pouco, mantém
            cfg.run_gps = false;      // CORTE: GPS gasta muito
            cfg.gps_timeout_ms = 0;
            break;

    }

    return cfg;
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

float get_battery_voltage_instant() {
    return read_battery_voltage_raw(); 
}


void setup_power_pins() {
    // Configura pino de controle do MOSFET
    pinMode(PIN_POWER_ANALOGIC_SENSOR, OUTPUT);
    pinMode(PIN_MASTER_POWER , OUTPUT);


    // Para MOSFET Canal N (low-side), NÍVEL BAIXO desliga
    digitalWrite(PIN_POWER_ANALOGIC_SENSOR, LOW);
    digitalWrite(PIN_MASTER_POWER, LOW);


    // Configura pino do ADC
    pinMode(BATTERY_ADC_PIN, INPUT);

    // Configura ADC1 para leitura de bateria
    adc1_config_width(BATTERY_ADC_WIDTH);
    adc1_config_channel_atten(BATTERY_ADC_CHANNEL, BATTERY_ADC_ATTEN);

    // Aloca memória para a estrutura de calibração
    adc_chars = (esp_adc_cal_characteristics_t*) calloc(1, sizeof(esp_adc_cal_characteristics_t));

    if (adc_chars == nullptr) {
        if (Serial) Serial.println("PowerManager: Falha ao alocar memória para ADC!");
        return;
    }

    
    // Caracteriza ADC usando eFuse do ESP32 (calibração de fábrica)
    esp_adc_cal_characterize(ADC_UNIT_1, BATTERY_ADC_ATTEN, BATTERY_ADC_WIDTH, 1100, adc_chars);
    
    if (Serial) {
        Serial.println("PowerManager: Sensor power e ADC inicializados");
        Serial.printf("Calibration offset: %.2fV\n", calibration_offset);
    }
}

void power_analogics_on() {
    digitalWrite(PIN_POWER_ANALOGIC_SENSOR, HIGH);
    delay(500);
}

void power_analogics_off() {
    digitalWrite(PIN_POWER_ANALOGIC_SENSOR, LOW);
    delay(500);
}

void power_booster_on() {
    digitalWrite(PIN_MASTER_POWER, HIGH);
    delay(500);
}

void power_booster_off() {
    digitalWrite(PIN_MASTER_POWER, LOW);
    delay(500);
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