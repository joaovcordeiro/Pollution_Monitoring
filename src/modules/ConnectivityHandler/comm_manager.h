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
    float speed_kph = 99.0f;
    float altitude = 0.0f;
    int satellites_visible = 0;
    int satellites_used = 0;
    float accuracy = 0.0f;
    bool isValid = false;
};

/**
 * @brief Inicializa a(s) porta(s) serial e os pinos de controle de hardware
 * para comunicação com o modem.
 *
 * @note Esta função deve ser chamada apenas UMA VEZ no início do setup() global.
 * Ela configura o pino PWRKEY e inicializa a SerialAT.
 * O baud rate está definido como 115200, que é o padrão de fábrica
 * para o SIM7000G, garantindo maior robustez e portabilidade.
 */
void init_serial();

/**
 * @brief Executa o ciclo de comunicação completo:
 * 1. Liga o modem e conecta à rede celular.
 * 2. Conecta GPRS e sincroniza o NTP (para o relógio e para o A-GPS).
 * 3. Obtém a localização GPS (agora rápida, graças ao NTP).
 * 4. Conecta ao AWS IoT (MQTT).
 * 5. Publica os dados dos sensores.
 * 6. Desconecta e desliga o modem de forma segura.
 *
 * @param scd_data Dados do sensor SCD40.
 * @param mics_data Dados do sensor MICS6814.
 * @param dsm_data Dados do sensor DSM501A.
 * @param out_gps_data Referência para a struct GPS_Data, que será PREENCHIDA
 * por esta função.
 * @return true se a PUBLICAÇÃO dos dados foi bem-sucedida, false caso contrário.
 */
bool perform_communication_cycle(
    const SCD40_Data& scd_data,
    const MICS6814_Data& mics_data,
    const DSM501A_Data& dsm_data,
    GPS_Data& out_gps_data // Passado por referência para ser preenchido
);



#endif // COMM_MANAGER_H