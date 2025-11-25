#include "sps30_handler.h"
#include <Wire.h>

// Definição dos tipos para compatibilidade (NO_ERROR)
#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0

// Endereço I2C padrão do SPS30
#define SPS30_I2C_ADDR 0x69 
#define MAX_POLLING_ATTEMPTS 30

// Instância do objeto da biblioteca do sensor (externa)
SensirionI2cSps30 sps30; 

// Variável para armazenar erros da biblioteca
uint16_t error;
char errorMessage[256];

/**
 * @brief Inicializa o sensor SPS30.
 *
 * NOTA: Usamos o formato SPS30_OUTPUT_FORMAT_OUTPUT_FORMAT_FLOAT do exemplo
 * para garantir que a leitura de dados seja em ponto flutuante.
 */
bool sps30_init() {
    Serial.println("SPS30: Initializing...");

    sps30.begin(Wire, SPS30_I2C_ADDR); 
    
    // 1. Para a medição, caso esteja ativa (boa prática)
    sps30.stopMeasurement();

    sps30.startFanCleaning();
    if (error != NO_ERROR) {
        Serial.print("SPS30: AVISO - Falha ao iniciar a limpeza do ventilador. Código: ");
        Serial.println(error);
        // Não é fatal, mas é bom logar.
    }
    
    // 2. Inicia a medição periódica com o formato FLOAT (como no seu exemplo)
    //    Esta chamada é o nosso 'probe'. Se falhar, o sensor não existe.
    error = sps30.startMeasurement(SPS30_OUTPUT_FORMAT_OUTPUT_FORMAT_FLOAT);
    
    if (error != NO_ERROR) {
        // Se esta chamada falhar, o sensor não foi encontrado no barramento I2C
        Serial.print("SPS30: FALHA CRÍTICA - Erro ao iniciar a medição (Sensor não encontrado?). Código: ");
        Serial.println(error);
        return false;
    }

    Serial.println("SPS30: Medição periódica iniciada no formato FLOAT.");
    Serial.println("SPS30: Inicialização bem-sucedida.");
    return true;
}

/**
 * @brief Realiza a leitura dos dados do sensor SPS30.
 *
 * NOTA: Esta função pressupõe que o tempo de aquecimento de 60s 
 * (delay(60000)) já foi cumprido no main.cpp.
 *
 * @return true se a leitura foi bem-sucedida, false caso contrário.
 */
bool sps30_read_measurements(SPS30_Data &data) {
    data.isValid = false; 
    
    // VARIÁVEIS DE ACUMULAÇÃO (float, para precisão)
    float sum_pm1_0 = 0.0f;
    float sum_pm2_5 = 0.0f;
    float sum_pm4_0 = 0.0f;
    float sum_pm10_0 = 0.0f;
    float last_typical_size = 0.0f; 
    int valid_samples = 0;
    
    // VARIÁVEIS TEMPORÁRIAS PARA A LEITURA
    float mc1p0, mc2p5, mc4p0, mc10p0; 
    float nc0p5, nc1p0, nc2p5, nc4p0, nc10p0;
    float typicalParticleSize;
    
    Serial.println("SPS30: Iniciando loop de média (30 amostras / 30s)...");

    // O loop roda 30 vezes, esperando 1s entre as leituras (30 segundos total).
    for (int i = 0; i < 30; i++) {
        
        // Não precisamos mais de polling com readDataReadyFlag, pois esperamos 60s
        // (400ms a mais para garantir a nova amostra)
        delay(400); 
        
        // Leitura dos valores reais (usando a função de floats do exemplo)
        error = sps30.readMeasurementValuesFloat(mc1p0, mc2p5, mc4p0, mc10p0,
                                                 nc0p5, nc1p0, nc2p5, nc4p0,
                                                 nc10p0, typicalParticleSize);
        
        if (error == NO_ERROR) {
            // Acumula as concentrações de massa (as mais importantes)
            sum_pm1_0 += mc1p0;
            sum_pm2_5 += mc2p5;
            sum_pm4_0 += mc4p0;
            sum_pm10_0 += mc10p0;
            
            last_typical_size = typicalParticleSize; 
            valid_samples++;
        } else {
            Serial.printf("SPS30: Aviso - Erro de leitura na amostra %d. Código: %d\n", i + 1, error);
        }
        
        // Espera 600ms restantes para fechar o ciclo de 1 segundo (400ms já passou)
        delay(600); 
    }

    // 3. Cálculo da Média Final
    if (valid_samples < 20) { // Se menos de 20 amostras válidas foram obtidas, falha.
        Serial.printf("SPS30: FALHA - Apenas %d amostras válidas obtidas. Sensor instável.\n", valid_samples);
        return false;
    }
    
    // Divide a soma pelo número de amostras válidas para obter a média
    data.pm1_0 = sum_pm1_0 / valid_samples;
    data.pm2_5 = sum_pm2_5 / valid_samples;
    data.pm4_0 = sum_pm4_0 / valid_samples;
    data.pm10_0 = sum_pm10_0 / valid_samples;
    
    data.typical_particle_size = last_typical_size;
    data.isValid = true; 
    
    Serial.printf("SPS30: Média FINAL de PM2.5 (30s): %.2f µg/m³\n", data.pm2_5);
    Serial.printf("SPS30: Amostras Válidas: %d/30\n", valid_samples);
    return true;
}