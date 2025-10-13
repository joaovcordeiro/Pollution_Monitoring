// #include "config.h"
// #include "modules/PowerManager/power_manager.h"
// #include "modules/SCD40/scd40_handler.h" 
// #include "modules/CommManager/comm_manager.h"

// SCD40_Data scd40SensorData; // Variável global ou local para armazenar os dados

// void setup() {
//     Serial.begin(115200);

//     delay(2000); 
//     Serial.println("\n--- System Boot / Wake Up ---");
//     Serial.printf("Free Heap at setup start: %u bytes\n", ESP.getFreeHeap());

//     comm_manager_init_serial();

//     setup_sensor_power();

//     power_sensors_on();

//     // Inicializa I2C (geralmente uma vez é suficiente se os pinos não mudam)
//     // Se você não especificou pinos para Wire.begin(), ele usa os padrões GPIO21 (SDA), GPIO22 (SCL).
//     // Certifique-se de que o conversor de nível lógico esteja conectado a esses pinos ou ajuste aqui.
//     Wire.begin(); 

//     if (scd40_init()) {
//         Serial.println(F("Main: SCD40 initialized successfully."));
//         if (!scd40_read_measurements(scd40SensorData) || !scd40SensorData.isValid) {
//             Serial.println(F("Main: Failed to read valid data from SCD40."));
//             // Tratar: os dados inválidos não serão enviados ou terão um placeholder
//         } else {
//             Serial.println(F("Main: SCD40 data read successfully."));
//             Serial.printf("SCD40 Data: CO2:%.1f ppm, Temp:%.1f C, Hum:%.1f %%RH\n",
//                              scd40SensorData.co2, scd40SensorData.temperature, scd40SensorData.humidity);
//         }
//     } else {
//         Serial.println(F("Main: SCD40 initialization failed."));
//     }

//     power_sensors_off();
//     Serial.printf("Free Heap after sensor readings: %u bytes\n", ESP.getFreeHeap());

//     // ETAPA 2: Comunicação de Dados
//     bool dataTransmissionSuccessful = false;
//     if (comm_manager_setup_modem_and_network()) {
//         if (comm_manager_connect_gprs()) {
//             if (comm_manager_connect_aws_iot()) {
//                 if (comm_manager_publish_data(scd40SensorData /*, outras_leituras */)) {
//                     dataTransmissionSuccessful = true;
//                 }
//             }
//         }
//     }

//     comm_manager_disconnect_and_powerdown_modem(); // Sempre tenta desligar o modem
//     Serial.printf("Free Heap after communication attempt: %u bytes\n", ESP.getFreeHeap());

//     // ETAPA 3: Entrar em Deep Sleep
//     if (dataTransmissionSuccessful) {
//         Serial.println(F("Main: Process complete, data sent. Entering Deep Sleep."));
//     } else {
//         Serial.println(F("Main: Process had errors (data not sent or other failure). Entering Deep Sleep to save power."));
//     }
//     enter_deep_sleep();
    
// }

// void loop() {
 
//     Serial.println(F("Main: Loop reached - this is unexpected. Forcing sleep."));
//     delay(1000); 
//     enter_deep_sleep();
// }