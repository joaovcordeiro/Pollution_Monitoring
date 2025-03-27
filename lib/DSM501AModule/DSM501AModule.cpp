#include "DSM501AModule.h"

DSM501AModule::DSM501AModule(uint8_t pinPM10, uint8_t pinPM25, unsigned long sampleTimeMs)
    : pinPM10(pinPM10), pinPM25(pinPM25), sampleTime(sampleTimeMs), lowPulseOccupancyPM10(0), lowPulseOccupancyPM25(0), startTime(0), concentrationPM10(-1), concentrationPM25(-1) {
}

void DSM501AModule::begin() {
    pinMode(pinPM10, INPUT);
    pinMode(pinPM25, INPUT);
    startTime = millis();
}

void DSM501AModule::calibrate() {
    Serial.println("Iniciando a calibração do DSM501A...");
    lowPulseOccupancyPM10 = 0;
    lowPulseOccupancyPM25 = 0;
    startTime = millis();

    // Loop bloqueante que aguarda o tempo de amostragem
    while (millis() - startTime < sampleTime) {
        // Acumula as durações de pulso baixo durante o tempo de amostragem
        lowPulseOccupancyPM10 += pulseIn(pinPM10, LOW, sampleTime);
        lowPulseOccupancyPM25 += pulseIn(pinPM25, LOW, sampleTime);
        delay(100); // Atraso para reduzir a carga no loop
    }

    // Calcula as concentrações baseadas nas durações acumuladas
    float ratioPM10 = lowPulseOccupancyPM10 / (sampleTime * 10.0);
    concentrationPM10 = 1.1 * pow(ratioPM10, 3) - 3.8 * pow(ratioPM10, 2) + 520 * ratioPM10 + 0.62;

    float ratioPM25 = lowPulseOccupancyPM25 / (sampleTime * 10.0);
    concentrationPM25 = 1.1 * pow(ratioPM25, 3) - 3.8 * pow(ratioPM25, 2) + 520 * ratioPM25 + 0.62;

    Serial.println("Calibração do DSM501A concluída!");
    Serial.print("Concentração de Partículas PM1.0 (µg/m³): ");
    Serial.println(concentrationPM10);
    Serial.print("Concentração de Partículas PM2.5 (µg/m³): ");
    Serial.println(concentrationPM25);
}

float DSM501AModule::getConcentrationPM10() const {
    return concentrationPM10;  // Retorna a última concentração de PM1.0
}

float DSM501AModule::getConcentrationPM25() const {
    return concentrationPM25;  // Retorna a última concentração de PM2.5
}
