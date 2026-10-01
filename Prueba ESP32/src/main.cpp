#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "PalmasInnWiFi";
const char* password = "palmasinn";

const char* mqttBroker = "192.168.20.127";
const uint16_t mqttPort = 1883;
const char* mqttUser = "Administrador";
const char* mqttPassword = "PalmasInn2026";
const char* deviceId = "esp32_01";
const char* commandTopic = "osmosis/device/esp32_01/command";
const char* telemetryTopic = "osmosis/device/telemetry";
const char* statusTopic = "osmosis/device/esp32_01/status";

WiFiClient espClient;
PubSubClient client(espClient);

const byte pinCaudalimetro = 4;
const byte pinSensorTDS_Entrada = 35;
const byte pinSensorTDS_Salida = 34;

volatile uint16_t pulseCount = 0;
float flowRate = 0.0f;
const float flowCalibrationFactor = 5.5f;

const float VREF = 3.3f;
const float ADC_RES = 4095.0f;

const float calFactorEntrada = 0.5f;
const float calFactorSalida = 0.25f;

void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

void setupWifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a Wi-Fi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi conectado");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Intentando conexión MQTT...");

    if (client.connect("ESP32_Osmosis_Client", mqttUser, mqttPassword)) {
      Serial.println("Broker MQTT conectado");
      client.subscribe(commandTopic);
      return;
    }

    Serial.print("Falló, rc=");
    Serial.print(client.state());
    Serial.println(". Reintentando en 5s");
    delay(5000);
  }
}

float leerSensorTDS(byte pinSensor, float factorCalib) {
  long sumaADC = 0;

  for (int i = 0; i < 30; i++) {
    sumaADC += analogRead(pinSensor);
    delay(1);
  }

  const float promedioADC = sumaADC / 30.0f;
  const float voltPin = (promedioADC / ADC_RES) * VREF;
  const float voltRealMaquina = voltPin * 2.0f;

  if (voltRealMaquina < 0.05f) {
    Serial.printf("[TDS DEBUG] Pin %d sin señal -> ADC=%.1f, volt=%.3fV\n", pinSensor, promedioADC, voltRealMaquina);
    return 0.0f;
  }

  float tdsPpm = (133.42f * pow(voltRealMaquina, 3) - 255.86f * pow(voltRealMaquina, 2) + 857.39f * voltRealMaquina) * factorCalib;

  Serial.printf("[TDS DEBUG] Pin %d -> ADC=%.1f, volt=%.3fV, TDS=%.1f ppm\n", pinSensor, promedioADC, voltRealMaquina, tdsPpm);

  if (tdsPpm < 0.0f) {
    tdsPpm = 0.0f;
  }

  return tdsPpm;
}

void publishReading(const char* topic, float value, int decimals) {
  char payload[32];
  dtostrf(value, 5, decimals, payload);
  client.publish(topic, payload);
}

String normalizeCommand(String message) {
  message.trim();

  if (message.startsWith("{")) {
    int commandIndex = message.indexOf("\"command\"");
    if (commandIndex != -1) {
      int firstQuote = message.indexOf('"', commandIndex + 9);
      int secondQuote = message.indexOf('"', firstQuote + 1);
      if (firstQuote != -1 && secondQuote != -1) {
        return message.substring(firstQuote + 1, secondQuote);
      }
    }
  }

  return message;
}

void handleCommand(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  String command = normalizeCommand(message);
  Serial.print("[MQTT COMMAND] Tópico: ");
  Serial.print(topic);
  Serial.print(" | Comando: ");
  Serial.println(command);

  if (command == "start_pump") {
    Serial.println("Acción: bomba activada");
  } else if (command == "stop_pump") {
    Serial.println("Acción: bomba detenida");
  } else if (command == "restart") {
    Serial.println("Acción: reinicio solicitado");
  }
}

void publishTelemetry(float flow, float tdsEntrada, float tdsSalida, float eficienciaRechazo) {
  String payload = "{";
  payload += "\"deviceId\":\"" + String(deviceId) + "\",";
  payload += "\"flow\":" + String(flow, 2) + ",";
  payload += "\"tdsEntrada\":" + String(tdsEntrada, 0) + ",";
  payload += "\"tdsSalida\":" + String(tdsSalida, 0) + ",";
  payload += "\"rejection\":" + String(eficienciaRechazo, 1);
  payload += "}";

  client.publish(telemetryTopic, payload.c_str());
}

void publishStatus(float flow, float tdsEntrada, float tdsSalida, float eficienciaRechazo) {
  String payload = "{";
  payload += "\"deviceId\":\"" + String(deviceId) + "\",";
  payload += "\"wifiConnected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  payload += "\"mqttConnected\":" + String(client.connected() ? "true" : "false") + ",";
  payload += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  payload += "\"uptimeMs\":" + String(millis()) + ",";
  payload += "\"flow\":" + String(flow, 2) + ",";
  payload += "\"tdsEntrada\":" + String(tdsEntrada, 0) + ",";
  payload += "\"tdsSalida\":" + String(tdsSalida, 0) + ",";
  payload += "\"rejection\":" + String(eficienciaRechazo, 1);
  payload += "}";

  client.publish(statusTopic, payload.c_str());
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetPinAttenuation(pinSensorTDS_Entrada, ADC_11db);
  analogSetPinAttenuation(pinSensorTDS_Salida, ADC_11db);

  setupWifi();
  client.setServer(mqttBroker, mqttPort);
  client.setCallback(handleCommand);

  pinMode(pinCaudalimetro, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pinCaudalimetro), pulseCounter, FALLING);

  pinMode(pinSensorTDS_Entrada, INPUT);
  pinMode(pinSensorTDS_Salida, INPUT);
}

void loop() {
  if (!client.connected()) {
    reconnect();
    client.subscribe(commandTopic);
  }
  client.loop();

  static unsigned long lastMillis = 0;
  const unsigned long currentMillis = millis();

  if (currentMillis - lastMillis >= 1000UL) {
    detachInterrupt(digitalPinToInterrupt(pinCaudalimetro));
    flowRate = ((1000.0f / (currentMillis - lastMillis)) * pulseCount) / flowCalibrationFactor;
    pulseCount = 0;
    lastMillis = currentMillis;
    attachInterrupt(digitalPinToInterrupt(pinCaudalimetro), pulseCounter, FALLING);

    const float tdsEntrada = leerSensorTDS(pinSensorTDS_Entrada, calFactorEntrada);
    const float tdsSalida = leerSensorTDS(pinSensorTDS_Salida, calFactorSalida);

    float eficienciaRechazo = 0.0f;
    if (tdsEntrada > 0.0f && tdsEntrada > tdsSalida) {
      eficienciaRechazo = ((tdsEntrada - tdsSalida) / tdsEntrada) * 100.0f;
    }

    publishReading("osmosis/sensor/flujo", flowRate, 2);
    publishReading("osmosis/sensor/tds_entrada", tdsEntrada, 0);
    publishReading("osmosis/sensor/tds_salida", tdsSalida, 0);
    publishReading("osmosis/sensor/rechazo", eficienciaRechazo, 1);
    publishTelemetry(flowRate, tdsEntrada, tdsSalida, eficienciaRechazo);
    publishStatus(flowRate, tdsEntrada, tdsSalida, eficienciaRechazo);

    Serial.print("[MQTT Publicado] Flujo: ");
    Serial.print(flowRate);
    Serial.print(" L/min | Entrada: ");
    Serial.print(tdsEntrada, 0);
    Serial.print(" ppm | Salida: ");
    Serial.print(tdsSalida, 0);
    Serial.print(" ppm | Rechazo: ");
    Serial.print(eficienciaRechazo, 1);
    Serial.println(" %");
  }
}