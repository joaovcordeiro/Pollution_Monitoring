#include <Arduino.h>
#define TINY_GSM_MODEM_SIM800  
#include "SIM800LModule.h"
#include "SCD40Module.h"
#include "MICS6814Module.h"
#include "DSM501AModule.h"
#include <ADS1X15.h> 

// Instâncias dos módulos
SIM800LModule sim800lModule;
SCD40Module scd40Module;
ADS1115 ads(0x48); // Endereço I2C do ADS1115, geralmente é 0x48
MICS6814Module mics6814Module(ads);
DSM501AModule dsm501aModule(25, 26);  // Usando GPIO 25 para PM1.0 e GPIO 26 para PM2.5

void setup() {
    Serial.begin(115200);

    // Inicializando o SIM800L
    sim800lModule.begin();

    // Inicializando o SCD40
    scd40Module.begin();

    // Inicializando o ADS1115 e o MICS6814
    ads.begin();
    mics6814Module.begin();
    mics6814Module.calibrate(); // Calibrando o MICS6814

    // Inicializando o DSM501A e realizando calibração bloqueante
    dsm501aModule.begin();
    dsm501aModule.calibrate();

    Serial.println("Sistema iniciado.");
}

void loop() {
    // Atualização do status de conexão do SIM800L
    sim800lModule.update();

    TowerInfo towerInfo = sim800lModule.getTowerInfo();

    // Leitura dos valores do SCD40
    float co2, temperature, humidity;
    bool scd40Valid = scd40Module.readMeasurements(co2, temperature, humidity);

    if (scd40Valid) {
        // Exibindo os valores lidos do SCD40
        Serial.print("CO2: ");
        Serial.print(co2);
        Serial.print(" ppm, Temperatura: ");
        Serial.print(temperature);
        Serial.print(" °C, Umidade: ");
        Serial.print(humidity);
        Serial.println(" %RH");
    } else {
        Serial.println("Erro na leitura do SCD40.");
    }

    // Leitura dos gases do MICS6814
    float co = mics6814Module.measure(CO);
    float nh3 = mics6814Module.measure(NH3);
    float no2 = mics6814Module.measure(NO2);

    // Exibindo os valores lidos do MICS6814
    Serial.print("CO: ");
    Serial.print(co);
    Serial.print(" ppm, NH3: ");
    Serial.print(nh3);
    Serial.print(" ppm, NO2: ");
    Serial.print(no2);
    Serial.println(" ppm");

     // Leitura das concentrações de partículas do DSM501A após calibração
    float concentrationPM10 = dsm501aModule.getConcentrationPM10();
    float concentrationPM25 = dsm501aModule.getConcentrationPM25();

    // Envio dos dados ao ThingSpeak através do SIM800L
    sim800lModule.sendData(
        co2, 
        temperature, 
        humidity, 
        co, 
        nh3, 
        no2, 
        concentrationPM10, 
        concentrationPM25, 
        towerInfo.mcc, 
        towerInfo.mnc, 
        String(towerInfo.lac),  // Convertendo LAC para String
        String(towerInfo.cellid) // Convertendo Cell ID para String
    );
    


    delay(20000);  // Aguarda 20 segundos antes de verificar novamente
}
