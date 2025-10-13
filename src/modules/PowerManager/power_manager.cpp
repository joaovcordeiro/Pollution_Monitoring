#include "power_manager.h"
#include "config.h" 

// Fatores de conversão para o tempo de sleep
#define uS_TO_S_FACTOR 1000000ULL
#define MINUTES_TO_uS_FACTOR (60ULL * uS_TO_S_FACTOR)

void setup_sensor_power() {
    pinMode(SENSOR_POWER_CTRL_PIN, OUTPUT);
    // Garante que os sensores comecem desligados
    // Para MOSFET Canal N (low-side), NÍVEL BAIXO desliga
    digitalWrite(SENSOR_POWER_CTRL_PIN, LOW); 
    if (Serial) { // Verifica se a Serial foi inicializada para evitar erros
        Serial.println("PowerManager: Sensor power control initialized. Sensors OFF.");
    }
}

void power_sensors_on() {
    if (Serial) {
        Serial.println("PowerManager: Powering sensors ON...");
    }
    // Para MOSFET Canal N (low-side), NÍVEL ALTO liga
    digitalWrite(SENSOR_POWER_CTRL_PIN, HIGH);
    
    if (Serial) {
        Serial.printf("PowerManager: Waiting %dms for sensor stabilization...\n", SENSOR_STABILIZATION_DELAY_MS);
    }
    delay(SENSOR_STABILIZATION_DELAY_MS); // Delay para estabilização dos sensores
    
    if (Serial) {
        Serial.println("PowerManager: Sensors presumed ON and stabilized.");
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