#ifndef SIM800LMODULE_H
#define SIM800LMODULE_H

#define TINY_GSM_MODEM_SIM800  // Define o modelo do modem SIM800
#include <TinyGsmClient.h>

// Configurações do SIM800L e GPRS
#define MODEM_RST 5
#define MODEM_PWKEY 4
#define MODEM_POWER_ON 23
#define MODEM_TX 27
#define MODEM_RX 26

// Declarações das variáveis globais como extern
extern const char* apiKey;       // Chave de API do primeiro canal do ThingSpeak
extern const char* server;       // Endereço do servidor ThingSpeak para o primeiro canal
extern const char* apiKey2;      // Chave de API do segundo canal do ThingSpeak
extern const char* server2;      // Endereço do servidor ThingSpeak para o segundo canal
extern const char apn[];         // APN da operadora
extern const char gprsUser[];    // Nome de usuário da operadora
extern const char gprsPass[];    // Senha da operadora

struct TowerInfo {
    String mcc;
    String mnc;
    long lac;
    long cellid;
};

class SIM800LModule {
public:
    SIM800LModule();

    void begin();                     // Inicializa o modem e conecta à rede
    void update();                    // Atualiza o status da conexão
    void sendData(float co2, 
                  float temperature, 
                  float humidity, 
                  float co, 
                  float nh3, 
                  float no2, 
                  float concentrationPM10, 
                  float concentrationPM25,
                  String mcc,
                  String mnc,
                  String lac,
                  String cellid); // Envia dados ao ThingSpeak
    TowerInfo getTowerInfo();               // Obtém e envia dados da torre de celular para o segundo canal

private:
    void connectToNetwork();          // Conecta à rede móvel
    bool reconnectGPRS();             // Tenta reconectar ao GPRS
    // void sendDataToSecondChannel(String mcc, String mnc, long lac, long cellid); // Envia dados ao segundo canal

    TinyGsm modem = TinyGsm(Serial2); // Instância do modem com Serial2
    TinyGsmClient client = TinyGsmClient(modem); // Cliente para envio de dados
};

#endif // SIM800LMODULE_H
