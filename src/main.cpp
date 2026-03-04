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

// Variáveis Globais para Dados dos Sensores
SCD40_Data scd40SensorData;
MICS6814_Data mics6814SensorData; 
GPS_Data gpsLocationData;
SPS30_Data sps30SensorData;
Battery_Data batteryData;


// Callback para o driver do SPS30 verificar se pode continuar rodando
static bool sps30_battery_check_cb() {
    float v_bat = ads1115_read_battery_voltage(); // Usa sua função nova
    
    // Se a bateria cair para 3.40V ou menos, aborta o SPS30 imediatamente.
    // Isso evita que o ESP32 reinicie por falta de tensão.
    if (v_bat < 3.40f) {
        Serial.printf("[SPS30] ABORT: Bateria crítica (%.2f V)\n", v_bat);
        return false;
    }
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println(F("\n--- System Boot / Wake Up  ---"));

    init_serial();      // SerialAT do modem
    Wire.begin();
    setup_power_pins(); 

    Serial.println(F("\nMain: Ligando Booster 5V (Sensores)..."));
    power_booster_on();
    delay(1000);

    sps30_init();

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

    // 3. OBTER PERFIL DE CONSUMO  
    ProfileConfig cfg = get_system_profile(v_bat);

    const char* profileNames[] = {"ECONOMIA", "NORMAL", "PERFORMANCE"};
    Serial.printf("Main: Perfil Ativo -> [%s]\n", profileNames[cfg.profile]);

    bool i2c_healthy = true;

    delay(1000);

    // ===================== SPS30 (Particulados) =====================
    if (cfg.run_sps30) {
        if (sps30_init()) {
            SPS30_ReadConfig spsCfg;
            spsCfg.power_ok = &sps30_battery_check_cb; 
            spsCfg.stop_measurement_after_read = true;
            
            // Aplica as configurações do perfil
            spsCfg.target_samples = cfg.sps_samples;
            spsCfg.min_valid_samples = (cfg.sps_samples > 5) ? (cfg.sps_samples - 5) : 2;
            spsCfg.max_seconds = cfg.sps_timeout;

            Serial.printf("[SPS30] Rodando: Samples=%d, Timeout=%ds\n", 
                          spsCfg.target_samples, spsCfg.max_seconds);

            bool success = sps30_read_measurements(sps30SensorData, spsCfg);
            
            if (!success) {
                Serial.println("[SPS30] Falha. Resetando I2C.");
                Wire.end(); delay(50); Wire.begin(); Wire.setClock(10000);
                i2c_healthy = false;
            }
        }
    } else {
        Serial.println("[SPS30] OFF (Economia de Energia)");
    }

    delay(2000);

    // ===================== SCD40 (CO2) =====================
    if (cfg.run_scd40) {
        // Se I2C falhou no SPS, tenta recuperar
        if (!i2c_healthy) { Wire.begin(); Wire.setClock(10000); }

        if (scd40_init()) {
            // Assume uso da função Single Shot
            scd40_read_measurements(scd40SensorData);
        }
    } else {
        Serial.println("[SCD40] OFF (Corte Crítico)");
    }

    // ---------- MICS ----------
    #if ENABLE_MICS
    Serial.println(F("\nMain: Iniciando ciclo do MICS6814..."));
    
    // 1. Liga a energia (Heater do MOX começa a esquentar)
    power_analogics_on();
    
    // 2. Carrega as calibrações na memória
    mics6814_init(CALIBRATED_R0_CO, CALIBRATED_R0_NO2, CALIBRATED_R0_NH3);
    
    // 3. Aguarda o aquecimento do filamento (Warm-up bloqueante)
    Serial.printf("Main: Warm-up MICS aguardando %d ms...\n", SENSOR_STABILIZATION_DELAY_MS);
    delay(SENSOR_STABILIZATION_DELAY_MS); 
    
    // 4. Faz a leitura
    Serial.println(F("Main: Lendo dados analógicos via ADS1115..."));
        if (!mics6814_read_data(mics6814SensorData)) {
            Serial.println(F("Main: Falha na leitura do MICS6814."));
            mics6814SensorData.isValid = false;
        }

    // 5. CRUCIAL: Desliga a energia imediatamente após ler (Economia de Bateria)
    power_analogics_off();
    Serial.println(F("Main: MICS6814 desligado (Heater Off)."));

    // 6. Log dos resultados
    Serial.printf("Main: MICS (isValid: %d) -> CO: %.2f ppm | NO2: %.2f ppm | NH3: %.2f ppm\n",
                  (int)mics6814SensorData.isValid, 
                  mics6814SensorData.ppm_co,
                  mics6814SensorData.ppm_no2,
                  mics6814SensorData.ppm_nh3);
    #else
        // Se o sensor estiver desabilitado na compilação, garanta que o dado seja inválido
        mics6814SensorData.isValid = false;
        
        // Um pequeno delay apenas para o barramento I2C respirar caso outros sensores precisem
        delay(300); 
    #endif

    Serial.println(F("\nMain: Desligando Booster 5V (Sensores)..."));
    power_booster_off();

    // ---------- Comunicação ----------
    Serial.println(F("Main: Starting full communication cycle..."));
    unsigned long commStartTime = millis();

    // Lê bateria final para envio
    batteryData.voltage = ads1115_read_battery_voltage();
    batteryData.percentage = voltage_to_percentage(batteryData.voltage);

    bool dataTransmissionSuccessful = perform_communication_cycle(
        scd40SensorData,
        mics6814SensorData,
        sps30SensorData,
        batteryData,
        gpsLocationData
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