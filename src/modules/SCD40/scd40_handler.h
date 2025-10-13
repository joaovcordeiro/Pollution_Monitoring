// scd40_handler.h
#ifndef SCD40_HANDLER_H
#define SCD40_HANDLER_H

#include <Arduino.h>
#include <SensirionI2cScd4x.h> // Biblioteca do sensor SCD4X

// Estrutura para armazenar os dados lidos do sensor
struct SCD40_Data {
    float co2;
    float temperature;
    float humidity;
    bool isValid; // Flag para indicar se os dados são válidos
};

/**
 * @brief Inicializa o sensor SCD40.
 * * Tenta estabelecer comunicação com o sensor e inicia o modo de medição periódica.
 * @return true se a inicialização for bem-sucedida, false caso contrário.
 */
bool scd40_init();

/**
 * @brief Realiza a leitura dos dados do sensor SCD40.
 * * Verifica se novos dados estão disponíveis e, em caso afirmativo, lê os valores
 * de CO2, temperatura e umidade.
 * @param data Referência para a estrutura SCD40_Data onde os dados serão armazenados.
 * @return true se novos dados foram lidos com sucesso, false caso contrário.
 */
bool scd40_read_measurements(SCD40_Data &data);

#endif // SCD40_HANDLER_H