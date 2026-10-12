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

    bool modem_is_alive = false;

    modem.sendAT(""); 
    modem.sendAT("AT");
    if (modem.waitResponse(500) == 1) modem_is_alive = true;

    if (!modem_is_alive) {
        modemPowerOn(); 
        
        SerialMon.println(F("CommManager: Aguardando boot inicial do modem (6s)..."));
        delay(6000); 
    }

    SerialMon.println(F("CommManager: Reiniciando modem..."));
    if (!modem.restart()) {
        SerialMon.println(F("CommManager: Tentando Hard Reset..."));
        modemPowerOff(); delay(2000); modemPowerOn(); delay(6000);
        if (!modem.restart()) {
            modemPowerOff(); return false;
        }
    }

    modem.sendAT(F("+CTZU=0")); modem.waitResponse();
    modem.sendAT(F("+CTZR=0")); modem.waitResponse();

    if (modem.setNetworkMode(38)) {
        SerialMon.println(F("CommManager: Modo de Rede definido para LTE Only (38)."));
    } else {
        SerialMon.println(F("CommManager: Falha ao definir modo LTE."));
    }

    // 4. FORÇAR CAT-M1 (Preferred Mode 1)
    if (modem.setPreferredMode(1)) {
         SerialMon.println(F("CommManager: Preferência definida para CAT-M1."));
    }

    String modemInfo = modem.getModemInfo();
    SerialMon.print(F("CommManager: Informação do Modem: "));
    SerialMon.println(modemInfo);

    SerialMon.println(F("CommManager: Aguardando registro na rede... (Monitorando CSQ)"));
    // Tenta por até 180 segundos (3 minutos)
    unsigned long start = millis();
    bool connected = false;
    
    while (millis() - start < 180000L) {
        // Verifica se já registrou
        if (modem.isNetworkConnected()) {
            connected = true;
            break;
        }

        // Diagnóstico: Mostra o nível de sinal a cada 2 segundos
        // CSQ: 0-9 (Péssimo), 10-14 (Ok), 15-19 (Bom), 20+ (Excelente), 99 (Sem sinal)
        int csq = modem.getSignalQuality();
        SerialMon.print(F("Sinal CSQ: "));
        SerialMon.print(csq);
        
        // Interpretação rápida para você
        if (csq == 99) SerialMon.print(F(" (Sem Sinal/Buscando...)"));
        else if (csq < 10) SerialMon.print(F(" (Fraco)"));
        else SerialMon.print(F(" (Bom)"));
        
        SerialMon.println();
        
        delay(2000); // Espera 2s entre checagens
    }
    // =================================================================

    if (!connected) { 
        SerialMon.println(F("CommManager: TIMEOUT - Falha ao registrar na rede celular."));
        modemPowerOff();
        return false;
    }
    
    SerialMon.println(F("CommManager: Rede celular registrada!"));
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

    const int MAX_RETRIES = 3;
    const int RETRY_DELAY_MS = 3000;

    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        
        if (attempt > 1) {
            SerialMon.printf("CommManager: Tentativa GPRS %d/%d...\n", attempt, MAX_RETRIES);
        }

        if (modem.gprsConnect(APN, GPRS_USER, GPRS_PASS)) {
            // SUCESSO!
            SerialMon.println(F("CommManager: GPRS conectado com sucesso."));
            SerialMon.print(F("CommManager: Endereço IP Local: "));
            SerialMon.println(modem.getLocalIP());
            return true;
        } else {
            // FALHA NA TENTATIVA ATUAL
            SerialMon.print(F("CommManager: Falha na conexão GPRS. "));
            if (attempt < MAX_RETRIES) {
                SerialMon.println(F("Aguardando para tentar novamente..."));
                delay(RETRY_DELAY_MS);
            } else {
                SerialMon.println(F("Desistindo."));
            }
        }
    }

    return false;
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
 * @brief (Função Privada) Testa se há conectividade IP real (não apenas GPRS ativo).
 * 
 * Quando o SIM está sem crédito/dados, o GPRS conecta normalmente (obtém IP),
 * mas o tráfego não passa. Esta função detecta isso antes de gastar tempo
 * com o handshake TLS (~5s por tentativa).
 * 
 * @return true se conseguiu resolver DNS ou alcançar a rede, false caso contrário.
 */
static bool verify_internet_connectivity() {
    SerialMon.println(F("CommManager: Verificando conectividade IP real..."));

    // Teste 1: Verifica se GPRS ainda está conectado
    if (!modem.isGprsConnected()) {
        SerialMon.println(F("CommManager: GPRS desconectado!"));
        return false;
    }

    // Teste 2: Tenta um HTTP GET simples para verificar se os dados passam.
    modem.sendAT(F("+CDNSGIP=\"pool.ntp.org\""));
    int dns_result = modem.waitResponse(10000L);
    
    if (dns_result == 1) {
        SerialMon.println(F("CommManager: Conectividade IP confirmada (DNS OK)."));
        return true;
    }
    
    SerialMon.println(F("CommManager: AVISO - DNS falhou. Possível falta de crédito/dados no SIM."));
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

    // Verifica conectividade real antes do handshake TLS (caro em tempo/energia)
    if (!verify_internet_connectivity()) {
        SerialMon.println(F("CommManager: Sem conectividade IP. Abortando SSL/MQTT."));
        SerialMon.println(F("CommManager: Verifique crédito/dados do SIM."));
        return false;
    }

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
    const int MAX_MQTT_RETRIES = 3;
    int retries = 0;

    while (!mqtt_client.connected() && retries < MAX_MQTT_RETRIES) {
        SerialMon.printf(" (Tentativa %d/%d)\n", retries + 1, MAX_MQTT_RETRIES);
        SerialMon.printf("CommManager: Free Heap: %u\n", ESP.getFreeHeap());
 
        if (mqtt_client.connect(AWS_IOT_CLIENT_ID)) {
            SerialMon.println(F("CommManager: MQTT conectado com o AWS IoT!"));
            return true;
        } else {
            int mqtt_state = mqtt_client.state();
            SerialMon.printf("CommManager: MQTT falhou (rc=%d). ", mqtt_state);
            
            // Diagnóstico detalhado baseado no código de erro
            switch (mqtt_state) {
                case -4: SerialMon.println(F("Timeout na conexão.")); break;
                case -3: SerialMon.println(F("Conexão perdida.")); break;
                case -2: SerialMon.println(F("Falha no handshake SSL/TLS.")); break;
                case -1: SerialMon.println(F("Desconectado.")); break;
                case  1: SerialMon.println(F("Protocolo MQTT rejeitado.")); break;
                case  2: SerialMon.println(F("Client ID rejeitado.")); break;
                case  3: SerialMon.println(F("Servidor indisponível.")); break;
                case  4: SerialMon.println(F("Credenciais inválidas.")); break;
                case  5: SerialMon.println(F("Não autorizado.")); break;
                default: SerialMon.println(F("Erro desconhecido.")); break;
            }
            
            // Se é erro SSL (-2), verifica se GPRS ainda está ativo
            if (mqtt_state == -2 && !modem.isGprsConnected()) {
                SerialMon.println(F("CommManager: GPRS caiu durante tentativa. Abortando."));
                break;
            }
            
            delay(5000);
            retries++;
        }
    }

    SerialMon.printf("CommManager: Falhou ao conectar ao AWS IoT após %d tentativas.\n", MAX_MQTT_RETRIES);
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
    const SPS30_Data& sps30_data, 
    const Battery_Data& bat_data,
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
        // mics_json["raw_co"] = mics_data.raw_co;
        // mics_json["raw_no2"] = mics_data.raw_no2;
        // mics_json["raw_nh3"] = mics_data.raw_nh3;
}

    if (sps30_data.isValid) {
        JsonObject sps_json = jsonDoc["sps30"].to<JsonObject>();
        
        // Concentrações de Massa (PM1.0, PM2.5, PM4.0, PM10.0)
        sps_json["pm1_0_ugm3"] = String(sps30_data.pm1_0, 2);
        sps_json["pm2_5_ugm3"] = String(sps30_data.pm2_5, 2);
        // sps_json["pm4_0_ugm3"] = String(sps30_data.pm4_0, 2);
        sps_json["pm10_0_ugm3"] = String(sps30_data.pm10_0, 2);
        
        // sps_json["typical_size_um"] = String(sps30_data.typical_particle_size, 2);
    }

    if (gps_data.isValid) {
        JsonObject location_json = jsonDoc["location"].to<JsonObject>();
        location_json["latitude"] = serialized(String(gps_data.latitude, 6));
        location_json["longitude"] = serialized(String(gps_data.longitude, 6));
        // location_json["accuracy_m"] = round(gps_data.accuracy * 100.0) / 100.0; 

        // location_json["satellites_used"] = gps_data.satellites_used;
        
        // location_json["satellites_visible"] = gps_data.satellites_visible;
        // location_json["altitude_m"] = round(gps_data.altitude * 100.0) / 100.0;
    }

    JsonObject bat_json = jsonDoc["battery"].to<JsonObject>();
    
    bat_json["voltage_v"] = String(bat_data.voltage, 2);
    bat_json["percentage"] = String(bat_data.percentage, 0); // Sem casas decimais
    // bat_json["days_remaining"] = String(bat_data.days_remaining, 1);

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

    int publish_retries = 0;
    bool publish_success = false;

    while (!publish_success && publish_retries < 3) { // Tenta 3 vezes
        if (publish_retries > 0) {
            SerialMon.println(F("CommManager: Tentando novamente em 1s..."));
            delay(1000); 
        }

        // 1. Garante que o cliente MQTT esteja ativo e processando antes de publicar
        mqtt_client.loop(); 
        delay(500);

        // 2. Tenta a publicação
        if (mqtt_client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer)) {
            publish_success = true;
            break; // Publicação bem-sucedida
        }

        publish_retries++;
    }
    
    // 3. Verificação Final
    if (publish_success) { 
        SerialMon.println(AWS_IOT_PUBLISH_TOPIC);
        SerialMon.println(F("CommManager: Mensagem publicada (buffer)... "));
        
        // Mantém o loop() rodando por 5 segundos para garantir o envio
        unsigned long startWait = millis();
        while (millis() - startWait < 5000) {
            mqtt_client.loop();
            delay(100);
        }
        
        SerialMon.println(F("CommManager: Envio físico concluído."));
        return true;
    } else {
        SerialMon.print(F("CommManager: FALHA FINAL ao publicar. estado MQTT: "));
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
    const SPS30_Data& dsm_data,
    const Battery_Data& bat_data,
    GPS_Data& out_gps_data,
    const ProfileConfig& profile
) {
    bool publication_successful = false;

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
    
    // ==================== GPS (com cache RTC) ====================
    // Lógica: O GPS só roda efetivamente a cada GPS_UPDATE_INTERVAL_HOURS.
    // Entre os intervalos, reutiliza a última localização salva na memória RTC.
    // A decisão é feita aqui (após NTP) pois precisamos do relógio para comparar.
    
    if (profile.run_gps) {
        bool need_fresh_gps = true;
    
        
        if (gps_cache.magic == 0xCAFE && gps_cache.timestamp_epoch > 0) {
            time_t now;
            time(&now);
            uint32_t age_seconds = (uint32_t)now - gps_cache.timestamp_epoch;
            uint32_t interval_seconds = GPS_UPDATE_INTERVAL_HOURS * 3600UL;
            
            if (age_seconds < interval_seconds) {
                // Cache ainda é válido — reutilizar
                float age_hours = (float)age_seconds / 3600.0f;
                SerialMon.printf("Comm. Cycle: GPS cache válido (idade: %.1f h, limite: %d h). Reutilizando.\n", age_hours, GPS_UPDATE_INTERVAL_HOURS);
                
                out_gps_data.latitude  = gps_cache.latitude;
                out_gps_data.longitude = gps_cache.longitude;
                out_gps_data.accuracy  = gps_cache.accuracy;
                out_gps_data.isValid   = true;
                need_fresh_gps = false;
            } else {
                float age_hours = (float)age_seconds / 3600.0f;
                SerialMon.printf("Comm. Cycle: GPS cache expirado (idade: %.1f h). Atualizando.\n", age_hours);
            }
        }
        
        if (need_fresh_gps) {
            SerialMon.println(F("Comm. Cycle: Obtendo localização GPS fresca..."));
            uint16_t gps_timeout = profile.gps_timeout_ms / 1000;
            get_gps_location(out_gps_data, gps_timeout);
            
            // Se obteve fix, salva no cache RTC para os próximos ciclos
            if (out_gps_data.isValid) {
                save_gps_to_cache(out_gps_data);
            }
        }
    } else {
        SerialMon.println(F("Comm. Cycle: GPS OFF (Economia de Energia - Perfil)"));
        
        if (gps_cache.magic == 0xCAFE) {
            out_gps_data.latitude  = gps_cache.latitude;
            out_gps_data.longitude = gps_cache.longitude;
            out_gps_data.accuracy  = gps_cache.accuracy;
            out_gps_data.isValid   = true;
            SerialMon.println(F("Comm. Cycle: Usando última localização conhecida do cache."));
        } else {
            out_gps_data.isValid = false;
        }
    }

    // ==================== RECONEXÃO GPRS PÓS-GPS ====================
    
    if (!modem.isGprsConnected()) {
        SerialMon.println(F("Comm. Cycle: GPRS caiu durante aquisição GPS. Reconectando..."));
        
        if (!connect_gprs()) {
            SerialMon.println(F("Comm. Cycle: FALHA - Não foi possível reconectar GPRS."));
            goto cleanup;
        }
        SerialMon.println(F("Comm. Cycle: GPRS reconectado com sucesso."));
    }

    if (!connect_aws_iot()) {
        SerialMon.println(F("Comm. Cycle: FALHA CRÍTICA - Não foi possível conectar ao AWS IoT (MQTT)."));
        goto cleanup;
    }

    SerialMon.println(F("Comm. Cycle: Publicando dados dos sensores..."));
    if (publish_data(scd_data, mics_data, dsm_data, bat_data, out_gps_data)) {
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