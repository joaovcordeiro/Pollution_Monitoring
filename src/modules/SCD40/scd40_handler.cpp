// scd40_handler.cpp
#include "scd40_handler.h"

// Definição do endereço I2C padrão do SCD40
#define SCD40_I2C_ADDRESS 0x62

SensirionI2cScd4x scd4x; // Instância do objeto da biblioteca do sensor

// Variável para armazenar erros da biblioteca
uint16_t error;
char errorMessage[256];

bool scd40_init() {
    Wire.begin(); // Inicializa a interface I2C (SDA, SCL padrão do ESP32)
    Serial.println("SCD40: Initializing...");

    // Tenta encontrar o sensor
    scd4x.begin(Wire, SCD40_I2C_ADDRESS);

    // Para a medição periódica, se estiver ativa, para configurar o sensor
    error = scd4x.stopPeriodicMeasurement();
    if (error) {
        Serial.print("SCD40: Error trying to stop periodic measurement: ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        // Mesmo com erro, tentamos prosseguir, pode não estar medindo ainda.
    }

    // Inicia a medição periódica (leituras a cada ~5 segundos por padrão)
    error = scd4x.startPeriodicMeasurement();
    if (error) {
        Serial.print("SCD40: Error starting periodic measurement: ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        return false; 
    }

    Serial.println("SCD40: Periodic measurement started.");
    Serial.println("SCD40: Initialization successful.");
    return true;
}

bool scd40_read_measurements(SCD40_Data &data) {
    data.isValid = false; 
    bool dataReady = false;

    for (int attempt = 0; attempt < 6; attempt++) {
        error = scd4x.getDataReadyStatus(dataReady); 
        if (error) {
            Serial.print("SCD40: Error checking data ready status: ");
            errorToString(error, errorMessage, 256);
            Serial.println(errorMessage);
            return false; 
        }
        if (dataReady) {
            break; // Dados estão prontos, sair do loop de tentativas
        }
        if (Serial) { // Só imprime se a Serial estiver ativa
             //Serial.println("SCD40: Data not ready yet, waiting 1 second...");
        }
        delay(1000); // Espera 1 segundo antes de tentar novamente
    }

    if (!dataReady) {
        if (Serial) {
            Serial.println("SCD40: Timed out waiting for data to become available.");
        }
        return false; // Nenhum dado novo disponível após as tentativas
    }

    uint16_t co2_ppm_uint; 
    float temperature_float;
    float humidity_float;

    error = scd4x.readMeasurement(co2_ppm_uint, temperature_float, humidity_float);
    if (error) {
        Serial.print("SCD40: Error reading measurement: ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        return false; 
    } 
    
    // O valor 0 ppm é fisicamente improvável para CO2 atmosférico.
    if (co2_ppm_uint == 0) { 
        Serial.println("SCD40: Warning - Invalid CO2 reading (0 ppm). Sensor might still be stabilizing or error in reading.");
        return false;
    }

    // Atribui os valores lidos à estrutura de dados, convertendo CO2 para float.
    data.co2 = static_cast<float>(co2_ppm_uint);
    data.temperature = temperature_float;
    data.humidity = humidity_float;
    data.isValid = true; 

    return true;
}