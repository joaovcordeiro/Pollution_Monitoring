#ifndef ADS1115_HANDLER_H
#define ADS1115_HANDLER_H

#include <Arduino.h>


typedef enum {
    ADS_CHANNEL_BATTERY = 0, // A0: Bateria Direta (via resistor 2.2k)
    ADS_CHANNEL_MICS_NH3    = 2, // A1: Sensor NH3
    ADS_CHANNEL_MICS_CO     = 1, // A2: Sensor CO
    ADS_CHANNEL_MICS_NO2    = 3, // A3: Sensor NO2
} ads_channel_t;


/**
 * @brief Inicializa o sensor ADS1115 com configurações de ganho seguro.
 * @param i2c_address Endereço I2C (Padrão 0x48).
 * @return true se comunicou com sucesso.
 */
bool ads1115_init(uint8_t i2c_address);

/**
 * @brief Lê um canal analógico com Oversampling (média de 32x) e Retry Logic.
 * * Se houver erro de I2C, tenta até 3 vezes antes de desistir.
 * Retorna -32768 em caso de falha total.
 * * @param channel O canal a ler.
 * @return Valor RAW (int16_t) ou código de erro.
 */
int16_t ads1115_read_robust(ads_channel_t channel);

/**
 * @brief Converte RAW para Volts baseado no ganho configurado (GAIN_TWOTHIRDS).
 */
float ads1115_convert_to_voltage(int16_t raw_adc_value);

/**
 * @brief Lê a tensão da bateria no pino A0.
 * Já considera que não há divisor resistivo (leitura direta).
 */
float ads1115_read_battery_voltage();

#endif // ADS1115_HANDLER_H