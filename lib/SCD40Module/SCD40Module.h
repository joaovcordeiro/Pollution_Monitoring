#ifndef SCD40MODULE_H
#define SCD40MODULE_H

#include <Wire.h>
#include "SensirionI2CScd4x.h"

class SCD40Module {
public:
    SCD40Module();
    void begin();  // Função para inicializar o sensor
    bool readMeasurements(float &co2, float &temperature, float &humidity);  // Agora retorna bool para indicar sucesso ou falha

private:
    SensirionI2CScd4x scd4x;  // Instância do sensor SCD40

    float calculateAverage(float *values, int count);  // Função privada para calcular a média das leituras
    
};

#endif