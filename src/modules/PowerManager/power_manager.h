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
void setup_sensor_power();

/**
 * @brief Liga alimentação dos sensores via MOSFET
 * 
 * Para MOSFET N-Channel low-side:
 * - HIGH = Sensores ligados
 * - LOW = Sensores desligados
 */
void power_sensors_on();

/**
 * @brief Desliga alimentação dos sensores via MOSFET
 * 
 * Inclui delay de 100ms para garantir corte completo
 * antes de processar próxima operação.
 */
void power_sensors_off();

/**
 * @brief Obtém status completo da bateria
 * 
 * Executa:
 * 1. Leitura ADC com oversampling (32 amostras)
 * 2. Filtro de média móvel (5 leituras históricas)
 * 3. Detecção de USB vs bateria
 * 4. Cálculo de porcentagem (curva não-linear)
 * 5. Estimativa de autonomia
 * 6. Verificação de estado crítico
 * 
 * @param data Estrutura Battery_Data preenchida com os resultados
 * 
 * Tempo de execução: ~16ms (otimizado)
 */
void get_battery_status(Battery_Data &data);

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

#endif // POWER_MANAGER_H