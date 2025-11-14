#include "scd40_handler.h"

// Definição do endereço I2C padrão do SCD40
#define SCD40_I2C_ADDRESS 0x62

SensirionI2cScd4x scd4x; 

bool scd40_init() {

    uint16_t error;
    char errorMessage[256];

    Serial.println("SCD40: Initializing...");

    scd4x.begin(Wire, SCD40_I2C_ADDRESS);

    error = scd4x.stopPeriodicMeasurement();
    if (error) {
        Serial.print("SCD40: AVISO - stopPeriodicMeasurement() falhou. ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        // Nota: Esta falha pode ser a primeira indicação de que o sensor
        // não está conectado. Continuamos mesmo assim, pois o start()
        // será a verificação definitiva.
    }

    delay(500);

    // Inicia a medição periódica (leituras a cada ~5 segundos por padrão)
    error = scd4x.startPeriodicMeasurement();
    if (error) {
        Serial.print("SCD40: FALHA CRÍTICA - startPeriodicMeasurement() falhou: ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        return false; // Se não conseguirmos iniciar, o sensor está inacessível
    }

    Serial.println("SCD40: Medição periódica iniciada.");
    Serial.println("SCD40: Inicialização bem-sucedida.");
    return true;
}

bool scd40_read_measurements(SCD40_Data &data) {

    uint16_t error;
    char errorMessage[256];

    data.isValid = false; 
    bool dataReady = false;

    Serial.println("SCD40: Aguardando o flag 'Data Ready' (timeout max 5s)...");

    for (int attempt = 0; attempt < 6; attempt++) {
        error = scd4x.getDataReadyStatus(dataReady); 
        if (error) {
            Serial.print("SCD40: FALHA CRÍTICA - Erro ao checar status (getDataReadyStatus): ");
            errorToString(error, errorMessage, 256);
            Serial.println(errorMessage);
            return false; // Falha de comunicação I2C
        }

        if (dataReady) {
            Serial.println("SCD40: Flag 'Data Ready' recebido.");
            break; 
        }

        if (attempt < 5) {
            Serial.println("SCD40: ...dados não estão prontos. Aguardando 1s.");
            delay(1000);
        }
    }

    if (!dataReady) {
        if (Serial) {
            Serial.println("SCD40: FALHA - Timeout. Sensor não disponibilizou dados.");
        }
        return false; 
    }

    uint16_t co2_ppm_uint; 
    float temperature_float;
    float humidity_float;

    error = scd4x.readMeasurement(co2_ppm_uint, temperature_float, humidity_float);
    if (error) {
        Serial.print("SCD40: FALHA CRÍTICA - Erro ao ler a medição (readMeasurement):");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        return false; 
    } 
    
    // O valor 0 ppm é fisicamente improvável (atmosfera = ~420ppm).
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