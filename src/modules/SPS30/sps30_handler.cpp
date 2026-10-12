#include "sps30_handler.h"
#include <Wire.h>

// Endereço I2C padrão do SPS30
#define SPS30_I2C_ADDR 0x69 

static SensirionI2cSps30 sps30;
static char errorMessage[256];
 
// Auxiliar para log de erros
static void log_sps_error(const char* context, uint16_t err) {
    if (err != 0) {
        Serial.printf("[SPS30] Erro em %s. Codigo: %u\n", context, err);
    }
}

void sps30_force_sleep() {
    sps30.stopMeasurement();
    delay(100);
    sps30.sleep();
}

bool sps30_init() {
    uint16_t error;
    Serial.println("SPS30: Initializing...");

    sps30.begin(Wire, SPS30_I2C_ADDR);

    // 1. Tenta acordar (caso tenha ficado dormindo do boot anterior)
    sps30.wakeUpSequence(); 
    delay(100);

    error = sps30.stopMeasurement();
    
    if (error == 0) {
        Serial.println("[SPS30] Comando STOP enviado com sucesso.");
    } else {
        Serial.printf("[SPS30] Aviso: Stop retornou erro 0x%04X (provavelmente já parado)\n", error);
    }

    uint8_t fw_major = 0, fw_minor = 0;
    error = sps30.readFirmwareVersion(fw_major, fw_minor);
    
    if (error != 0) {
        log_sps_error("readFirmwareVersion (Sensor não responde)", error);
        // Tenta mandar dormir mesmo com erro, só por garantia
        sps30.sleep();
        return false;
    }

    Serial.printf("[SPS30] Online! Firmware %u.%u\n", fw_major, fw_minor);

    sps30.clearDeviceStatusRegister();

    // 4. CRÍTICO: Mandar dormir imediatamente para economizar energia
    // O sensor só deve acordar na hora de ler.
    error = sps30.sleep();
    if (error == 0) {
        Serial.println("[SPS30] Sensor colocado em Sleep (Modo Baixo Consumo).");
    } else {
        log_sps_error("Init Sleep", error);
    }

    return true;
}

bool sps30_read_measurements(SPS30_Data &data, const SPS30_ReadConfig &cfg) {
    uint16_t error;
    data.isValid = false; 

    const int SAMPLES_TO_DISCARD = 5; // Ignora as primeiras 5 leituras


    // --- FASE 1: ACORDAR E LIGAR ---
    
    // Verifica bateria antes de começar (Fan consome pico de corrente)
    if (cfg.power_ok && !cfg.power_ok()) {
        Serial.println("[SPS30] Abortado: Bateria fraca antes do start.");
        return false;
    }

    // Acorda
    error = sps30.wakeUpSequence();
    if (error != 0) log_sps_error("WakeUp", error);
    delay(50);

    // Inicia medição (Float Mode)
    // Isso liga o FAN e o LASER (~60-80mA)
    error = sps30.startMeasurement(SPS30_OUTPUT_FORMAT_OUTPUT_FORMAT_FLOAT);
    if (error != 0) {
        log_sps_error("StartMeasurement", error);
        sps30_force_sleep(); // Tenta dormir de novo se falhar
        return false;
    }

    // [FIX] Grace period após ligar o fan para a tensão estabilizar.
    // O inrush current do fan causa um sag que se recupera em ~2s.
    // Não verificamos bateria durante esse período para evitar falsos positivos.
    const uint16_t FAN_STABILIZATION_MS = 2000;
    Serial.printf("[SPS30] Fan ligado. Aguardando estabilização (%dms)...\n", FAN_STABILIZATION_MS);
    delay(FAN_STABILIZATION_MS);


    Serial.printf("[SPS30] Aquecendo (%ds) e Coletando (Target: %d)...\n", SAMPLES_TO_DISCARD, cfg.target_samples);

    // --- FASE 2: LOOP DE COLETA ---
    
    float sum_pm1_0 = 0, sum_pm2_5 = 0, sum_pm4_0 = 0, sum_pm10_0 = 0, sum_size = 0;

    int valid_count = 0;  
    int warmup_count = 0;

    uint32_t start_time = millis();
    bool success = false;

    // Variáveis temporárias
    float mc1p0, mc2p5, mc4p0, mc10p0, nc0p5, nc1p0, nc2p5, nc4p0, nc10p0, typSize;

    while (valid_count < cfg.target_samples) {
        // Checagem de Timeout
        if ((millis() - start_time) > (cfg.max_seconds * 1000UL)) {
            Serial.println("[SPS30] Timeout atingido!");
            break;
        }

        // Checagem de Bateria (abortar se cair muito durante uso do fan)
        if (cfg.power_ok && !cfg.power_ok()) {
            Serial.println("[SPS30] Abortado durante leitura (Bateria caiu).");
            break;
        }

        // Verifica flag de dados prontos (Polling eficiente)
        uint16_t dataReady = 0;
        error = sps30.readDataReadyFlag(dataReady);
        
        if (error == 0 && dataReady) {
            // Lê valores
            error = sps30.readMeasurementValuesFloat(
                mc1p0, mc2p5, mc4p0, mc10p0,
                nc0p5, nc1p0, nc2p5, nc4p0, nc10p0,
                typSize
            );

            if (error == 0) {

                if (warmup_count < SAMPLES_TO_DISCARD) {
                    Serial.print("w"); // Feedback visual de 'warming up'
                    warmup_count++; 
                    continue; 
                }

                // Acumula médias
                sum_pm1_0 += mc1p0;
                sum_pm2_5 += mc2p5;
                sum_pm4_0 += mc4p0;
                sum_pm10_0 += mc10p0;
                sum_size += typSize;
                valid_count++;
                
                // Feedback visual simples (um ponto por leitura)
                Serial.print("."); 
            } else {
                log_sps_error("ReadValues", error);
            }
        }
        
        delay(500); // Espera meio segundo entre checagens
    }
    Serial.println(); // Nova linha

    // --- FASE 3: PROCESSAR RESULTADOS ---

    if (valid_count >= cfg.min_valid_samples) {
        data.pm1_0 = sum_pm1_0 / valid_count;
        data.pm2_5 = sum_pm2_5 / valid_count;
        data.pm4_0 = sum_pm4_0 / valid_count;
        data.pm10_0 = sum_pm10_0 / valid_count;
        data.typical_particle_size = sum_size / valid_count;
        data.isValid = true;
        success = true;
        Serial.printf("[SPS30] Sucesso! PM1.0: %.2f, PM2.5: %.2f, PM10: %.2f (Amostras: %d)\n",data.pm1_0, data.pm2_5, data.pm10_0, valid_count);
    } else {
        Serial.printf("[SPS30] Falha. Amostras insuficientes (%d/%d)\n", valid_count, cfg.min_valid_samples);
    }

    // --- FASE 4: DESLIGAR E DORMIR ---
    
    if (cfg.stop_measurement_after_read) {
        // Para o Fan
        error = sps30.stopMeasurement();
        if (error != 0) log_sps_error("StopMeasurement", error);
        
        delay(100); 
        
        // Entra em Deep Sleep (<50uA)
        error = sps30.sleep();
        if (error != 0) log_sps_error("Sleep", error);
        else Serial.println("[SPS30] Sensor dormindo.");
    }

    return success;
}