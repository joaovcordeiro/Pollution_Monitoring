#include "comm_manager.h"
#include "config.h"

// --- Bibliotecas de Comunicação ---
#include <TinyGsmClient.h>
#include <TinyGsmClientSIM7000.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>      
#include <SSLClientESP32.h>    
#include <time.h>     
#include <sys/time.h>  

// --- Definições e Variáveis Estáticas do Módulo ---
#define SerialMon Serial        // Serial para monitoramento/debug
#define SerialAT  Serial1       // Serial para comunicação AT com o modem

// Objetos de comunicação (estáticos para este módulo)
static TinyGsm modem(SerialAT);
static TinyGsmClient base_client(modem, 0);
static SSLClientESP32 ssl_client(&base_client);
static PubSubClient mqtt_client(ssl_client);

// --- Implementação das Funções ---

/**
 * @brief Envia o pulso de energia para o pino PWRKEY para ligar o modem.
 * @note Esta função APENAS envia o pulso, ela não espera o modem 
 * inicializar. A espera deve ser feita pela função modem.restart() 
 * ou modem.begin() que ativamente verifica a resposta AT.
 */
static void modemPowerOn() {
    SerialMon.println(F("CommManager: Enviando pulso de 'power on' (1s)..."));

    digitalWrite(MODEM_PWRKEY_PIN, LOW);
    delay(100); 
    digitalWrite(MODEM_PWRKEY_PIN, HIGH);
    delay(1000); 
    digitalWrite(MODEM_PWRKEY_PIN, LOW);

    SerialMon.println(F("CommManager: Pulso de 'power on' enviado..."));
}

/**
 * @brief Envia o pulso de energia para o pino PWRKEY para desligar o modem.
 * @note A maioria dos modems SIMCOM requer um pulso um pouco mais longo 
 * para desligar do que para ligar.
 */
static void modemPowerOff() {
    SerialMon.println(F("CommManager: Enviando pulso de 'power off' (1s)..."));

    digitalWrite(MODEM_PWRKEY_PIN, LOW);
    delay(100);
    digitalWrite(MODEM_PWRKEY_PIN, HIGH);
    delay(1000); 
    digitalWrite(MODEM_PWRKEY_PIN, LOW);

    SerialMon.println(F("CommManager: Pulso de 'power off' enviado."));
}

void init_serial() {
    
    pinMode(MODEM_PWRKEY_PIN, OUTPUT);
    digitalWrite(MODEM_PWRKEY_PIN, LOW);

    SerialAT.begin(57600, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);

    SerialMon.println(F("CommManager: SerialAT (57600) e pinos de controle inicializados."));
}

/**
 * @brief (Função Privada) Liga o modem, reinicia e aguarda o registro na rede.
 * * @note Esta é a função principal de inicialização do modem, chamada a cada 
 * despertar (wake-up) do deep sleep.
 * @note Ela assume que init_serial() JÁ FOI CHAMADA
 * uma vez no setup() global (em main.cpp) para configurar os pinos e a SerialAT.
 * * @return true se o modem estiver ligado e registrado na rede, false caso contrário.
 */
static bool setup_modem_and_network() {
    SerialMon.println(F("--- Iniciando Sequência de Modem ---"));

    modemPowerOn();

    SerialMon.println(F("CommManager: Reiniciando modem (TinyGSM) e aguardando boot..."));

    if (!modem.restart()) {
        SerialMon.println(F("CommManager: Falha ao reiniciar modem (não respondeu aos comandos AT)!"));
        
        modemPowerOff(); 
        return false;
    }

    String modemInfo = modem.getModemInfo();
    SerialMon.print(F("CommManager: Informação do Modem: "));
    SerialMon.println(modemInfo);

    SerialMon.println(F("CommManager: Aguardando registro na rede (max 3 min)..."));
    if (!modem.waitForNetwork(180000L)) { 
        SerialMon.println(F("CommManager: Falha ao registrar na rede celular."));
        modemPowerOff();
        return false;
    }
    SerialMon.println(F("CommManager: Rede celular registrada."));
    SerialMon.print(F("CommManager: Qualidade do Sinal (CSQ 0-31): "));
    SerialMon.println(modem.getSignalQuality());

    return true;
}

/**
 * @brief (Função Privada) Tenta obter uma localização GPS válida do modem.
    // 5. Desliga fisicamente o modem
 *
 * Esta função liga o GPS, faz um loop de "polling" (sondagem) por um
 * período de timeout e tenta obter um "fix" válido.
 *
 * @note Ela assume que o NTP já foi sincronizado (em uma etapa anterior)
 * para permitir um "Hot Start" rápido (TTFF de ~30s).
 *
 * @param gps_data Referência para a struct GPS_Data que será preenchida.
 * @param timeout_seconds Duração máxima em segundos para tentar obter o "fix".
 * @return true se um "fix" válido foi obtido, false caso contrário.
 */
bool get_gps_location(GPS_Data& gps_data, uint16_t timeout_seconds) {
    gps_data.isValid = false;
    Serial.println(F("CommManager: Habilitando GPS ..."));

    modem.sendAT(F("+SGPIO=0,4,1,1"));
    if (modem.waitResponse() != 1) {
        SerialMon.println(F("CommManager: AVISO - Comando SGPIO (ligar antena) falhou."));
    }

    SerialMon.println(F("CommManager: Habilitando GPS (Etapa 2: Rádio)..."));
    if (!modem.enableGPS()) {
        SerialMon.println(F("CommManager: Falha ao ligar o módulo GPS (AT+CGNSPWR=1)."));
        // Se a habilitação do rádio falhar, desliga a antena
        modem.sendAT(F("+SGPIO=0,4,1,0")); 
        modem.waitResponse();
        return false;
    }

    SerialMon.print(F("CommManager: GPS habilitado. Aguardando 'fix' (timeout: "));

    SerialMon.print(timeout_seconds);

    SerialMon.println(F("s)..."));

    unsigned long start_time = millis();
    bool got_fix = false;

    while (millis() - start_time < (unsigned long)timeout_seconds * 1000) {

        // 3. Pede os dados (TinyGSM envia AT+CGNSINF e faz o parse)
        if (modem.getGPS(&gps_data.latitude, &gps_data.longitude, 
                         &gps_data.speed_kph, &gps_data.altitude, 
                         &gps_data.satellites_visible, &gps_data.satellites_used, 
                         &gps_data.accuracy)) {

            // A função getGPS() pode retornar true mas com dados 0.0 se não houver fix.
            if (gps_data.latitude != 0.00 ) {
                SerialMon.println(F("--- FIX VÁLIDO OBTIDO! ---"));
                SerialMon.printf("Lat: %s, Lon: %s, Precisão: %s, Satélites Visiveis: %d, Satélites Utilizados: %d\n",
                                 String(gps_data.latitude, 6).c_str(), 
                                 String(gps_data.longitude, 6).c_str(),
                                 String(gps_data.accuracy, 2).c_str(),
                                 gps_data.satellites_visible, gps_data.satellites_used);
                                 
                gps_data.isValid = true;
                got_fix = true;
                break; // Sucesso, sai do loop
            }
        }
        
        SerialMon.print(F(".")); // Imprime um ponto a cada tentativa
        delay(5000); // Tenta a cada 5 segundos
    }

    if (!got_fix) {
        Serial.println(F("\nTimeout! Não foi possível obter um fix de GPS."));
    }

    SerialMon.println(F("CommManager: Desabilitando o GPS..."));
    modem.disableGPS();

    return gps_data.isValid;
}


/**
 * @brief (Função Privada) Estabelece a conexão GPRS (contexto PDP) usando
 * as credenciais do config.
 *
 * @note Esta função é bloqueante e pode levar algum tempo (até o 
 * timeout interno da TinyGSM) para conectar ou falhar.
 *
 * @return true se a conexão GPRS for estabelecida, false caso contrário.
 */
static bool connect_gprs() {
    SerialMon.print(F("CommManager: Conectando ao GPRS (APN: "));
    SerialMon.print(APN);
    SerialMon.println(F(")..."));

    if (!modem.gprsConnect(APN, GPRS_USER, GPRS_PASS)) {
        SerialMon.println(F("CommManager: Falha na conexão GPRS."));
        return false;
    }

    SerialMon.println(F("CommManager: GPRS conectado com sucesso."));
    SerialMon.print(F("CommManager: Endereço IP Local: "));
    SerialMon.println(modem.getLocalIP());
    return true;
}

/**
 * @brief (Função Privada/Callback) Chamada pela biblioteca PubSubClient
 * sempre que uma mensagem é recebida em um tópico assinado.
 *
 * @note Esta função é essencial para qualquer funcionalidade "nuvem-para-dispositivo",
 * como atualizações do AWS Device Shadow, comandos de reboot/OTA ou
 * reconfiguração remota.
 *
 * @param topic O tópico MQTT no qual a mensagem foi recebida.
 * @param payload O conteúdo (binário) da mensagem.
 * @param len O comprimento (em bytes) do payload.
 */
static void mqtt_callback(char* topic, byte* payload, unsigned int len) {
    SerialMon.print(F("CommManager: Mensagem recebida ["));
    SerialMon.print(topic);
    SerialMon.print(F("]: "));


    char msg_buffer[len + 1];

    memcpy(msg_buffer, payload, len);

    msg_buffer[len] = '\0';

    SerialMon.println(msg_buffer);
}

static bool synchronize_time_with_ntp() {
    SerialMon.println(F("CommManager: Sincronizando NTP ..."));

    if (!modem.NTPServerSync("pool.ntp.org", 0)) {
        SerialMon.println(F("CommManager: AVISO - Comando NTPServerSync falhou."));
    } else {
        SerialMon.println(F("CommManager: Comando NTPServerSync enviado."));
    }

    int ntp_year = 0, ntp_month = 0, ntp_day = 0;
    int ntp_hour = 0, ntp_min = 0, ntp_sec = 0;
    float ntp_timezone = 0.0f;

    for (int8_t i = 5; i; i--) {
        SerialMon.printf("CommManager: Tentando ler a hora do modem (tentativa %d/5)...\n", 6 - i);
        
        if (modem.getNetworkTime(&ntp_year, &ntp_month, &ntp_day, &ntp_hour,
                                 &ntp_min, &ntp_sec, &ntp_timezone)) {
            
            SerialMon.println(F("============================================="));
            SerialMon.println(F("CommManager: SUCESSO - modem.getNetworkTime()"));
            SerialMon.printf("  Data: %d-%02d-%02d\n", ntp_year, ntp_month, ntp_day);
            SerialMon.printf("  Hora (lida do modem): %02d:%02d:%02d\n", ntp_hour, ntp_min, ntp_sec);
            SerialMon.printf("  FUSO HORÁRIO (Timezone) reportado: %f (quartos de hora)\n", ntp_timezone);
            SerialMon.println(F("============================================="));

            struct tm timeinfo = {0};
            timeinfo.tm_year = ntp_year - 1900; 
            timeinfo.tm_mon = ntp_month - 1;    
            timeinfo.tm_mday = ntp_day;
            timeinfo.tm_hour = ntp_hour;
            timeinfo.tm_min = ntp_min;
            timeinfo.tm_sec = ntp_sec;

            time_t epoch_time_lida = mktime(&timeinfo);
            
            long timezone_seconds = (long)(ntp_timezone * 15.0f * 60.0f);
            time_t epoch_time_utc = epoch_time_lida - timezone_seconds; 

            struct timeval tv;
            tv.tv_sec = epoch_time_utc; 
            tv.tv_usec = 0;
            settimeofday(&tv, NULL); 

            SerialMon.println(F("CommManager: Relógio interno (RTC) do ESP32 sincronizado para UTC!"));
            
            return true; 

        } else {
            SerialMon.println(F("CommManager: Falha ao ler a hora... retentando em 5s."));
            delay(5000L); 
        }
    }

    SerialMon.println(F("CommManager: AVISO - Falha ao obter a hora do modem após 5 tentativas."));
    return false;
}


/**
 * @brief (Função Privada) Configura o cliente SSL e conecta ao AWS IoT via MQTT.
 *
 * Esta função carrega os certificados (CA, Certificado, Chave Privada)
 * na instância do SSLClient e, em seguida, usa o PubSubClient para
 * estabelecer a conexão MQTT segura (porta 8883) com o endpoint da AWS.
 *
 * Inclui uma lógica de 5 retentativas com 5s de espera em caso de falha.
 *
 * @return true se a conexão MQTT for estabelecida, false caso contrário.
 */
static bool connect_aws_iot() {

    // Log de Heap: Para depurar falhas de alocação de memória SSL
    SerialMon.printf("CommManager: Free Heap antes da configuração SSL: %u\n", ESP.getFreeHeap());
    SerialMon.println(F("CommManager: Configurando SSL/TLS para o AWS IoT..."));

    // 1. Configura os certificados para o cliente SSL
    ssl_client.setCACert(AWS_IOT_ROOT_CA);
    ssl_client.setCertificate(AWS_CERT_CRT);
    ssl_client.setPrivateKey(AWS_PRIVATE_KEY);

    // 2. Configura o cliente MQTT
    mqtt_client.setServer(AWS_IOT_ENDPOINT, 8883); // Porta padrão AWS IoT
    mqtt_client.setCallback(mqtt_callback); // Define o "ouvido"
    mqtt_client.setBufferSize(512); // Aumenta o buffer 

    SerialMon.print(F("CommManager: Tentando conexão MQTT com o AWS IoT..."));
    int retries = 0;

    while (!mqtt_client.connected() && retries < 5) {
        SerialMon.printf(" (Tentativa %d/3)\n", retries + 1);
        SerialMon.printf("CommManager: Free Heap antes da chamada de conexão MQTT: %u\n", ESP.getFreeHeap());

        if (mqtt_client.connect(AWS_IOT_CLIENT_ID)) {
            SerialMon.println(F("CommManager: MQTT conectado com o AWS IoT!"));
            return true;
        } else {
            SerialMon.print(F("CommManager: conexão MQTT falhou, rc="));
            SerialMon.print(mqtt_client.state()); 
            SerialMon.println(F(". Tentando novamente em 5 segundos..."));
            
            delay(5000);
            retries++;
        }
    }

    SerialMon.println(F("CommManager: Falhou ao conectar ao AWS IoT após 5 tentativas."));
    return false;
}


/**
 * @brief (Função Privada) Serializa todos os dados dos sensores em um JSON
 * e o publica no tópico MQTT da AWS IoT.

 * @return true se a publicação MQTT for bem-sucedida, false caso contrário.
 */
static bool publish_data(
    const SCD40_Data& scd40_data, 
    const MICS6814_Data& mics_data, 
    const DSM501A_Data& dsm_data, 
    const GPS_Data& gps_data) {

    if (!modem.isGprsConnected()) {
         SerialMon.println(F("CommManager: GPRS não conectado. Não é possível publicar dados."));
         return false;
    }
    if (!mqtt_client.connected()) {
        SerialMon.println(F("CommManager: Cliente MQTT não conectado. Tentando reconexão..."));
        if (!connect_aws_iot()) { // Tenta reconectar ao MQTT
            SerialMon.println(F("CommManager: Falha ao reconectar MQTT. Não é possível publicar dados."));
            return false;
        }
    }

    JsonDocument jsonDoc; 
    jsonDoc["deviceId"] = AWS_IOT_CLIENT_ID;
    
    time_t now_epoch_utc;
    time(&now_epoch_utc); 
    jsonDoc["timestamp_utc_sec"] = now_epoch_utc;

    char time_str[32];
    struct tm *ptm = gmtime(&now_epoch_utc); 
    strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%SZ", ptm); 
    jsonDoc["datetime_utc_str"] = time_str;



     if (scd40_data.isValid) {
        JsonObject scd_json = jsonDoc["scd40"].to<JsonObject>();
        scd_json["co2"] = round(scd40_data.co2 * 100.0) / 100.0; 
        scd_json["temperature"] = round(scd40_data.temperature * 100.0) / 100.0;
        scd_json["humidity"] = round(scd40_data.humidity * 100.0) / 100.0;
    }
    
    if (mics_data.isValid) {
        JsonObject mics_json = jsonDoc["mics6814"].to<JsonObject>();
        // Enviar tanto a tensão (para depuração) quanto o PPM (para análise)
        mics_json["ppm_co"] = String(mics_data.ppm_co, 2);
        mics_json["ppm_no2"] = String(mics_data.ppm_no2, 2);
        mics_json["ppm_nh3"] = String(mics_data.ppm_nh3, 2);
        mics_json["raw_co"] = mics_data.raw_co;
        mics_json["raw_no2"] = mics_data.raw_no2;
        mics_json["raw_nh3"] = mics_data.raw_nh3;
}

    if (dsm_data.isValid) {
        JsonObject dsm_json = jsonDoc["dsm501a"].to<JsonObject>();
        dsm_json["lop_ratio_pm25"] = round(dsm_data.low_pulse_occupancy_ratio_pm25 * 100.0) / 100.0; 
        dsm_json["lop_ratio_pm10"] = round(dsm_data.low_pulse_occupancy_ratio_pm10 * 100.0) / 100.0;
    }

    if (gps_data.isValid) {
        JsonObject location_json = jsonDoc["location"].to<JsonObject>();
        location_json["latitude"] = serialized(String(gps_data.latitude, 6));
        location_json["longitude"] = serialized(String(gps_data.longitude, 6));
        location_json["accuracy_m"] = round(gps_data.accuracy * 100.0) / 100.0; 

        location_json["satellites_used"] = gps_data.satellites_used;
        
        location_json["satellites_visible"] = gps_data.satellites_visible;
        location_json["altitude_m"] = round(gps_data.altitude * 100.0) / 100.0;
    }

    char jsonBuffer[1024];
    size_t n = serializeJson(jsonDoc, jsonBuffer);
    if (n == 0) {
        SerialMon.println(F("CommManager: FALHA CRÍTICA - serializeJson() falhou. (JSON > 1024 bytes?)"));
        return false;
    }

    SerialMon.print(F("CommManager: Publicando mensagem ("));
    SerialMon.print(n);
    SerialMon.print(F(" bytes): "));
    SerialMon.println(jsonBuffer);

    mqtt_client.loop();
    delay(100);

    if (mqtt_client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer)) { 
        SerialMon.println(AWS_IOT_PUBLISH_TOPIC);
        SerialMon.println(F("CommManager: Mensagem publicada no tópico com sucesso!"));
        mqtt_client.loop();
        delay(500);
        return true;
    } else {
        SerialMon.print(F("CommManager: Falha ao publicar a mensagem. estado MQTT: "));
        SerialMon.println(mqtt_client.state());
        return false;
    }
}


/**
 * @brief (Função Privada) Desconecta e desliga todas as camadas da rede.
 *
 * Desliga a pilha na ordem correta (de cima para baixo):
 * 1. MQTT (PubSubClient)
 * 2. SSL (SSLClientESP32)
 * 3. TCP (TinyGsmClient)
 * 4. GPRS (TinyGSM)
 * 5. Hardware (Pulso de energia)
 */
static void disconnect_and_powerdown_modem() {
    SerialMon.println(F("CommManager: Iniciando sequência de desligamento..."));

    if (mqtt_client.connected()) {
        mqtt_client.disconnect();
        SerialMon.println(F("CommManager: MQTT desconectado."));
    }

    if (ssl_client.connected()) { 
        ssl_client.stop();
        SerialMon.println(F("CommManager: Cliente SSL parado."));
    }

    if (base_client.connected()) {
        base_client.stop();
        SerialMon.println(F("CommManager: Cliente Base (TCP) parado."));
    }

    if (modem.isGprsConnected()) {
        modem.gprsDisconnect(); 
        SerialMon.println(F("CommManager: GPRS desconectado."));
    }

    modemPowerOff(); 
    SerialMon.println(F("CommManager: Modem desligado fisicamente."));
}

bool perform_communication_cycle(
    const SCD40_Data& scd_data,
    const MICS6814_Data& mics_data,
    const DSM501A_Data& dsm_data,
    GPS_Data& out_gps_data
) {
    bool publication_successful = false;

    //===========
    int ntp_year = 0, ntp_month = 0, ntp_day = 0;
    int ntp_hour = 0, ntp_min = 0, ntp_sec = 0;
    float ntp_timezone = 0.0f;
    bool time_synced = false;
    String time_str;

    SerialMon.println(F("\n=== INICIANDO CICLO DE COMUNICAÇÃO ==="));

    if (!setup_modem_and_network()) {
        SerialMon.println(F("Comm. Cycle: FALHA CRÍTICA - Não foi possível ligar ou registrar o modem."));
        goto cleanup; 
    }

    if (!connect_gprs()) {
        SerialMon.println(F("Comm. Cycle: FALHA CRÍTICA - Não foi possível conectar ao GPRS (APN)."));
        goto cleanup;
    }

    if (!synchronize_time_with_ntp()) {
        SerialMon.println(F("Comm. Cycle: AVISO - Falha ao sincronizar o relógio."));
    } else {
        SerialMon.println(F("Comm. Cycle: Sincronização de relógio bem-sucedida."));
    }
    
    SerialMon.println(F("Comm. Cycle: Tentando obter localização GPS..."));
    get_gps_location(out_gps_data, 150);

    if (!connect_aws_iot()) {
        SerialMon.println(F("Comm. Cycle: FALHA CRÍTICA - Não foi possível conectar ao AWS IoT (MQTT)."));
        goto cleanup;
    }

    SerialMon.println(F("Comm. Cycle: Publicando dados dos sensores..."));
    if (publish_data(scd_data, mics_data, dsm_data, out_gps_data)) {
        SerialMon.println(F("Comm. Cycle: Publicação de dados BEM-SUCEDIDA."));
        publication_successful = true; 
    } else {
        SerialMon.println(F("Comm. Cycle: FALHA - Não foi possível publicar os dados."));
    }

cleanup:
SerialMon.println(F("Comm. Cycle: Executando limpeza e desligamento do modem..."));

disconnect_and_powerdown_modem();

SerialMon.println(F("=== CICLO DE COMUNICAÇÃO FINALIZADO ==="));

return publication_successful;
}