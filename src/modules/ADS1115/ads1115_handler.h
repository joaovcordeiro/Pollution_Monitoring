#ifndef ADS1115_HANDLER_H
#define ADS1115_HANDLER_H

#include <Arduino.h>
#include <Adafruit_ADS1X15.h> // Biblioteca da Adafruit para o ADS1115

// Estrutura para armazenar dados de uma leitura do ADS1115, se necessário
// struct ADS1115_Reading {
//     int16_t raw_value;
//     float voltage;
//     uint8_t channel;
//     bool isValid;
// };

/**
 * @brief Inicializa o ADC ADS1115.
 * Tenta comunicação e configura um ganho padrão.
 * @param i2c_address O endereço I2C do ADS1115 (padrão 0x48 se ADDR = GND).
 * @return true se a inicialização for bem-sucedida, false caso contrário.
 */
bool ads1115_init(uint8_t i2c_address = 0x48);

/**
 * @brief Define o ganho do Amplificador de Ganho Programável (PGA) do ADS1115.
 * @param gain O novo ganho a ser configurado (ex: GAIN_TWOTHIRDS, GAIN_ONE, etc.).
 */
void ads1115_set_gain(adsGain_t gain);

/**
 * @brief Lê um valor analógico de um canal single-ended do ADS1115.
 * @param channel O canal a ser lido (0, 1, 2 ou 3).
 * @return O valor ADC de 16 bits lido. Retorna INT16_MIN em caso de canal inválido.
 */
int16_t ads1115_read_adc_single_ended(uint8_t channel);

/**
 * @brief Converte a leitura bruta do ADC para tensão (em Volts).
 * Calcula a tensão baseada no valor ADC e no ganho ESPECIFICADO que foi usado
 * durante a aquisição daquele valor ADC.
 * @param adc_value A leitura bruta do ADC (valor de 16 bits).
 * @param gain_used_for_reading O ganho do PGA que foi ativo quando adc_value foi lido.
 * @return A tensão calculada em Volts.
 */
float ads1115_compute_voltage(int16_t adc_value, adsGain_t gain_used_for_reading);

// Se precisar de leituras diferenciais no futuro, adicione funções para elas:
// int16_t ads1115_read_adc_differential_0_1(); // Exemplo para AIN0 - AIN1

#endif // ADS1115_HANDLER_H
