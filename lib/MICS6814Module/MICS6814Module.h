#ifndef MICS6814MODULE_H
#define MICS6814MODULE_H

#include <Arduino.h>
#include <ADS1X15.h> // Biblioteca correta para o ADS1115

// Definição dos tipos de gases
typedef enum {
    CH_CO,
    CH_NO2,
    CH_NH3
} channel_t;

typedef enum {
    CO,
    NO2,
    NH3
} gas_t;

class MICS6814Module {
public:
    MICS6814Module(ADS1115 &ads);
    
    void begin(); // Inicializa o sensor e o ADC
    void calibrate(); // Calibra o sensor
    void loadCalibrationData(uint16_t baseNH3, uint16_t baseCO, uint16_t baseNO2); // Carrega dados de calibração
    float measure(gas_t gas); // Mede a concentração de um gás específico

private:
    ADS1115 &_ads;
    uint16_t _baseNH3;
    uint16_t _baseCO;
    uint16_t _baseNO2;

    uint16_t getResistance(channel_t channel) const; // Obtém a resistência do canal especificado
    uint16_t getBaseResistance(channel_t channel) const; // Obtém a resistência de base do canal especificado
    float getCurrentRatio(channel_t channel) const; // Obtém a razão corrente/base de resistência
};

#endif // MICS6814MODULE_H
