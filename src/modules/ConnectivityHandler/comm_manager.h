#ifndef COMM_MANAGER_H
#define COMM_MANAGER_H

#include <Arduino.h>
#include "config.h" // Para credenciais e constantes de rede/AWS
#include "modules/SCD40/scd40_handler.h" // Para o tipo SCD40_Data
#include "modules/MICS6814/mics6814_handler.h"
#include "modules/DSM501A/dsm501a_handler.h"

struct GPS_Data {
    float latitude = 0.0f;
    float longitude = 0.0f;
    float hdop = 99.0f;
    int satellites = 0;
    bool isValid = false;
};

/**
 * @brief Inicializa a comunicação serial com o modem.
 * Deve ser chamado uma vez no setup, após Serial.begin().
 */
void comm_manager_init_serial();

/**
 * @brief Realiza a sequência completa para ligar, inicializar o modem e conectar à rede celular.
 * @return true se o modem foi configurado e conectado à rede com sucesso, false caso contrário.
 */
bool comm_manager_setup_modem_and_network();

/**
 * @brief Ativa o GPS do modem, tenta obter a localização e o desliga.
 * @param gps_data Referência para a estrutura onde os dados de GPS serão armazenados.
 * @param timeout_seconds Tempo máximo em segundos para tentar obter um fix.
 * @return true se a localização foi obtida com sucesso, false caso contrário.
 */
bool comm_manager_get_gps_location(GPS_Data& gps_data, uint16_t timeout_seconds = 90);


/**
 * @brief Conecta à rede GPRS.
 * @return true se conectado com sucesso, false caso contrário.
 */
bool comm_manager_connect_gprs();

/**
 * @brief Conecta ao AWS IoT Core via MQTTs.
 * Pressupõe que a conexão GPRS já está ativa.
 * Configura os certificados SSL/TLS.
 * @return true se conectado com sucesso, false caso contrário.
 */
bool comm_manager_connect_aws_iot();

/**
 * @brief Publica os dados dos sensores para o AWS IoT Core.
 * @param scd40_data Referência para a estrutura com os dados do SCD40.
 * // Adicionar parâmetros para outros sensores conforme necessário
 * @return true se a publicação for bem-sucedida, false caso contrário.
 */
bool comm_manager_publish_data(const SCD40_Data& scd40_data , const MICS6814_Data& mics_data, const DSM501A_Data& dsm_data, const GPS_Data& gps_data);

/**
 * @brief Desconecta do MQTT, GPRS e desliga o modem.
 */
void comm_manager_disconnect_and_powerdown_modem();

/**
 * @brief Função de callback para mensagens MQTT recebidas.
 * (Implementação pode ser simples se não esperar comandos).
 */
void comm_manager_mqtt_callback(char* topic, byte* payload, unsigned int length);

#endif // COMM_MANAGER_H