#ifndef ADS1115_HANDLER_H
#define ADS1115_HANDLER_H

#include <Arduino.h>


typedef enum {
    ADS_CHANNEL_MICS_NH3 = 0,
    ADS_CHANNEL_MICS_CO = 1,
    ADS_CHANNEL_MICS_NO2 = 2,
} ads_channel_t;


/**
 * @brief Inicializa o sensor ADS1115 no barramento I2C.
 * * Configura o ganho (PGA) e a taxa de dados.
 * @note PRESSUPÕE que Wire.begin() já foi chamado no setup() principal (main.cpp).
 *
 * @param i2c_address O endereço I2C do módulo ADS1115.
 * @return true se a inicialização for bem-sucedida, false caso contrário.
 */
bool ads1115_init(uint8_t i2c_address);

/**
 * @brief Lê um canal analógico usando "oversampling".
 *
 * Esta função lê o ADC 32 vezes em um loop rápido e retorna a média.
 * Isso filtra o ruído elétrico e fornece uma leitura estável,
 *
 * @param channel O canal a ser lido (ex: ADS_CHANNEL_MICS_NH3).
 * @return O valor RAW (bruto) de 16 bits (de -32768 a 32767) lido do ADC.
 */
int16_t ads1115_read_stable_raw_value(ads_channel_t channel);

/**
 * @brief Converte um valor RAW (bruto) de 16 bits para Volts (float).
 *
 * @param raw_adc_value O valor bruto retornado por ads1115_read_stable_raw_value.
 * @return A tensão calculada em Volts.
 */
float ads1115_convert_to_voltage(int16_t raw_adc_value);

#endif // ADS1115_HANDLER_H