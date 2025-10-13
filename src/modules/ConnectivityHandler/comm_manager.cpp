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

static void modemPowerOn() {
    pinMode(MODEM_PWRKEY_PIN, OUTPUT);
    SerialMon.println(F("CommManager: Powering on modem..."));
    digitalWrite(MODEM_PWRKEY_PIN, LOW);
    delay(100); 
    digitalWrite(MODEM_PWRKEY_PIN, HIGH);
    delay(1000); 
    digitalWrite(MODEM_PWRKEY_PIN, LOW);
    SerialMon.println(F("CommManager: Modem power on sequence complete. Waiting for boot..."));
    delay(5000); 
}

static void modemPowerOff() {
    pinMode(MODEM_PWRKEY_PIN, OUTPUT);
    SerialMon.println(F("CommManager: Powering off modem..."));
    digitalWrite(MODEM_PWRKEY_PIN, LOW);
    delay(100);
    digitalWrite(MODEM_PWRKEY_PIN, HIGH);
    delay(1200); 
    digitalWrite(MODEM_PWRKEY_PIN, LOW);
    SerialMon.println(F("CommManager: Modem powered off."));
}

/**
 * @brief Converts a tm struct (assumed to be in UTC) to a time_t epoch value.
 * This is a portable implementation of the non-standard timegm().
 * @param tm Pointer to the tm struct containing the UTC time.
 * @return The number of seconds since the epoch (1970-01-01 00:00:00 UTC).
 */
static time_t timegm_custom(struct tm *tm) {
    const int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    long days = 0;
    int year = tm->tm_year + 1900;

    for (int y = 1970; y < year; ++y) {
        days += 365 + ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0));
    }
    for (int m = 0; m < tm->tm_mon; ++m) {
        days += days_in_month[m];
        if (m == 1 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
            days += 1; // Leap year
        }
    }
    days += tm->tm_mday - 1;
    return ((days * 24L + tm->tm_hour) * 60 + tm->tm_min) * 60 + tm->tm_sec;
}

/**
 * @brief Tenta obter a hora da rede celular e configurar a hora do sistema ESP32 para UTC.
 * @return true se a hora do sistema foi configurada com sucesso, false caso contrário.
 */
static bool comm_manager_sync_system_time() {
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    float timezone_offset_from_utc = 0; 
    bool time_successfully_set_on_esp32 = false;

    SerialMon.println(F("CommManager: Attempting to get network time for system sync..."));
    
    for (int i = 0; i < 3 && !time_successfully_set_on_esp32; i++) {
        // For SIM7000, getNetworkTime() returns UTC when network time sync is enabled (AT+CLTS=1).
        // The getNetworkUTCTime() is not implemented for this modem in TinyGSM.
        if (modem.getNetworkTime(&year, &month, &day, &hour, &minute, &second, &timezone_offset_from_utc)) {
            SerialMon.printf("CommManager: Network Time (from modem): Y:%04d M:%02d D:%02d H:%02d M:%02d S:%02d (TZ offset: %.1f)\n",
                               year, month, day, hour, minute, second, timezone_offset_from_utc);
            
            if (year < 2025) { 
                SerialMon.printf("CommManager: Network time year (%d) seems invalid or too old. Attempt %d/3.\n", year, i + 1);
                delay(1000); 
                continue; 
            }

            struct tm tminfo; 
            tminfo.tm_year = year - 1900; 
            tminfo.tm_mon = month - 1;    
            tminfo.tm_mday = day;
            tminfo.tm_hour = hour;        
            tminfo.tm_min = minute;
            tminfo.tm_sec = second;
            tminfo.tm_isdst = 0;              

            // Use our custom, portable timegm() to convert UTC components to epoch time.
            // This avoids all timezone-related issues with mktime().
            time_t epoch_time = timegm_custom(&tminfo);

            if (epoch_time == (time_t)-1) {
                SerialMon.println(F("CommManager: ERROR - timegm_custom() failed to convert time."));
            } else {
                SerialMon.printf("CommManager: timegm_custom() success. Calculated epoch_time (UTC): %lu\n", (unsigned long)epoch_time);
                struct timeval tv;
                tv.tv_sec = epoch_time; 
                tv.tv_usec = 0;         
                
                if (settimeofday(&tv, NULL) == 0) {
                    SerialMon.println(F("CommManager: ESP32 System time (UTC) successfully set."));
                    // Now, set the local timezone for functions like localtime() to work correctly.
                    // For UTC-3, the POSIX string is "UTC+3".
                    setenv("TZ", "UTC+3", 1);
                    tzset();
                    time_successfully_set_on_esp32 = true;

                    time_t check_time;
                    time(&check_time); 
                    struct tm *ptm = gmtime(&check_time);
                    char buffer_time_str[32];
                    strftime(buffer_time_str, sizeof(buffer_time_str), "%Y-%m-%d %H:%M:%S UTC", ptm);
                    SerialMon.print(F("CommManager: System time check (string after settimeofday): ")); SerialMon.println(buffer_time_str);
                } else {
                    SerialMon.println(F("CommManager: ERROR - settimeofday() failed."));
                }
            }
        } else {
            SerialMon.printf("CommManager: Failed to get network time on attempt %d/3.\n", i + 1);
            delay(2000); 
        }
    } 

    if (!time_successfully_set_on_esp32) {
        SerialMon.println(F("CommManager: CRITICAL - Failed to set system time from network after all attempts. SSL/TLS handshake may fail."));
    }
    return time_successfully_set_on_esp32;
}

void comm_manager_init_serial() {
    SerialAT.begin(57600, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
    SerialMon.println(F("CommManager: SerialAT initialized for modem."));
    delay(100); 
}

bool comm_manager_setup_modem_and_network() {
    modemPowerOn();

    comm_manager_init_serial();

    SerialMon.println(F("CommManager: Initializing modem with TinyGSM..."));
    if (!modem.begin()) { 
        SerialMon.println(F("CommManager: Failed to initialize modem with TinyGSM!"));
        return false;
    }

    String modemInfo = modem.getModemInfo();
    SerialMon.print(F("CommManager: Modem Info: "));
    SerialMon.println(modemInfo);


    SerialMon.println(F("CommManager: Waiting for network registration..."));
    if (!modem.waitForNetwork(180000L)) { 
        SerialMon.println(F("CommManager: Failed to connect to cellular network."));
        return false;
    }
    SerialMon.println(F("CommManager: Cellular network connected."));
    SerialMon.print(F("CommManager: Signal quality: "));
    SerialMon.println(modem.getSignalQuality());


    return true;
}

static void parseGpsResponse(String response, bool &fixStatus, String &utc, float &lat, float &lon, float &alt, float &speed_kph) {
    int dataStartIndex = response.indexOf(":") + 1;
    String data = response.substring(dataStartIndex);
    data.trim();

    int lastIndex = 0;
    int commaIndex = 0;
    String fieldValue;

    // O formato de +CGNSINF é:
    // <GNSS run status>,<Fix status>,<UTC datetime>,<Latitude>,<Longitude>,<MSL Altitude>,<Speed Over Ground>...
    for (int i = 0; i < 7; i++) {
        commaIndex = data.indexOf(',', lastIndex);
        if (commaIndex == -1) {
            commaIndex = data.length();
        }
        fieldValue = data.substring(lastIndex, commaIndex);
        lastIndex = commaIndex + 1;

        switch (i) {
            case 0: break; // <GNSS run status>
            case 1: fixStatus = (fieldValue.toInt() == 1); break;
            case 2: utc = fieldValue; break;
            case 3: lat = fieldValue.toFloat(); break;
            case 4: lon = fieldValue.toFloat(); break;
            case 5: alt = fieldValue.toFloat(); break;
            case 6: speed_kph = fieldValue.toFloat(); break; // Velocidade em Km/h
        }
    }
}

bool comm_manager_get_gps_location(GPS_Data& gps_data, uint16_t timeout_seconds) {
    gps_data.isValid = false;
    Serial.println(F("Ativando GPS com AT+CGNSPWR=1..."));

    // Liga a energia do GPS
    modem.sendAT("+CGNSPWR=1");
    if (modem.waitResponse() != 1) {
        Serial.println(F("Falha ao ligar o GPS (SGPIO também pode ser uma opção)."));
        // Poderíamos tentar o SGPIO aqui como fallback se quiséssemos
        return false;
    }
    Serial.println(F("GPS ativado. Tentando obter localização via polling com AT+CGNSINF..."));

    unsigned long start_time = millis();
    bool got_fix = false;

    while (millis() - start_time < (unsigned long)timeout_seconds * 10) {
        modem.sendAT("+CGNSINF");
        String rawResponse = "";
        if (modem.waitResponse(2000L, rawResponse) == 1 && rawResponse.indexOf("+CGNSINF:") != -1) {
            
            bool fix = false;
            String utc = "";
            float lat = 0.0, lon = 0.0, alt = 0.0, speed = 0.0;
            
            parseGpsResponse(rawResponse, fix, utc, lat, lon, alt, speed);

            if (fix) {
                Serial.println(F("\n--- FIX VÁLIDO OBTIDO! ---"));
                got_fix = true;
                gps_data.isValid = true;
                gps_data.latitude = lat;
                gps_data.longitude = lon;
                // Os outros campos do +CGNSINF podem ser mapeados para a struct se necessário
                // Ex: gps_data.hdop, gps_data.satellites etc.
                break; // Sai do loop de tentativa
            } else {
                Serial.print(F("GPS respondeu, mas sem fix. "));
            }
        } else {
            Serial.print(F("Aguardando resposta do modem... "));
        }
        Serial.println("Tentando novamente em 5s.");
        delay(5000);
    }

    if (!got_fix) {
        Serial.println(F("\nTimeout! Não foi possível obter um fix de GPS."));
    }

    // Desliga a energia do GPS para economizar bateria
    Serial.println(F("Desligando a energia do GPS com AT+CGNSPWR=0..."));
    modem.sendAT("+CGNSPWR=0");
    modem.waitResponse();

    return gps_data.isValid;
}

bool comm_manager_connect_gprs() {
    SerialMon.print(F("CommManager: Connecting to GPRS (APN: "));
    SerialMon.print(APN);
    SerialMon.println(F(")..."));

    if (!modem.gprsConnect(APN, GPRS_USER, GPRS_PASS)) {
        SerialMon.println(F("CommManager: GPRS connection failed."));
        return false;
    }
    SerialMon.println(F("CommManager: GPRS connected."));
    SerialMon.print(F("CommManager: IP Address: "));
    SerialMon.println(modem.getLocalIP());
    return true;
}

void comm_manager_mqtt_callback(char* topic, byte* payload, unsigned int len) {
    SerialMon.print(F("CommManager: Message arrived ["));
    SerialMon.print(topic);
    SerialMon.print(F("]: "));
    for (unsigned int i = 0; i < len; i++) {
        SerialMon.print((char)payload[i]);
    }
    SerialMon.println();
}

bool comm_manager_connect_aws_iot() {
   
    if (!comm_manager_sync_system_time()) {
        SerialMon.println(F("CommManager: Warning - Proceeding with MQTT connection attempt despite failed system time sync."));
    }

    SerialMon.printf("CommManager: Free Heap before SSL config: %u\n", ESP.getFreeHeap()); // Verificar Heap
    SerialMon.println(F("CommManager: Configuring SSL/TLS for AWS IoT..."));
    ssl_client.setCACert(AWS_IOT_ROOT_CA);
    ssl_client.setCertificate(AWS_CERT_CRT);
    ssl_client.setPrivateKey(AWS_PRIVATE_KEY);

    mqtt_client.setServer(AWS_IOT_ENDPOINT, 8883);
    mqtt_client.setCallback(comm_manager_mqtt_callback);
    mqtt_client.setBufferSize(512); 

    SerialMon.print(F("CommManager: Attempting MQTT connection to AWS IoT..."));
    int retries = 0;
    while (!mqtt_client.connected() && retries < 3) {
        SerialMon.printf(" (Attempt %d/3)\n", retries + 1);
        SerialMon.printf("CommManager: Free Heap before MQTT connect call: %u\n", ESP.getFreeHeap());
        if (mqtt_client.connect(AWS_IOT_CLIENT_ID)) {
            SerialMon.println(F("CommManager: MQTT connected to AWS IoT!"));
            return true;
        } else {
            SerialMon.print(F("CommManager: MQTT connection failed, rc="));
            SerialMon.print(mqtt_client.state()); 
            SerialMon.println(F(". Trying again in 5 seconds..."));
            
            delay(5000);
            retries++;
        }
    }
    SerialMon.println(F("CommManager: Failed to connect to AWS IoT after retries."));
    return false;
}

bool comm_manager_publish_data(const SCD40_Data& scd40_data, const MICS6814_Data& mics_data, const DSM501A_Data& dsm_data, const GPS_Data& gps_data) {
    if (!modem.isGprsConnected()) {
         SerialMon.println(F("CommManager: GPRS not connected. Cannot publish."));
         return false;
    }
    if (!mqtt_client.connected()) {
        SerialMon.println(F("CommManager: MQTT client not connected. Attempting to reconnect..."));
        if (!comm_manager_connect_aws_iot()) { // Tenta reconectar ao MQTT
            SerialMon.println(F("CommManager: Failed to reconnect MQTT. Cannot publish."));
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
        mics_json["voltage_co"] = mics_data.voltage_co;
        mics_json["ppm_co"] = mics_data.ppm_co;
        mics_json["voltage_no2"] = mics_data.voltage_no2;
        mics_json["ppm_no2"] = mics_data.ppm_no2;
        mics_json["voltage_nh3"] = mics_data.voltage_nh3;
        mics_json["ppm_nh3"] = mics_data.ppm_nh3;
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
        location_json["hdop"] = round(gps_data.hdop * 100.0) / 100.0; // Agora enviamos o HDOP
        location_json["satellites"] = gps_data.satellites;
    }

    char jsonBuffer[1024];
    size_t n = serializeJson(jsonDoc, jsonBuffer);

    SerialMon.print(F("CommManager: Publishing message ("));
    SerialMon.print(n);
    SerialMon.print(F(" bytes): "));
    SerialMon.println(jsonBuffer);

    mqtt_client.loop();
    delay(100);

    if (mqtt_client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer)) { 
        SerialMon.println(AWS_IOT_PUBLISH_TOPIC);
        SerialMon.println(F("CommManager: Message published successfully!"));
        mqtt_client.loop();
        delay(2000);
        return true;
    } else {
        SerialMon.print(F("CommManager: Failed to publish message. MQTT state: "));
        SerialMon.println(mqtt_client.state());
        return false;
    }
}

void comm_manager_disconnect_and_powerdown_modem() {
    if (mqtt_client.connected()) {
        mqtt_client.disconnect();
        SerialMon.println(F("CommManager: MQTT disconnected."));
    }
    if (ssl_client.connected()) { 
        ssl_client.stop();
        SerialMon.println(F("CommManager: SSL client stopped."));
    }
    if (base_client.connected()) {
        base_client.stop();
        SerialMon.println(F("CommManager: Base (TCP) client stopped."));
    }
    if (modem.isGprsConnected()) {
        modem.gprsDisconnect(); 
        SerialMon.println(F("CommManager: GPRS detached."));
    }
    modemPowerOff(); 
    SerialMon.println(F("CommManager: Modem powered down or in sleep mode."));
}