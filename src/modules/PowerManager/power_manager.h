#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>     

// ==================== CONFIGURAÇÕES DA BATERIA ====================

// Especificações da bateria
#define BATTERY_CAPACITY_MAH 2000.0f        // Capacidade nominal (mAh)
#define SYSTEM_AVG_CONSUMPTION_MA 150.0f    // Consumo médio do sistema (mA)

// Hardware da placa
#define VOLTAGE_DIVIDER_RATIO 2.0f          // Divisor resistivo 100k/100k = 2:1

// Limites de segurança
#define BATTERY_CUTOFF_VOLTAGE 3.0f         // Tensão mínima (proteção deep discharge)
#define BATTERY_FULL_VOLTAGE 4.2f           // Tensão máxima (carga completa)
#define USB_DETECTION_THRESHOLD 2.5f        // Limiar para detecção de USB

// Filtro de média móvel
#define MOVING_AVERAGE_SAMPLES 5            // Número de amostras para média

// ==================== ESTRUTURAS DE DADOS ====================

/**
 * @brief Estrutura com dados completos da bateria
 */
struct Battery_Data {
    float voltage;          // Tensão atual (Volts)
    float percentage;       // Carga estimada (0-100%)
    float days_remaining;   // Autonomia prevista (dias)
    bool is_usb_connected;  // true = USB conectado / false = bateria
    bool is_critical;       // true = carga < 10% (estado crítico)
};

// Definição dos Perfis
typedef enum {
    PROFILE_ECONOMY,      // 3.40V - 3.55V
    PROFILE_NORMAL,       // 3.55V - 3.80V
    PROFILE_PERFORMANCE   // > 3.80V
} SystemProfile;

// Configuração Dinâmica (O que roda em cada perfil)
struct ProfileConfig {
    SystemProfile profile;
    
    // SPS30
    bool run_sps30;
    uint8_t sps_samples;
    uint16_t sps_timeout;
    
    // MICS6814
    bool run_mics;
    uint32_t mics_warmup_ms;
    
    // SCD40
    bool run_scd40;
    
    // GPS
    bool run_gps;
    uint32_t gps_timeout_ms;
};

ProfileConfig get_system_profile(float voltage);

/**
 * @brief Inicializa sistema de gerenciamento de energia
 * 
 * Configura:
 * - Pino de controle do MOSFET (sensor power)
 * - ADC para leitura de bateria (GPIO35)
 * - Calibração do ADC usando eFuse
 * 
 * Deve ser chamado UMA VEZ no setup()
 */
void setup_power_pins();

/**
 * @brief Liga alimentação dos sensores via MOSFET
 * 
 * Para MOSFET N-Channel low-side:
 * - HIGH = Sensores ligados
 * - LOW = Sensores desligados
 */
void power_analogics_on();

/**
 * @brief Desliga alimentação dos sensores via MOSFET
 * 
 * Inclui delay de 100ms para garantir corte completo
 * antes de processar próxima operação.
 */
void power_analogics_off();

void power_booster_off();

void power_booster_on();



/**
 * @brief Lê a tensão da bateria sem filtro (instantâneo)
 * Útil para detectar queda rápida (sag) durante picos de corrente.
 */
float get_battery_voltage_instant();

/**
 * @brief Entra em modo Deep Sleep
 * 
 * Configuração:
 * - Timer wake-up baseado em TIME_TO_SLEEP_INTERVAL_MINUTES
 * - ESP32 reinicia após despertar
 * - Código após esta chamada não executa
 * 
 * Deep Sleep garante:
 * - Consumo ultra-baixo (~10µA)
 * - WiFi/Bluetooth desligados
 * - CPU desligada
 * - Apenas RTC ativo
 */
void enter_deep_sleep();

// ==================== FUNÇÕES AUXILIARES (DIAGNÓSTICO) ====================

/**
 * @brief Calibra ADC da bateria usando multímetro
 * 
 * Procedimento:
 * 1. Meça tensão real da bateria com multímetro de qualidade
 * 2. Chame esta função: calibrate_battery_adc(3.87); // exemplo
 * 3. Anote o offset calculado no Serial Monitor
 * 4. Atualize 'calibration_offset' no power_manager.cpp
 * 
 * @param measured_voltage Tensão medida no multímetro (Volts)
 * 
 * Exemplo de uso no setup():
 * 
 *     setup_sensor_power();
 *     calibrate_battery_adc(3.87);  // Valor do multímetro
 *     while(1);  // Pare aqui para ler resultado
 * 
 * IMPORTANTE: Esta função é apenas para calibração inicial.
 * Remova ou comente após ajustar o offset!
 */
void calibrate_battery_adc(float measured_voltage);

/**
 * @brief Imprime diagnóstico completo do sistema de energia
 * 
 * Exibe no Serial:
 * - Tensão, carga, autonomia
 * - Status de USB e estado crítico
 * - Parâmetros de configuração
 * - Valor de calibração aplicado
 * 
 * Útil para:
 * - Debugging
 * - Validação após calibração
 * - Monitoramento durante testes
 */
void print_power_diagnostics();

float voltage_to_percentage(float voltage);

#endif // POWER_MANAGER_H