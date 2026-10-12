#pragma once
// --- Power Control Pins (Topologia Dual-Switch) ---
// MOSFET 1 (IRLZ44N): Controla MICS (GND)
#define PIN_POWER_ANALOGIC_SENSOR  25
#define PIN_MASTER_POWER  13


// Fator de Correção da Bateria (Calibrado em 31/01)
// Tensão Real = Leitura ADS * BATTERY_CORRECTION_FACTOR
#define BATTERY_CORRECTION_FACTOR  1.0033f 

#define MODEM_PWRKEY_PIN   GPIO_NUM_4  // GPIO para ligar/desligar o modem 
#define MODEM_RST_PIN      GPIO_NUM_5  // GPIO para resetar o modem
#define MODEM_TX_PIN       GPIO_NUM_27 // GPIO TX do ESP32 para RX do Modem
#define MODEM_RX_PIN       GPIO_NUM_26 // GPIO RX do ESP32 para TX do Modem

#define ENABLE_SPS30 1
#define ENABLE_SCD40 1
#define ENABLE_MICS 1

#define BATTERY_ADC_PIN 35
#define BATTERY_ADC_CHANNEL ADC1_CHANNEL_7  // GPIO35 = ADC1_CH7
#define BATTERY_ADC_ATTEN ADC_ATTEN_DB_12   // 0-3.3V
#define BATTERY_ADC_WIDTH ADC_WIDTH_BIT_12

// --- Configurações GSM/GPRS ---
const char APN[] = "marquetplaceiot.arqia.br"; 
const char GPRS_USER[] = "arqia";      
const char GPRS_PASS[] = "arqia";      

// ==================== CONFIGURAÇÕES DE OPERAÇÃO ====================

// Intervalo de Deep Sleep (minutos)
#define TIME_TO_SLEEP_INTERVAL_MINUTES  30

// Intervalo entre atualizações de GPS (horas)
// O GPS só roda efetivamente a cada N horas. Entre os intervalos,
// reutiliza a última localização salva na memória RTC do ESP32.
#define GPS_UPDATE_INTERVAL_HOURS  12

// Habilita/desabilita MICS6814 na compilação (1 = habilitado, 0 = desabilitado)
// NOTA: Mesmo habilitado, o perfil de energia pode desligá-lo em runtime
#define ENABLE_MICS  1


#include "secrets.h"