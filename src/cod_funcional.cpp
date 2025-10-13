
// #define TINY_GSM_MODEM_SIM7000 // Define o modelo do modem

// #include "config.h"
// #include <PubSubClient.h>     // Para comunicação MQTT
// #include <ArduinoJson.h>      // Para construir payloads JSON
// #include <TinyGsmClient.h>    // Para o módulo GSM
// #include <TinyGsmClientSIM7000.h> // Específico para SIM7000
// #include <TinyGsm.h>
// #include <SSLClientESP32.h>
// #include "modules/SCD40/scd40_handler.h"
// #include "modules/PowerManager/power_manager.h"

// SCD40_Data scd40SensorData;


// // --- Configurações GSM/GPRS ---
// #define SerialMon Serial       // Serial para monitoramento
// #define SerialAT Serial1       // Serial para comunicação AT com o modem


// // --- Objetos de conexão ---
// TinyGsm modem(SerialAT);
// TinyGsmClient base_client(modem, 0);
// SSLClientESP32 ssl_client(&base_client);
// PubSubClient client(ssl_client);

// // --- Variáveis de estado ---
// unsigned long lastMsg = 0;
// #define MSG_INTERVAL 5000 // Intervalo de envio de mensagens (5 segundos)

// void powerOnModem() {
//   SerialMon.println("Powering on modem...");
//   pinMode(MODEM_PWRKEY_PIN, OUTPUT);
//   digitalWrite(MODEM_PWRKEY_PIN, LOW);
//   delay(100);
//   digitalWrite(MODEM_PWRKEY_PIN, HIGH);
//   delay(1000); // Mantenha o pino alto por ~1 segundo
//   digitalWrite(MODEM_PWRKEY_PIN, LOW);
//   delay(3000); // Aguarde o modem iniciar
//   SerialMon.println("Modem should be on.");
// }

// void modemReset() {
//   SerialMon.println("Resetting modem...");
//   digitalWrite(MODEM_RST_PIN, LOW);
//   delay(100);
//   digitalWrite(MODEM_RST_PIN, HIGH);
//   delay(3000); // Aguarde o modem reiniciar
//   SerialMon.println("Modem reset complete.");
// }

// void connectGPRS() {
//   SerialMon.print("Connecting to GPRS network (APN: ");
//   SerialMon.print(APN);
//   SerialMon.println(")...");

//   if (!modem.gprsConnect(APN, GPRS_USER, GPRS_PASS)) {
//     SerialMon.println(" GPRS failed to connect. Retrying...");
//     delay(5000);
//     ESP.restart(); // Reinicia o ESP32 em caso de falha persistente
//   }
//   SerialMon.println("GPRS connected.");
// }

// void messageHandler(char* topic, byte* payload, unsigned int length) {
//   SerialMon.print("Message arrived [");
//   SerialMon.print(topic);
//   SerialMon.print("] ");
//   for (int i = 0; i < length; i++) {
//     SerialMon.print((char)payload[i]);
//   }
//   SerialMon.println();
//   // Aqui você pode processar a mensagem recebida, como comandos para o dispositivo
// }

// void connectAWSIoT() {
//   SerialMon.print("Connecting to AWS IoT Core...");

//   // Configura a conexão SSL/TLS
//   ssl_client.setCACert(AWS_IOT_ROOT_CA);
//   ssl_client.setCertificate(AWS_CERT_CRT);
//   ssl_client.setPrivateKey(AWS_PRIVATE_KEY);

//    // Configura o cliente MQTT
//   client.setServer(AWS_IOT_ENDPOINT, 8883); // Porta padrão MQTTs
//   client.setCallback(messageHandler);
//   client.setBufferSize(256); // Mantenha ou ajuste este valor
//   SerialMon.printf("Free Heap after MQTT setup: %d bytes\n", ESP.getFreeHeap());

//   while (!client.connected()) {
//     SerialMon.print("Attempting MQTT connection...");
//     // Tente conectar ao AWS IoT Core
//     if (client.connect(AWS_IOT_CLIENT_ID)) {
//       SerialMon.println("connected!");
//       // Se precisar subscrever a tópicos, faça aqui
//       // client.subscribe("telemetria/lilygo/MeuLilyGoTSIM7000G/commands");
//     } else {
//       SerialMon.print("failed, rc=");
//       SerialMon.print(client.state()); // Código de erro MQTT
//       SerialMon.println(" trying again in 5 seconds");
//       delay(5000);
//       if (client.state() == -2) { // MQTT_CONNECT_FAILED
//         SerialMon.println("MQTT Connect Failed. Check certificates, endpoint, and client ID.");
//         // Considerar um reset ou reconfiguração do GPRS aqui
//         // ESP.restart(); // Reinicia o ESP32
//       }
//     }
//   }
// }




// void setup() {
//   SerialMon.begin(115200); // Ajuste para 115200
//   delay(10);

//   SerialMon.println("Initializing...");
//   SerialMon.printf("Free Heap at setup start: %d bytes\n", ESP.getFreeHeap());

//    setup_sensor_power();

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


//   powerOnModem();
//   delay(100);

//   SerialAT.begin(57600, SERIAL_8N1, 26, 27); // Ajuste para 115200
//   delay(3000);

//   SerialMon.println("Modem initializing...");
//   if (!modem.begin()) {
//     SerialMon.println("Failed to start modem. Please check wiring and power.");
//     while (true);
//   }
//   SerialMon.println("Modem started.");
//   SerialMon.printf("Free Heap after modem.begin(): %d bytes\n", ESP.getFreeHeap());

  
  

 


//   modem.waitForNetwork();
//   if (modem.isNetworkConnected()) {
//     SerialMon.println("Modem connected to network.");
//   } else {
//     SerialMon.println("Modem not connected to network. Check SIM card and antenna.");
//     while(true);
//   }
//   SerialMon.printf("Free Heap after network connected: %d bytes\n", ESP.getFreeHeap());

//   connectGPRS();
//   SerialMon.printf("Free Heap after GPRS connected: %d bytes\n", ESP.getFreeHeap());
//   connectAWSIoT();
//   SerialMon.printf("Free Heap after AWS IoT connected: %d bytes\n", ESP.getFreeHeap());

//   if (!client.connected()) {
//     SerialMon.println("AWS IoT Core connection lost. Reconnecting...");
//     connectGPRS(); // Garante que o GPRS ainda está conectado
//     connectAWSIoT();
//   }
//   client.loop(); // Processa mensagens MQTT pendentes e mantém a conexão

//   unsigned long currentMillis = millis();
//   if (currentMillis - lastMsg > MSG_INTERVAL) {
//     lastMsg = currentMillis;

//     // --- Montar payload JSON ---
//     StaticJsonDocument<256> doc; // Tamanho do documento JSON (ajuste conforme a necessidade)
//     doc["deviceId"] = AWS_IOT_CLIENT_ID;
//     doc["timestamp"] = String(currentMillis);
//     doc["co2"] = round(scd40SensorData.co2 * 100.0) / 100.0; // Arredonda para 2 casas decimais
//     doc["temperature"] = round(scd40SensorData.temperature * 100.0) / 100.0;
//     doc["humidity"] = round(scd40SensorData.humidity * 100.0) / 100.0;
//     // Adicionar mais dados do sensor aqui

//     char jsonBuffer[256];
//     serializeJson(doc, jsonBuffer);

//     SerialMon.print("Publishing message: ");
//     SerialMon.println(jsonBuffer);

//     // Publica a mensagem
//     if (client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer)) {
//       SerialMon.println("Message published successfully!");
//     } else {
//       SerialMon.print("Failed to publish message. Error code: ");
//       SerialMon.println(client.state());
//     }
//   }
//   SerialMon.printf("Free Heap in loop: %d bytes\n", ESP.getFreeHeap());
// }

// void loop() {
  
// }