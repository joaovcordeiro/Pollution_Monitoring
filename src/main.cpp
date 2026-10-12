#include "config.h"
#include "modules/PowerManager/power_manager.h"
#include "modules/SCD40/scd40_handler.h"
#include "modules/MICS6814/mics6814_handler.h" 
#include "modules/ConnectivityHandler/comm_manager.h"
#include "modules/SPS30/sps30_handler.h"

// Insira os valores de R0 que você obteve do script "MICS_Calibrar.ino"
const int16_t CALIBRATED_R0_CO  = 12345; // <-- SUBSTITUA ESTE VALOR
const int16_t CALIBRATED_R0_NO2 = 6789;  // <-- SUBSTITUA ESTE VALOR
const int16_t CALIBRATED_R0_NH3 = 10111; // <-- SUBSTITUA ESTE VALOR

// ==================== GPS CACHE (Memória RTC) ====================
 
RTC_DATA_ATTR RTC_GPS_Cache gps_cache = {0};
RTC_DATA_ATTR uint32_t boot_count = 0;
 
/**
 * @brief Salva um fix GPS válido no cache RTC para reutilização futura.
 */
void save_gps_to_cache(const GPS_Data& gps_data) {
    if (!gps_data.isValid) return;
    gps_cache.magic     = GPS_CACHE_MAGIC;
    gps_cache.latitude  = gps_data.latitude;
    gps_cache.longitude = gps_data.longitude;
    gps_cache.accuracy  = gps_data.accuracy;
    gps_cache.boot_count = boot_count;
    
    // Captura o timestamp atual (UTC) se o relógio estiver sincronizado
    time_t now;
    time(&now);
    gps_cache.timestamp_epoch = (uint32_t)now;
    
    Serial.printf("[GPS Cache] Localização salva no RTC (epoch: %lu)\n", gps_cache.timestamp_epoch);
}

// ==================== I2C BUS RECOVERY ====================
 
#define I2C_SDA_PIN 21  
#define I2C_SCL_PIN 22  
 
static bool i2c_bus_recovery() {
    Serial.println(F("[I2C] Iniciando recovery do barramento..."));
    
    // 1. Desliga o Wire para ter controle manual dos pinos
    Wire.end();
    delay(10);
    
    // 2. Configura SCL como saída, SDA como input com pull-up
    pinMode(I2C_SCL_PIN, OUTPUT);
    pinMode(I2C_SDA_PIN, INPUT_PULLUP);
    
    // 3. Verifica se SDA está stuck LOW
    if (digitalRead(I2C_SDA_PIN) == HIGH) {
        Serial.println(F("[I2C] SDA livre. Recovery não necessário."));
        // Mesmo assim reinicia o Wire limpo
        Wire.begin();
        Wire.setClock(10000);
        Wire.setTimeOut(2000);
        return true;
    }
    
    Serial.println(F("[I2C] SDA preso em LOW! Pulsando SCL..."));
    
    // 4. Pulsa SCL até 18 vezes (9 bits de dado + 9 de ACK no pior caso)
    for (int i = 0; i < 18; i++) {
        digitalWrite(I2C_SCL_PIN, HIGH);
        delayMicroseconds(5);
        digitalWrite(I2C_SCL_PIN, LOW);
        delayMicroseconds(5);
        
        // Verifica se SDA liberou
        if (digitalRead(I2C_SDA_PIN) == HIGH) {
            Serial.printf("[I2C] SDA liberado após %d pulsos de clock.\n", i + 1);
            break;
        }
    }
    
    // 5. Envia STOP condition (SDA LOW→HIGH enquanto SCL está HIGH)
    pinMode(I2C_SDA_PIN, OUTPUT);
    digitalWrite(I2C_SDA_PIN, LOW);
    delayMicroseconds(5);
    digitalWrite(I2C_SCL_PIN, HIGH);
    delayMicroseconds(5);
    digitalWrite(I2C_SDA_PIN, HIGH);
    delayMicroseconds(5);
    
    // 6. Reinicia o Wire normalmente
    Wire.begin();
    Wire.setClock(10000);
    Wire.setTimeOut(2000);
    
    // 7. Verifica se funcionou
    bool recovered = (digitalRead(I2C_SDA_PIN) == HIGH);
    Serial.printf("[I2C] Recovery %s.\n", recovered ? "BEM-SUCEDIDO" : "FALHOU");
    return recovered;
}
 
// Reinicia o barramento I2C de forma limpa
static void i2c_restart() {
    Wire.end();
    delay(50);
    Wire.begin();
    Wire.setClock(10000);
    Wire.setTimeOut(2000);
}

// ==================== Variáveis Globais para dados dos sensores ====================
SCD40_Data scd40SensorData;
MICS6814_Data mics6814SensorData; 
GPS_Data gpsLocationData;
SPS30_Data sps30SensorData;
Battery_Data batteryData;

// ==================== Proteção de Sag (SPS30) ====================
static const float    SAG_VOLTAGE_THRESHOLD    = 3.40f;  // Limiar de proteção (V)
static const uint8_t  SAG_CONSECUTIVE_REQUIRED = 3;      // Leituras consecutivas para confirmar
static const uint16_t SAG_RECHECK_DELAY_MS     = 500;    // Delay entre re-leituras (ms)

static uint8_t sag_fail_counter = 0; // Contador persistente entre chamadas do callback

// Callback para o driver do SPS30 verificar se pode continuar rodando
static bool sps30_battery_check_cb() {
    float v_bat = ads1115_read_battery_voltage(); 
    
    // Se a bateria cair para 3.40V ou menos, aborta o SPS30 imediatamente.
    // Isso evita que o ESP32 reinicie por falta de tensão.
     if (v_bat < SAG_VOLTAGE_THRESHOLD) {
        sag_fail_counter++;
        Serial.printf("[SPS30] Tensão baixa detectada: %.2f V (%d/%d)\n", 
                      v_bat, sag_fail_counter, SAG_CONSECUTIVE_REQUIRED);
        
        if (sag_fail_counter >= SAG_CONSECUTIVE_REQUIRED) {
            // Confirmado: bateria realmente fraca
            Serial.printf("[SPS30] ABORT CONFIRMADO: Bateria crítica (%.2f V, %d leituras consecutivas)\n", 
                          v_bat, sag_fail_counter);
            return false;
        }
        
        // Ainda pode ser sag transitório — espera e deixa o loop re-checar
        delay(SAG_RECHECK_DELAY_MS);
        return true;
    }

    // Tensão OK — reseta o contador de falhas
    if (sag_fail_counter > 0) {
        Serial.printf("[SPS30] Tensão recuperou: %.2f V (sag descartado)\n", v_bat);
        sag_fail_counter = 0;
    }
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    boot_count++;
    Serial.printf("\n--- System Boot / Wake Up (Boot #%lu) ---\n", boot_count);

    init_serial();      // SerialAT do modem
    Wire.begin();
    setup_power_pins(); 

    Serial.println(F("\nMain: Ligando Booster 5V (Sensores)..."));
    power_booster_on();
    delay(1000);

    sps30_init();  // Reset preventivo

    Wire.setClock(10000); 
    Wire.setTimeOut(2000);
    
    // ---------- ADS ----------
    bool ads_ok = ads1115_init(0x48);
    delay(1000);
    if (!ads_ok) {
        Serial.println(F("Main: FALHA CRÍTICA - ADS1115 não encontrado."));
        // Sem o ADS o sistema fica sem visão da tensão da bateria, então entro em sleep.
        enter_deep_sleep();
    } 

    float v_bat = ads1115_read_battery_voltage();
    Serial.printf("Main: Tensão de Boot: %.2f V\n", v_bat);

    // No caso da bateria chegar a menos de 3.4V interrompemos o processo para permitir a recarga.
    if (v_bat < 3.40f) {
        Serial.println("BATERIA CRÍTICA (< 3.40V). Abortando para proteger hardware.");
        Serial.println("Sistema indo dormir profundamente...");
        Serial.flush();
        
        // Dorme por 1 hora para não drenar o resto da bateria em loops de boot
        esp_sleep_enable_timer_wakeup(3600ULL * 1000000ULL); 
        esp_deep_sleep_start();
    }

    // OBTER PERFIL DE CONSUMO  
    ProfileConfig cfg = get_system_profile(v_bat);

    const char* profileNames[] = {"ECONOMIA", "NORMAL", "PERFORMANCE"};
    Serial.printf("Main: Perfil Ativo -> [%s]\n", profileNames[cfg.profile]);

    bool i2c_healthy = true;

    delay(1000);

    // ===================== SPS30 (Particulados) =====================
    if (cfg.run_sps30) {
        bool sps_ok = false;
        for (int attempt = 1; attempt <= 2 && !sps_ok; attempt++) {
            if (attempt > 1) {
                Serial.printf("[SPS30] Retry %d/2 após recovery I2C...\n", attempt);
                i2c_bus_recovery();
                delay(500);
            }
            
            sag_fail_counter = 0;
            if (sps30_init()) {
                SPS30_ReadConfig spsCfg;
                spsCfg.power_ok = &sps30_battery_check_cb; 
                spsCfg.stop_measurement_after_read = true;
                spsCfg.target_samples = cfg.sps_samples;
                spsCfg.min_valid_samples = (cfg.sps_samples > 5) ? (cfg.sps_samples - 5) : 2;
                spsCfg.max_seconds = cfg.sps_timeout;
 
                Serial.printf("[SPS30] Rodando: Samples=%d, Timeout=%ds\n", 
                              spsCfg.target_samples, spsCfg.max_seconds);
 
                sps_ok = sps30_read_measurements(sps30SensorData, spsCfg);
            }
        }
        
        if (!sps_ok) {
            Serial.println("[SPS30] Falha após 2 tentativas. Resetando I2C para próximo sensor.");
            i2c_bus_recovery();
        }
    } else {
        Serial.println("[SPS30] OFF (Economia de Energia)");
    }

    delay(2000);

    // ===================== SCD40 (CO2) =====================
    if (cfg.run_scd40) {
        bool scd_ok = false;
        for (int attempt = 1; attempt <= 2 && !scd_ok; attempt++) {
            if (attempt > 1) {
                Serial.printf("[SCD40] Retry %d/2 após recovery I2C...\n", attempt);
                i2c_bus_recovery();
                delay(500);
            }
            
            if (scd40_init()) {
                scd_ok = scd40_read_measurements(scd40SensorData);
            }
        }
        
        if (!scd_ok) {
            Serial.println("[SCD40] Falha após 2 tentativas.");
            i2c_bus_recovery();
        }
    } else {
        Serial.println("[SCD40] OFF (Corte Crítico)");
    }

    // ---------- MICS ----------
    #if ENABLE_MICS
    if (cfg.run_mics) {
        Serial.println(F("\nMain: Iniciando ciclo do MICS6814..."));
        power_analogics_on();
        mics6814_init(CALIBRATED_R0_CO, CALIBRATED_R0_NO2, CALIBRATED_R0_NH3);
        
        Serial.printf("Main: Warm-up MICS aguardando %lu ms (perfil: %s)...\n", 
                      cfg.mics_warmup_ms, profileNames[cfg.profile]);
        delay(cfg.mics_warmup_ms); 
        
        Serial.println(F("Main: Lendo dados analógicos via ADS1115..."));
        if (!mics6814_read_data(mics6814SensorData)) {
            Serial.println(F("Main: Falha na leitura do MICS6814."));
            mics6814SensorData.isValid = false;
        }
 
        power_analogics_off();
        Serial.println(F("Main: MICS6814 desligado (Heater Off)."));
 
        Serial.printf("Main: MICS (isValid: %d) -> CO: %.2f ppm | NO2: %.2f ppm | NH3: %.2f ppm\n",
                      (int)mics6814SensorData.isValid, 
                      mics6814SensorData.ppm_co,
                      mics6814SensorData.ppm_no2,
                      mics6814SensorData.ppm_nh3);
    } else {
        Serial.println(F("[MICS] OFF (Economia de Energia - Perfil)"));
        mics6814SensorData.isValid = false;
    }
    #else
        // Sensor desabilitado na compilação
        mics6814SensorData.isValid = false;
        delay(300); 
    #endif

    // ---------- Leitura final de bateria ----------
    batteryData.voltage = ads1115_read_battery_voltage();
    batteryData.percentage = voltage_to_percentage(batteryData.voltage);
    Serial.printf("Main: Bateria pré-comunicação: %.2f V (%.0f%%)\n", 
                  batteryData.voltage, batteryData.percentage);
 
    Serial.println(F("\nMain: Desligando Booster 5V (Sensores)..."));
    power_booster_off();

    // ---------- Comunicação ----------
    Serial.println(F("Main: Starting full communication cycle..."));
    unsigned long commStartTime = millis();

    bool dataTransmissionSuccessful = perform_communication_cycle(
        scd40SensorData,
        mics6814SensorData,
        sps30SensorData,
        batteryData,
        gpsLocationData,
        cfg   
    );

    Serial.printf("Communication phase took: %lu ms\n", millis() - commStartTime);

    if (dataTransmissionSuccessful) {
        Serial.println(F("Main: Data transmission cycle reported as successful."));
    } else {
        Serial.println(F("Main: Data transmission cycle had errors or was incomplete."));
    }

    Serial.println(F("Main: Preparing to enter Deep Sleep..."));
    enter_deep_sleep();
}

void loop() {
    Serial.println(F("Main: Loop reached - unexpected! Forcing sleep."));
    delay(2000);
    enter_deep_sleep();
}