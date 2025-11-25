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


void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println(F("\n--- System Boot / Wake Up  ---"));

    init_serial(); // Inicializa SerialAT para o modem
    Wire.begin(); 

    setup_sensor_power(); // Configura o pino do MOSFET para controle de energia dos sensores

    // // ETAPA 1: Ligar, Ler e Desligar Sensores
    Serial.println(F("Main: Powering ON sensors..."));
    // // para o pré-aquecimento do MICS6814 e SPS30 
    power_sensors_on(); 

    if (!ads1115_init(0x48)) { // 0x48 é o endereço (ADDR no GND)
        Serial.println(F("Main: FALHA CRÍTICA - ADS1115 não encontrado."));
    }

    if (!sps30_init()) {
    Serial.println(F("Main: FALHA CRÍTICA - SPS30 não encontrado."));
    }

    //inicializa o handler do MICS com os valores de calibração
    mics6814_init(CALIBRATED_R0_CO, CALIBRATED_R0_NO2, CALIBRATED_R0_NH3);

        // // O SENSOR_STABILIZATION_DELAY_MS no config.h deve ser longo o suficiente
    Serial.println(F("Main: Aguardando 1 minuto (Warm-up) para MICS e SPS30..."));
    delay(SENSOR_STABILIZATION_DELAY_MS);

    // // Leitura do SCD40
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

    Serial.println(F("Main: Lendo SPS30 (poeira)..."));
    if (!sps30_read_measurements(sps30SensorData)) {
        Serial.println(F("Main: Falha ao ler dados do SPS30."));
    }
    Serial.printf("Main: SPS30 (isValid: %d) -> PM2.5: %.2f µg/m³\n", sps30SensorData.isValid, sps30SensorData.pm2_5);

    // Leitura do MICS6814 via ADS1115
    Serial.println(F("Main: Lendo MICS6814..."));
    // As funções antigas (read_voltages, calculate_ppm) foram substituídas
    // por esta única chamada:
    if (!mics6814_read_data(mics6814SensorData)) {
        Serial.println(F("Main: Falha ao ler dados do MICS6814."));
    }
    Serial.printf("Main: MICS (isValid: %d) -> CO: %.2f ppm\n", mics6814SensorData.isValid, mics6814SensorData.ppm_co);

    
    Serial.println(F("Main: Powering OFF sensors..."));
    delay(5000);
    power_sensors_off(); // Desliga o MOSFET (desconecta GND dos sensores)

    Serial.println(F("Main: Lendo status da bateria..."));
    get_battery_status(batteryData);
 

    // ETAPA 2: Comunicação de Dados Completa
    Serial.println(F("Main: Starting full communication cycle..."));
    unsigned long commStartTime = millis();

    bool dataTransmissionSuccessful = perform_communication_cycle(
        scd40SensorData,
        mics6814SensorData,
        sps30SensorData,
        batteryData,
        gpsLocationData 
    );

    Serial.printf("Communication phase took: %lu ms\n", millis() - commStartTime);

    // ETAPA 3: Entrar em Deep Sleep
    if (dataTransmissionSuccessful) {
        Serial.println(F("Main: Data transmission cycle reported as successful."));
    } else {
        Serial.println(F("Main: Data transmission cycle had errors or was incomplete."));
    }
    
    Serial.println(F("Main: Preparing to enter Deep Sleep..."));
    enter_deep_sleep(); 
}

void loop() {
    // Esta lógica é uma salvaguarda. Em um projeto de deep sleep,
    // o loop() nunca deve ser alcançado após o setup().
    Serial.println(F("Main: Loop reached - this is unexpected! Forcing sleep."));
    delay(5000); 
    enter_deep_sleep();
}
