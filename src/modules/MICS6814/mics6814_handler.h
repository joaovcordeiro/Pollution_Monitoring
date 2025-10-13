#ifndef MICS6814_HANDLER_H
#define MICS6814_HANDLER_H

#include <Arduino.h>
#include "modules/ADS1115/ads1115_handler.h"

#define MICS_CO_CHANNEL  0
#define MICS_NO2_CHANNEL 1
#define MICS_NH3_CHANNEL 2

#define MICS_CO_R0   (13.11)  
#define MICS_NO2_R0  (165.56)   
#define MICS_NH3_R0  (170.18) 

struct MICS6814_Data {
    float voltage_co;
    float voltage_no2;
    float voltage_nh3;
    float ppm_co;   
    float ppm_no2;  
    float ppm_nh3;  
    bool isValid;
};

bool mics6814_init();
bool mics6814_read_voltages(MICS6814_Data &data);

/**
 * @brief Calcula as concentrações de gás em PPM a partir das tensões lidas.
 * Esta função deve ser chamada APÓS mics6814_read_voltages().
 * @param data Referência para a estrutura MICS6814_Data, que já deve conter
 * as tensões e será preenchida com os valores de PPM.
 */
void mics6814_calculate_ppm(MICS6814_Data &data);

#endif // MICS6814_HANDLER_H