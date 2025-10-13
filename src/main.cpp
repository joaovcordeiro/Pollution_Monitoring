#include "config.h"
#include "modules/PowerManager/power_manager.h"
#include "modules/SCD40/scd40_handler.h"
#include "modules/ADS1115/ads1115_handler.h"
#include "modules/MICS6814/mics6814_handler.h" 
#include "modules/DSM501A/dsm501a_handler.h"
#include "modules/ConnectivityHandler/comm_manager.h"

// Variáveis Globais para Dados dos Sensores
SCD40_Data scd40SensorData;
MICS6814_Data mics6814SensorData; 
DSM501A_Data dsm501aSensorData;
GPS_Data gpsLocationData;

unsigned long lastAttemptTime = 0;
const unsigned long ATTEMPT_INTERVAL_MS = 120000; 

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println(F("\n--- System Boot / Wake Up  ---"));
    Serial.printf("Timestamp (millis no início do setup): %lu\n", millis());
    Serial.printf("Free Heap at setup start: %u bytes\n", ESP.getFreeHeap());

    comm_manager_init_serial(); // Inicializa SerialAT para o modem
    setup_sensor_power(); // Configura o pino do MOSFET para controle de energia dos sensores
    Wire.begin(); 
    dsm501a_init();

    unsigned long cycleStartTime = millis(); // Para medir o tempo total do ciclo ativo
    
    // ETAPA 1: Ligar, Ler e Desligar Sensores
    Serial.println(F("Main: Powering ON sensors..."));
    // O SENSOR_STABILIZATION_DELAY_MS no config.h deve ser longo o suficiente
    // para o pré-aquecimento do MICS6814 e DSM501A 
    power_sensors_on(); 

    // Leitura do SCD40
    if (scd40_init()) {
        if (scd40_read_measurements(scd40SensorData) && scd40SensorData.isValid) {
            Serial.println(F("Main: SCD40 data read."));
            Serial.printf("SCD40: CO2:%.1f ppm, Temp:%.1f C, Hum:%.1f %%RH\n",
                             scd40SensorData.co2, scd40SensorData.temperature, scd40SensorData.humidity);
        } else { 
            scd40SensorData.isValid = false; 
            Serial.println(F("Main: Failed SCD40 read."));
        }
    } else { 
        scd40SensorData.isValid = false; 
        Serial.println(F("Main: Failed SCD40 init."));
    }

    // Leitura do MICS6814 via ADS1115
    if (ads1115_init(0x48)) { 
        if (mics6814_init()) { 
            if (mics6814_read_voltages(mics6814SensorData) && mics6814SensorData.isValid) {
                Serial.println(F("Main: MICS6814 voltages read. Calculating PPM..."));
                mics6814_calculate_ppm(mics6814SensorData);
            } else { 
                mics6814SensorData.isValid = false; 
                Serial.println(F("Main: Failed MICS6814 read."));
            }
        } else { 
            mics6814SensorData.isValid = false; 
            Serial.println(F("Main: Failed MICS6814 init."));
        }
    } else { 
        Serial.println(F("Main: ADS1115 initialization FAILED. Skipping MICS6814."));
        mics6814SensorData.isValid = false;
    }

    // Leitura do DSM501A
  
    Serial.println(F("Main: Starting DSM501A reading (this will take time)..."));
    if (dsm501a_read_data(dsm501aSensorData)) { 
        Serial.println(F("Main: DSM501A data (LOP ratio) calculated."));
        Serial.printf("DSM501A Data: PM2.5 LOP Ratio: %.2f %%\n", dsm501aSensorData.low_pulse_occupancy_ratio_pm25);
        Serial.printf("DSM501A Data: PM10 LOP Ratio: %.2f %%\n", dsm501aSensorData.low_pulse_occupancy_ratio_pm10);
    } else {
        Serial.println(F("Main: Failed to read DSM501A data."));
        dsm501aSensorData.isValid = false;
    }
    
    Serial.println(F("Main: Powering OFF sensors..."));
    power_sensors_off(); // Desliga o MOSFET (desconecta GND dos sensores)
    Serial.printf("Sensor reading phase took: %lu ms\n", millis() - cycleStartTime);
    Serial.printf("Free Heap after sensor readings: %u bytes\n", ESP.getFreeHeap());

    // ETAPA 2: Comunicação de Dados Completa
    bool dataTransmissionSuccessful = false;
    Serial.println(F("Main: Starting communication sequence..."));
    unsigned long commStartTime = millis();

    if (comm_manager_setup_modem_and_network()) {
        
        // --- NOVO: Obter localização GPS após o modem estar pronto ---
        Serial.println(F("Main: Attempting to get GPS location..."));
        // Esta chamada pode demorar até o timeout definido (padrão de 90s)
        if (comm_manager_get_gps_location(gpsLocationData, 300)) {
            Serial.println(F("Main: GPS location acquired successfully."));
        } else {
            Serial.println(F("Main: Failed to acquire GPS location. Proceeding without it."));
        }
        // A flag `gpsLocationData.isValid` controlará se os dados são enviados ou não.
        
        if (comm_manager_connect_gprs()) {
            if (comm_manager_connect_aws_iot()) {
                
                // --- ALTERADO: Passar os dados de GPS para a função de publicação ---
                if (comm_manager_publish_data(scd40SensorData, mics6814SensorData, dsm501aSensorData, gpsLocationData)) {
                    dataTransmissionSuccessful = true;
                } else {
                    Serial.println(F("Main: Failed to publish data to AWS."));
                }
            } else {
                Serial.println(F("Main: Failed to connect to AWS IoT."));
            }
        } else {
            Serial.println(F("Main: Failed to connect to GPRS."));
        }
    } else {
        Serial.println(F("Main: Failed to setup modem and network."));
    }
    
    Serial.println(F("Main: Finalizing communication sequence (disconnect/powerdown modem)..."));
    comm_manager_disconnect_and_powerdown_modem(); 
    Serial.printf("Communication phase took: %lu ms\n", millis() - commStartTime);
    Serial.printf("Free Heap after communication attempt: %u bytes\n", ESP.getFreeHeap());

    // ETAPA 3: Entrar em Deep Sleep
    if (dataTransmissionSuccessful) {
        Serial.println(F("Main: Data transmission cycle reported as successful."));
    } else {
        Serial.println(F("Main: Data transmission cycle had errors or was incomplete."));
    }
    
    Serial.printf("Total active cycle time: %lu ms\n", millis() - cycleStartTime);
    Serial.println(F("Main: Preparing to enter Deep Sleep..."));
    enter_deep_sleep(); 
}

void loop() {
    Serial.println(F("Main: Loop reached - this is unexpected! Forcing sleep."));
    delay(5000); 
    enter_deep_sleep();
}
