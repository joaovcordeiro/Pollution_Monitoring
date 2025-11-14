#ifndef MICS6814_HANDLER_H
#define MICS6814_HANDLER_H

#include <Arduino.h>
#include "modules/ADS1115/ads1115_handler.h" // Precisamos do nosso leitor de ADC

// Estrutura para armazenar os dados lidos
struct MICS6814_Data {
    // Valores de PPM (Partes por Milhão) calculados
    float ppm_co;
    float ppm_no2;
    float ppm_nh3;
    
    // Valores "brutos" (RAW) para depuração
    int16_t raw_co;
    int16_t raw_no2;
    int16_t raw_nh3;
    
    bool isValid; 
};

/**
 * @brief Inicializa o handler do MICS6814.
 *
 * @note Esta função *DEVE* ser chamada *DEPOIS* de ads1115_init().
 * @note Ela carrega os valores de R0 (calibração) definidos no arquivo .cpp.
 *
 * @param r0_co Valor de calibração (R0) para CO em ar limpo.
 * @param r0_no2 Valor de calibração (R0) para NO2 em ar limpo.
 * @param r0_nh3 Valor de calibração (R0) para NH3 em ar limpo.
 */
void mics6814_init(int16_t r0_co, int16_t r0_no2, int16_t r0_nh3);

/**
 * @brief CALIBRA o sensor para encontrar os valores de R0.
 *
 * @note VOCÊ DEVE RODAR ESTA FUNÇÃO UMA VEZ COM O SENSOR EM "AR LIMPO".
 * Ela é bloqueante e demora 10 segundos.
 *
 * @param out_r0_co Referência onde o valor R0 (CO) será salvo.
 * @param out_r0_no2 Referência onde o valor R0 (NO2) será salvo.
 * @param out_r0_nh3 Referência onde o valor R0 (NH3) será salvo.
 */
void mics6814_calibrate(int16_t& out_r0_co, int16_t& out_r0_no2, int16_t& out_r0_nh3);

/**
 * @brief Lê os valores atuais do sensor (Rs) e os converte para PPM.
 *
 * @note PRESSUPÕE que o sensor já foi aquecido (Warm-up) por 1-3 minutos
 * (controlado pelo PowerManager no main.cpp).
 *
 * @param data Referência para a estrutura MICS6814_Data onde os dados 
 * calculados (PPM) serão armazenados.
 * @return true se a leitura foi bem-sucedida, false caso contrário.
 */
bool mics6814_read_data(MICS6814_Data &data);

#endif // MICS6814_HANDLER_H