#include "SIM800LModule.h"
#include "credentials.h"

// // Configuração da chave de API do ThingSpeak
// const char* apiKey = "U7QJ6FTL2G47KTO5";  
// const char* server = "api.thingspeak.com";
// // const char* apiKey2 = "LVCL59C5YTFG13V9";  // Substitua pela sua chave de API do segundo canal

// // Configuração do ponto de acesso da rede, configurar conforme o chip utilizado
// const char apn[] = "zap.vivo.com.br";        
// const char gprsUser[] = "vivo";            
// const char gprsPass[] = "vivo";           


SIM800LModule::SIM800LModule() 
    : modem(Serial2), client(modem) {
}

void SIM800LModule::begin() {
    // Set modem reset, enable, power pins
    pinMode(MODEM_RST, OUTPUT);
    digitalWrite(MODEM_RST, HIGH);

    pinMode(MODEM_PWKEY, OUTPUT);
    digitalWrite(MODEM_PWKEY, HIGH);

    pinMode(MODEM_POWER_ON, OUTPUT);
    digitalWrite(MODEM_POWER_ON, HIGH);

    // Start serial for SIM800L
    Serial2.begin(9600);
    delay(3000);

    // Turn on the module by toggling the PWRKEY pin
    digitalWrite(MODEM_PWKEY, LOW);
    delay(1000);
    digitalWrite(MODEM_PWKEY, HIGH);

    // Initialize the modem
    Serial.println("Inicializando o modem...");
    modem.restart();

    // Check signal quality
    int signalQuality = modem.getSignalQuality();
    Serial.print("Qualidade do sinal: ");
    Serial.println(signalQuality);

    // Register on the network
    connectToNetwork();
}

void SIM800LModule::update() {
    if (!modem.isNetworkConnected()) {
        Serial.println("Conexão de rede perdida. Tentando reconectar...");
        connectToNetwork();
    }

    if (!modem.isGprsConnected()) {
        Serial.println("Conexão GPRS perdida. Tentando reconectar...");
        if (!reconnectGPRS()) {
            // If reconnection fails, reset the modem
            Serial.println("Reiniciando o modem...");
            modem.restart();
            connectToNetwork();
        }
    }
}

void SIM800LModule::sendData(float co2, float temperature, float humidity, float co, float nh3, float no2, float concentrationPM10, float concentrationPM25, String mcc, String mnc, String lac, String cellid) {
    // Agrupamento dos dados dos sensores em um objeto JSON
    String sensorDataJson = "{\"co2\":" + String(co2) +
                            ",\"temperature\":" + String(temperature) +
                            ",\"humidity\":" + String(humidity) +
                            ",\"co\":" + String(co) +
                            ",\"nh3\":" + String(nh3) +
                            ",\"no2\":" + String(no2) +
                            ",\"pm10\":" + String(concentrationPM10) +
                            ",\"pm25\":" + String(concentrationPM25) + "}";

    // Agrupamento dos dados da torre em um objeto JSON
    String towerDataJson = "{\"mcc\":\"" + mcc +
                           "\",\"mnc\":\"" + mnc +
                           "\",\"lac\":\"" + lac +
                           "\",\"cellid\":\"" + cellid + "\"}";

    // Cria a URL para enviar os dados agrupados em campos JSON
    String url = "/update?api_key=" + String(apiKey) +
                 "&field1=" + sensorDataJson +  // Dados dos sensores
                 "&field2=" + towerDataJson;   // Dados da torre

    // Tenta enviar os dados até 3 vezes antes de desistir
    for (int i = 0; i < 3; i++) {
        if (client.connect(server, 80)) {
            Serial.println("Conectado ao ThingSpeak");

            // Envia a requisição GET com os dados agrupados
            client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                         "Host: " + server + "\r\n" +
                         "Connection: close\r\n\r\n");

            // Espera a resposta do servidor
            while (client.connected()) {
                String line = client.readStringUntil('\n');
                if (line == "\r") {
                    break;
                }
            }
            Serial.println("Dados enviados com sucesso.");
            client.stop();
            return; // Sai da função após o sucesso
        } else {
            Serial.println("Falha ao conectar ao ThingSpeak. Tentando novamente...");
            delay(5000);  // Espera 5 segundos antes de tentar novamente
        }
    }
    Serial.println("Falha ao enviar dados ao ThingSpeak após múltiplas tentativas.");
}


TowerInfo SIM800LModule::getTowerInfo() {
    TowerInfo towerInfo; // Estrutura para armazenar os dados da torre

    // Envia o comando para escanear as torres
    modem.sendAT("+CNETSCAN=1");
    modem.sendAT("+CNETSCAN");

    // Aguarda a resposta do comando
    if (modem.waitResponse(20000L, GF("Operator:")) == 1) {
        String response = Serial2.readStringUntil('\n');  // Lê a resposta
        Serial.println("Resposta da Torre: " + response);

        // Extração e conversão dos valores
        String cellidHex = response.substring(response.indexOf("Cellid:") + 7, response.indexOf(",Arfcn:"));
        String lacHex = response.substring(response.indexOf("Lac:") + 4, response.indexOf(",Bsic:"));
        towerInfo.cellid = strtol(cellidHex.c_str(), NULL, 16);  // Converte Cell ID de hexadecimal para decimal
        towerInfo.lac = strtol(lacHex.c_str(), NULL, 16);        // Converte LAC de hexadecimal para decimal
        towerInfo.mcc = response.substring(response.indexOf("MCC:") + 4, response.indexOf(",MNC:"));
        towerInfo.mnc = response.substring(response.indexOf("MNC:") + 4, response.indexOf(",Rxlev:"));

        Serial.print("Cell ID (Decimal): ");
        Serial.println(towerInfo.cellid);
        Serial.print("LAC (Decimal): ");
        Serial.println(towerInfo.lac);
        Serial.print("MCC: ");
        Serial.println(towerInfo.mcc);
        Serial.print("MNC: ");
        Serial.println(towerInfo.mnc);
    } else {
        Serial.println("Falha ao obter dados da torre de celular.");

        // Definindo valores padrão para indicar falha
        towerInfo.mcc = "N/A";
        towerInfo.mnc = "N/A";
        towerInfo.lac = -1;
        towerInfo.cellid = -1;
    }

    return towerInfo;  // Retorna os dados da torre
}


// void SIM800LModule::sendDataToSecondChannel(String mcc, String mnc, long lac, long cellid) {
//     // Cria a URL com os campos para enviar os dados da torre ao segundo canal
//     String url = "/update?api_key=" + String(apiKey2) +
//                  "&field1=" + mcc +
//                  "&field2=" + mnc +
//                  "&field3=" + String(lac) +
//                  "&field4=" + String(cellid);

//     // Tenta enviar os dados até 3 vezes antes de desistir
//     for (int i = 0; i < 3; i++) {
//         if (client.connect(server, 80)) {
//             Serial.println("Conectado ao segundo canal do ThingSpeak");

//             // Envia a requisição GET com os dados da torre
//             client.print(String("GET ") + url + " HTTP/1.1\r\n" +
//                          "Host: " + server + "\r\n" +
//                          "Connection: close\r\n\r\n");

//             // Espera a resposta do servidor
//             while (client.connected()) {
//                 String line = client.readStringUntil('\n');
//                 if (line == "\r") {
//                     break;
//                 }
//             }
//             Serial.println("Dados da torre enviados com sucesso.");
//             client.stop();
//             return; // Sai da função após o sucesso
//         } else {
//             Serial.println("Falha ao conectar ao segundo canal do ThingSpeak. Tentando novamente...");
//             delay(5000);  // Espera 5 segundos antes de tentar novamente
//         }
//     }
//     Serial.println("Falha ao enviar dados ao segundo canal do ThingSpeak após múltiplas tentativas.");
// }

void SIM800LModule::connectToNetwork() {
    unsigned long startTime = millis();
    const unsigned long timeout = 60000; // 60 seconds timeout

    int registrationStatus = modem.getRegistrationStatus();
    while ((registrationStatus != 1 && registrationStatus != 5) && (millis() - startTime < timeout)) {
        Serial.println("Tentando registrar na rede...");
        delay(5000);
        registrationStatus = modem.getRegistrationStatus();
        Serial.print("Status de registro na rede: ");
        Serial.println(registrationStatus);

        // If status is 0, try to initiate network registration
        if (registrationStatus == 0) {
            modem.sendAT("+CREG=0"); // Disable network registration unsolicited result code
            modem.sendAT("+CREG=1"); // Enable network registration unsolicited result code
            delay(1000);
        }
    }

    if (registrationStatus == 1 || registrationStatus == 5) {
        Serial.println("Registrado na rede com sucesso!");
    } else {
        Serial.println("Falha ao registrar na rede após múltiplas tentativas.");
        // Try resetting the modem
        Serial.println("Reiniciando o modem...");
        modem.restart();
        delay(5000); // Wait for modem to restart
        // Optionally, you can try to register again or return to avoid blocking
        return;
    }

    if (modem.gprsConnect(apn, gprsUser, gprsPass)) {
        Serial.println("Conectado ao GPRS.");
    } else {
        Serial.println("Falha ao conectar ao GPRS.");
    }
}

bool SIM800LModule::reconnectGPRS() {
    for (int i = 0; i < 3; i++) { // Tenta reconectar ao GPRS 3 vezes
        if (modem.gprsConnect(apn, gprsUser, gprsPass)) {
            Serial.println("Reconectado ao GPRS.");
            return true;
        } else {
            Serial.println("Falha ao reconectar ao GPRS. Tentando novamente...");
            delay(5000);  // Espera 5 segundos antes de tentar novamente
        }
    }
    Serial.println("Falha ao reconectar ao GPRS após múltiplas tentativas.");
    return false;
}
