#ifndef DSM501AMODULE_H
#define DSM501AMODULE_H

#include <Arduino.h>

class DSM501AModule {
public:
    DSM501AModule(uint8_t pinPM10, uint8_t pinPM25, unsigned long sampleTimeMs = 30000);  // Construtor com pinos e tempo de amostragem
    void begin();                               // Inicializa o sensor
    void calibrate();                           // Função bloqueante que espera os dados ficarem prontos
    void measure();  
    float getConcentrationPM10() const;         // Retorna a última concentração de PM1.0
    float getConcentrationPM25() const;         // Retorna a última concentração de PM2.5

private:
    uint8_t pinPM10;                            // Pino do sensor DSM501A para partículas PM1.0
    uint8_t pinPM25;                            // Pino do sensor DSM501A para partículas PM2.5
    unsigned long sampleTime;                   // Tempo de amostragem para medir a duração do pulso baixo
    unsigned long lowPulseOccupancyPM10;        // Duração do pulso baixo para PM1.0
    unsigned long lowPulseOccupancyPM25;        // Duração do pulso baixo para PM2.5
    unsigned long startTime;                    // Tempo inicial da medição
    float concentrationPM10;                    // Armazena a última concentração medida de PM1.0
    float concentrationPM25;                    // Armazena a última concentração medida de PM2.5
};

#endif // DSM501AMODULE_H
