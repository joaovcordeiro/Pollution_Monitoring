#include "SCD40Module.h"

SCD40Module::SCD40Module() {
}

void SCD40Module::begin() {
    Wire.begin();
    scd4x.begin(Wire);

    uint16_t error;
    char errorMessage[256];

    error = scd4x.startPeriodicMeasurement();
    if (error) {
        Serial.print("Error trying to start measurement: ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);

        // Tenta continuar mesmo após falha inicial
        Serial.println("Tentando continuar mesmo com falha na inicialização...");
    } else {
        Serial.println("SCD40 inicializado com sucesso.");
    }

    delay(5000); // Tempo adicional para permitir estabilização do sensor
}

bool SCD40Module::readMeasurements(float &co2, float &temperature, float &humidity) {
    const int numReadings = 10; // Número de leituras para média
    float co2Readings[numReadings];
    float tempReadings[numReadings];
    float humidityReadings[numReadings];

    int validReadings = 0;
    int maxAttempts = 10; // Máximo de tentativas para obter leituras válidas

    for (int attempt = 0; attempt < maxAttempts; attempt++) {
        uint16_t error;
        uint16_t co2_raw;
        char errorMessage[256];

        // Tenta ler o sensor
        error = scd4x.readMeasurement(co2_raw, temperature, humidity);
        if (error) {
            Serial.print("Erro ao tentar realizar a leitura: ");
            errorToString(error, errorMessage, 256);
            Serial.println(errorMessage);
            delay(2000); 

            if (validReadings >= 3) {
                
                break;
            }
        } else if (co2_raw != 0) {
            // Armazena leituras válidas
            co2Readings[validReadings] = static_cast<float>(co2_raw);
            tempReadings[validReadings] = temperature;
            humidityReadings[validReadings] = humidity;
            validReadings++;
            Serial.println("Leitura válida obtida.");
            delay(200); 

            if (validReadings >= 3) {
                
                break;
            }
        } else {
            Serial.println("Leitura inválida detectada, ignorando.");
        }
    }

    // Verifica se obteve leituras suficientes para calcular a média
    if (validReadings < 3) {
        Serial.println("Falha em obter leituras suficientes do SCD40.");
        co2 = 0.0;
        temperature = 0.0;
        humidity = 0.0;
        return false;
    }

    // Calcula a média das leituras válidas
    co2 = calculateAverage(co2Readings, validReadings);
    temperature = calculateAverage(tempReadings, validReadings);
    humidity = calculateAverage(humidityReadings, validReadings);


    return true;
}

float SCD40Module::calculateAverage(float *values, int count) {
    float sum = 0.0;
    for (int i = 0; i < count; i++) {
        sum += values[i];
    }
    return sum / count;
}
