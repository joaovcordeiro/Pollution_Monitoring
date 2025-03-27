#include "MICS6814Module.h"

MICS6814Module::MICS6814Module(ADS1115 &ads) : _ads(ads) {}

void MICS6814Module::begin() {
    _ads.begin(); // Inicializa o ADS1115
    Serial.println("MICS6814 iniciado com ADS1115.");
}

void MICS6814Module::calibrate() {
    uint8_t seconds = 3;  // Reduz o número de segundos para calibração
    uint8_t delta = 15;   // Aumenta a tolerância para acelerar a estabilização
    uint16_t bufferNH3[seconds];
    uint16_t bufferCO[seconds];
    uint16_t bufferNO2[seconds];
    uint8_t pntrNH3 = 0, pntrCO = 0, pntrNO2 = 0;
    uint16_t fltSumNH3 = 0, fltSumCO = 0, fltSumNO2 = 0;
    bool isStableNH3 = false, isStableCO = false, isStableNO2 = false;

    for (int i = 0; i < seconds; ++i) {
        bufferNH3[i] = 0;
        bufferCO[i] = 0;
        bufferNO2[i] = 0;
    }

    Serial.println("Iniciando a calibração rápida...");

    do {
        delay(500);  // Intervalo entre as leituras ajustado para 500ms

        uint16_t curNH3 = _ads.readADC(0); // Canal 0 para NH3
        uint16_t curCO = _ads.readADC(1);  // Canal 1 para CO
        uint16_t curNO2 = _ads.readADC(2); // Canal 2 para NO2

        fltSumNH3 = fltSumNH3 + curNH3 - bufferNH3[pntrNH3];
        fltSumCO = fltSumCO + curCO - bufferCO[pntrCO];
        fltSumNO2 = fltSumNO2 + curNO2 - bufferNO2[pntrNO2];

        bufferNH3[pntrNH3] = curNH3;
        bufferCO[pntrCO] = curCO;
        bufferNO2[pntrNO2] = curNO2;

        isStableNH3 = abs(fltSumNH3 / seconds - curNH3) < delta;
        isStableCO = abs(fltSumCO / seconds - curCO) < delta;
        isStableNO2 = abs(fltSumNO2 / seconds - curNO2) < delta;

        Serial.print("NH3: ");
        Serial.print(curNH3);
        Serial.print(", CO: ");
        Serial.print(curCO);
        Serial.print(", NO2: ");
        Serial.print(curNO2);
        Serial.println();

        pntrNH3 = (pntrNH3 + 1) % seconds;
        pntrCO = (pntrCO + 1) % seconds;
        pntrNO2 = (pntrNO2 + 1) % seconds;

    } while (!isStableNH3 || !isStableCO || !isStableNO2);

    Serial.println("Calibração rápida concluída!");

    _baseNH3 = fltSumNH3 / seconds;
    _baseCO = fltSumCO / seconds;
    _baseNO2 = fltSumNO2 / seconds;
}

void MICS6814Module::loadCalibrationData(uint16_t baseNH3, uint16_t baseCO, uint16_t baseNO2) {
    _baseNH3 = baseNH3;
    _baseCO = baseCO;
    _baseNO2 = baseNO2;
}

float MICS6814Module::measure(gas_t gas) {
    float ratio;
    float c = 0;

    switch (gas) {
        case CO:
            ratio = getCurrentRatio(CH_CO);
            c = pow(ratio, -1.179) * 4.385;
            break;
        case NO2:
            ratio = getCurrentRatio(CH_NO2);
            c = pow(ratio, 1.007) / 6.855;
            break;
        case NH3:
            ratio = getCurrentRatio(CH_NH3);
            c = pow(ratio, -1.67) / 1.47;
            break;
    }

    return isnan(c) ? -1 : c;
}

uint16_t MICS6814Module::getResistance(channel_t channel) const {
    uint16_t resistance = 0;

    switch (channel) {
        case CH_CO:
            resistance = _ads.readADC(1); // Canal 1 para CO
            break;
        case CH_NO2:
            resistance = _ads.readADC(2); // Canal 2 para NO2
            break;
        case CH_NH3:
            resistance = _ads.readADC(0); // Canal 0 para NH3
            break;
    }

    return resistance;
}

uint16_t MICS6814Module::getBaseResistance(channel_t channel) const {
    switch (channel) {
        case CH_NH3:
            return _baseNH3;
        case CH_CO:
            return _baseCO;
        case CH_NO2:
            return _baseNO2;
    }

    return 0;
}

float MICS6814Module::getCurrentRatio(channel_t channel) const {
    float baseResistance = (float)getBaseResistance(channel);
    float resistance = (float)getResistance(channel);

    return resistance / baseResistance * (1023.0 - baseResistance) / (1023.0 - resistance);
}
