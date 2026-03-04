#include "scd40_handler.h"

// Definição do endereço I2C padrão do SCD40
#define SCD40_I2C_ADDRESS 0x62

SensirionI2cScd4x scd4x; 

bool scd40_init() {

    uint16_t error;
    char errorMessage[256];

    Serial.println("SCD40: Initializing...");

    scd4x.begin(Wire, SCD40_I2C_ADDRESS);

    error = scd4x.stopPeriodicMeasurement();
    if (error) {
        // Apenas aviso, pois se já estiver parado, pode retornar erro.
        Serial.println("SCD40: AVISO - stopPeriodicMeasurement() falhou (Pode já estar parado).");
    }

    delay(500);

    Serial.println("SCD40: Inicialização bem-sucedida (Modo Idle).");
    return true;
}

bool scd40_read_measurements(SCD40_Data &data) {

    uint16_t error;
    char errorMessage[256];

    data.isValid = false; 
    bool dataReady = false;

    // --- FASE 1: INICIAR MEDIÇÃO ---
    // Inicia o ciclo periodicamente apenas quando for ler
    error = scd4x.startPeriodicMeasurement();
    if (error) {
        Serial.println("SCD40: FALHA CRÍTICA - Erro ao iniciar medição.");
        return false;
    }

    Serial.print("SCD40: Aquecendo e lendo dados (5s)");

    // --- FASE 2: AGUARDAR DADOS (Polling eficiente) ---
    for (int attempt = 0; attempt < 12; attempt++) { 
        error = scd4x.getDataReadyStatus(dataReady); 
        
        if (error) {
            Serial.print("x"); // Erro de I2C
        } else if (dataReady) {
            break; // Sai do loop, dados prontos!
        } else {
            Serial.print("."); 
        }
        
        delay(500); 
    }
    Serial.println(); 

    if (!dataReady) {
        Serial.println("SCD40: FALHA - Timeout. Sensor não disponibilizou dados.");
        scd4x.stopPeriodicMeasurement(); 
        return false; 
    }

    // --- FASE 3: LER E PARAR ---
    uint16_t co2_ppm_uint; 
    float temperature_float;
    float humidity_float;

    error = scd4x.readMeasurement(co2_ppm_uint, temperature_float, humidity_float);
    
    scd4x.stopPeriodicMeasurement();

    if (error) {
        Serial.print("SCD40: FALHA CRÍTICA - Erro ao ler a medição: ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        return false; 
    } 
    
    if (co2_ppm_uint == 0) { 
        Serial.println("SCD40: AVISO - Leitura inválida (0 ppm). Sensor pode estar instável.");
        return false;
    }

    // --- FASE 4: SALVAR DADOS ---
    data.co2 = static_cast<float>(co2_ppm_uint);
    data.temperature = temperature_float;
    data.humidity = humidity_float;
    data.isValid = true; 

    Serial.printf("SCD40: Sucesso! CO2: %.0f ppm | Temp: %.1f C | Umid: %.1f %%\n", data.co2, data.temperature, data.humidity);

    return true;
}