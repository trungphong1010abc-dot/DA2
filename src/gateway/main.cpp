#include <Arduino.h>
#include <LoRa.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <math.h>

#include "analysis.h"
#include "project_config.h"
#include "protocol.h"

namespace {

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

uint32_t lastPacketIdByNode[256] = {0};
uint32_t sleepOverrideSeconds = 0;
bool loraReady = false;
bool wifiConnecting = false;
uint32_t wifiAttemptStartedMs = 0;
uint32_t lastWifiAttemptMs = 0;
QueueHandle_t loraPacketQueue = nullptr;
SemaphoreHandle_t loraMutex = nullptr;
SemaphoreHandle_t mqttMutex = nullptr;

struct ReceivedLoraPacket {
  char payload[640];
  int rssi;
};

String jsonEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if (c == '"' || c == '\\') {
      escaped += '\\';
    }
    escaped += c;
  }
  return escaped;
}

void appendJsonNumber(String &json, const char *key, float value, uint8_t decimals) {
  if (!json.endsWith("{")) {
    json += ",";
  }
  json += "\"";
  json += key;
  json += "\":";
  if (isfinite(value)) {
    json += String(value, static_cast<unsigned int>(decimals));
  } else {
    json += "null";
  }
}

void appendJsonNumber(String &json, const char *key, uint32_t value) {
  if (!json.endsWith("{")) {
    json += ",";
  }
  json += "\"";
  json += key;
  json += "\":";
  json += String(value);
}

void appendJsonNumber(String &json, const char *key, int32_t value) {
  if (!json.endsWith("{")) {
    json += ",";
  }
  json += "\"";
  json += key;
  json += "\":";
  json += String(value);
}

void appendJsonText(String &json, const char *key, const String &value) {
  if (!json.endsWith("{")) {
    json += ",";
  }
  json += "\"";
  json += key;
  json += "\":\"";
  json += jsonEscape(value);
  json += "\"";
}

void appendJsonBool(String &json, const char *key, bool value) {
  if (!json.endsWith("{")) {
    json += ",";
  }
  json += "\"";
  json += key;
  json += "\":";
  json += value ? "true" : "false";
}

bool initLora() {
  SPI.begin(Config::LORA_SCK_PIN, Config::LORA_MISO_PIN, Config::LORA_MOSI_PIN, Config::LORA_CS_PIN);
  LoRa.setPins(Config::LORA_CS_PIN, Config::LORA_RST_PIN, Config::LORA_DIO0_PIN);

  if (!LoRa.begin(Config::LORA_FREQUENCY_HZ)) {
    Serial.println("[LoRa] init failed");
    return false;
  }

  LoRa.setSyncWord(Config::LORA_SYNC_WORD);
  LoRa.setTxPower(Config::LORA_TX_POWER_DBM);
  LoRa.setSignalBandwidth(Config::LORA_SIGNAL_BANDWIDTH);
  LoRa.setSpreadingFactor(Config::LORA_SPREADING_FACTOR);
  LoRa.setCodingRate4(Config::LORA_CODING_RATE_DENOM);
  LoRa.enableCrc();
  LoRa.receive();
  Serial.println("[LoRa] ready");
  return true;
}

void connectWifi() {
  if (WiFi.status() == WL_CONNECTED || wifiConnecting) {
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);
  wifiConnecting = true;
  wifiAttemptStartedMs = millis();
  lastWifiAttemptMs = wifiAttemptStartedMs;
}

void serviceWifi() {
  if (!wifiConnecting) {
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnecting = false;
    Serial.printf("[WiFi] connected ip=%s\n", WiFi.localIP().toString().c_str());
    return;
  }

  if (millis() - wifiAttemptStartedMs >= 20000) {
    wifiConnecting = false;
    WiFi.disconnect();
    Serial.println("[WiFi] connection failed");
  }
}

bool connectMqtt() {
  if (mqtt.connected()) {
    return true;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  const String clientId = String("esp32-gateway-") + String(static_cast<uint32_t>(ESP.getEfuseMac()), HEX);
  Serial.printf("[MQTT] connecting to %s:%u\n", Config::TB_HOST, Config::TB_PORT);
  const bool ok = mqtt.connect(clientId.c_str(), Config::TB_TOKEN, nullptr);
  if (ok) {
    Serial.println("[MQTT] connected to ThingsBoard");
  } else {
    Serial.printf("[MQTT] failed rc=%d\n", mqtt.state());
  }
  return ok;
}

String buildThingsBoardPayload(const Protocol::SensorPacket &data, const Analysis::Result &analysis) {
  String json;
  json.reserve(1400);
  json += "{";

  appendJsonText(json, "project", Config::PROJECT_TAG);
  appendJsonNumber(json, "gateway_id", data.gatewayId);
  appendJsonNumber(json, "node_id", data.nodeId);
  appendJsonNumber(json, "packet_id", data.packetId);
  appendJsonNumber(json, "timestamp_ms", data.timestampMs);

  appendJsonNumber(json, "soil_adc", static_cast<uint32_t>(data.soilAdc));
  appendJsonNumber(json, "h_soil", data.soilPercent, 2);
  appendJsonNumber(json, "beta_deg", data.betaDeg, 3);
  appendJsonNumber(json, "beta_dot_deg_per_hour", data.betaDotDegPerHour, 4);
  appendJsonNumber(json, "a_rms_g", data.vibrationRmsG, 5);
  appendJsonNumber(json, "pitch_deg", data.pitchDeg, 3);
  appendJsonNumber(json, "roll_deg", data.rollDeg, 3);
  appendJsonNumber(json, "v_bat", data.batteryV, 3);
  appendJsonNumber(json, "lora_rssi", static_cast<int32_t>(data.rssi));
  appendJsonNumber(json, "error_flag", static_cast<uint32_t>(data.errorFlags));
  appendJsonText(json, "error_text", Analysis::errorFlagsToText(data.errorFlags));

  appendJsonBool(json, "analysis_valid", analysis.valid);
  appendJsonNumber(json, "u_kpa", analysis.porePressureKpa, 3);
  appendJsonNumber(json, "tau_kpa", analysis.tauDriveKpa, 3);
  appendJsonNumber(json, "sigma_n_kpa", analysis.sigmaNormalKpa, 3);
  appendJsonNumber(json, "sigma_effective_kpa", analysis.sigmaEffectiveKpa, 3);
  appendJsonNumber(json, "tau_f_kpa", analysis.shearStrengthKpa, 3);
  appendJsonNumber(json, "epsilon_star", analysis.strainIndex, 5);

  // Descriptive aliases retained for existing ThingsBoard widgets.
  appendJsonNumber(json, "pore_pressure_kpa", analysis.porePressureKpa, 3);
  appendJsonNumber(json, "sigma_normal_kpa", analysis.sigmaNormalKpa, 3);
  appendJsonNumber(json, "tau_drive_kpa", analysis.tauDriveKpa, 3);
  appendJsonNumber(json, "shear_strength_kpa", analysis.shearStrengthKpa, 3);
  appendJsonNumber(json, "fs", analysis.factorOfSafety, 3);
  appendJsonNumber(json, "di", analysis.dynamicIndex, 3);
  appendJsonNumber(json, "strain_index", analysis.strainIndex, 3);
  appendJsonText(json, "duty_cycle_mode", analysis.dutyCycleMode);
  appendJsonText(json, "battery_status", analysis.batteryStatus);
  appendJsonBool(json, "ota_locked", analysis.otaLocked);
  appendJsonNumber(json, "sleep_duration_sec", analysis.nextSleepSeconds);
  appendJsonNumber(json,
                   "sleep_duration_min",
                   static_cast<uint32_t>(analysis.nextSleepSeconds / 60UL));
  appendJsonNumber(json, "next_sleep_sec", analysis.nextSleepSeconds);
  appendJsonText(json, "alert_level", analysis.alertLevel);
  appendJsonText(json, "risk_status", analysis.riskStatus);
  appendJsonText(json, "warning_message", analysis.warningMessage);

  json += "}";
  return json;
}

bool publishTelemetry(const String &payload) {
  if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
    Serial.println("[MQTT] busy, telemetry not published");
    return false;
  }

  if (!connectMqtt()) {
    xSemaphoreGive(mqttMutex);
    Serial.println("[MQTT] offline, telemetry not published");
    return false;
  }

  const bool ok = mqtt.publish(Config::TB_TELEMETRY_TOPIC, payload.c_str());
  xSemaphoreGive(mqttMutex);
  Serial.printf("[MQTT] publish %s bytes=%u\n", ok ? "OK" : "FAILED", payload.length());
  if (!ok) {
    Serial.println(payload);
  }
  return ok;
}

void sendAck(const Protocol::SensorPacket &data, const Analysis::Result &analysis, bool accepted) {
  Protocol::AckPacket ack;
  ack.gatewayId = data.gatewayId;
  ack.nodeId = data.nodeId;
  ack.packetId = data.packetId;
  ack.status = accepted ? "OK" : "DUPLICATE";
  ack.alertLevel = analysis.alertLevel;
  ack.sleepSeconds = sleepOverrideSeconds > 0 ? sleepOverrideSeconds : analysis.nextSleepSeconds;

  const String packet = Protocol::buildAckPacket(ack);
  if (xSemaphoreTake(loraMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    Serial.println("[LoRa] ACK failed: radio busy");
    return;
  }
  LoRa.idle();
  LoRa.beginPacket();
  LoRa.print(packet);
  LoRa.endPacket();
  LoRa.receive();
  xSemaphoreGive(loraMutex);

  Serial.printf("[LoRa] ACK node=%u pid=%lu status=%s sleep=%.1f minutes alert=%s\n",
                ack.nodeId,
                static_cast<unsigned long>(ack.packetId),
                ack.status.c_str(),
                ack.sleepSeconds / 60.0f,
                ack.alertLevel.c_str());
}

void printAllVariables(const Protocol::SensorPacket &data, const Analysis::Result &result) {
  Serial.printf("[RAW] gateway_id=%u node_id=%u packet_id=%lu timestamp_ms=%lu soil_adc=%d "
                "h_soil=%.2f beta_deg=%.3f beta_dot_deg_per_hour=%.4f a_rms_g=%.5f "
                "pitch_deg=%.3f roll_deg=%.3f v_bat=%.3f lora_rssi=%d error_flag=%u\n",
                data.gatewayId,
                data.nodeId,
                static_cast<unsigned long>(data.packetId),
                static_cast<unsigned long>(data.timestampMs),
                data.soilAdc,
                data.soilPercent,
                data.betaDeg,
                data.betaDotDegPerHour,
                data.vibrationRmsG,
                data.pitchDeg,
                data.rollDeg,
                data.batteryV,
                data.rssi,
                data.errorFlags);
  Serial.printf("[ANALYSIS] valid=%s u_kpa=%.3f tau_kpa=%.3f sigma_n_kpa=%.3f "
                "sigma_effective_kpa=%.3f tau_f_kpa=%.3f fs=%.5f di=%.5f epsilon_star=%.5f\n",
                result.valid ? "true" : "false",
                result.porePressureKpa,
                result.tauDriveKpa,
                result.sigmaNormalKpa,
                result.sigmaEffectiveKpa,
                result.shearStrengthKpa,
                result.factorOfSafety,
                result.dynamicIndex,
                result.strainIndex);
  Serial.printf("[STATE] alert_level=%s risk_status=%s warning_message=%s battery_status=%s "
                "ota_locked=%s duty_cycle_mode=%s sleep_duration_sec=%lu\n",
                result.alertLevel.c_str(),
                result.riskStatus.c_str(),
                result.warningMessage.c_str(),
                result.batteryStatus.c_str(),
                result.otaLocked ? "true" : "false",
                result.dutyCycleMode.c_str(),
                static_cast<unsigned long>(result.nextSleepSeconds));
}

void processIncomingPacket(const String &packet, int rssi) {
  Protocol::SensorPacket data;
  if (!Protocol::parseDataPacket(packet, data)) {
    Serial.printf("[LoRa] invalid packet: %s\n", packet.c_str());
    return;
  }

  data.rssi = rssi;
  if (data.gatewayId != Config::GATEWAY_ID) {
    Serial.printf("[LoRa] packet for gateway %u ignored\n", data.gatewayId);
    return;
  }

  const bool duplicate = lastPacketIdByNode[data.nodeId] == data.packetId;
  const Analysis::Result result = Analysis::evaluate(data);

  if (!duplicate) {
    lastPacketIdByNode[data.nodeId] = data.packetId;
    const String payload = buildThingsBoardPayload(data, result);
    printAllVariables(data, result);
    sendAck(data, result, true);
    publishTelemetry(payload);
  } else {
    Serial.printf("[LoRa] duplicate node=%u pid=%lu, ACK only\n",
                  data.nodeId,
                  static_cast<unsigned long>(data.packetId));
    sendAck(data, result, false);
  }
}

void pollLora() {
  if (!loraReady) {
    return;
  }

  if (xSemaphoreTake(loraMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return;
  }

  const int packetSize = LoRa.parsePacket();
  if (packetSize <= 0) {
    xSemaphoreGive(loraMutex);
    return;
  }

  ReceivedLoraPacket received = {};
  size_t index = 0;
  while (LoRa.available() && index < sizeof(received.payload) - 1) {
    received.payload[index++] = static_cast<char>(LoRa.read());
  }
  received.payload[index] = '\0';
  received.rssi = LoRa.packetRssi();
  xSemaphoreGive(loraMutex);

  if (xQueueSend(loraPacketQueue, &received, 0) != pdTRUE) {
    Serial.println("[RTOS] LoRa queue full, packet dropped");
  }
}

void printHelp() {
  Serial.println("Commands:");
  Serial.println("  SET_SLEEP <minutes>  Override sleep returned to node in ACK");
  Serial.println("  CLEAR_SLEEP          Use adaptive sleep again");
  Serial.println("  STATUS               Print WiFi/MQTT state");
}

void handleSerialCommand() {
  if (!Serial.available()) {
    return;
  }

  String line = Serial.readStringUntil('\n');
  line.trim();
  line.toUpperCase();

  if (line.length() == 0) {
    return;
  }
  if (line == "HELP") {
    printHelp();
  } else if (line.startsWith("SET_SLEEP ")) {
    const uint32_t minutes = static_cast<uint32_t>(line.substring(10).toInt());
    const uint32_t seconds = minutes * 60UL;
    if (seconds >= Config::SLEEP_DANGER_SEC && seconds <= Config::SLEEP_NORMAL_SEC) {
      sleepOverrideSeconds = seconds;
      Serial.printf("[CMD] sleep override=%lu minutes\n", static_cast<unsigned long>(minutes));
    } else {
      Serial.println("[CMD] invalid sleep. Use 5..30 minutes");
    }
  } else if (line == "CLEAR_SLEEP") {
    sleepOverrideSeconds = 0;
    Serial.println("[CMD] adaptive sleep restored");
  } else if (line == "STATUS") {
    Serial.printf("[STATUS] wifi=%s mqtt=%s sleep_override=%lu\n",
                  WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED",
                  mqtt.connected() ? "CONNECTED" : "DISCONNECTED",
                  static_cast<unsigned long>(sleepOverrideSeconds));
  } else {
    Serial.println("[CMD] unknown. Type HELP");
  }
}

void loraTask(void *) {
  for (;;) {
    pollLora();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void processingTask(void *) {
  ReceivedLoraPacket received = {};
  for (;;) {
    if (xQueueReceive(loraPacketQueue, &received, portMAX_DELAY) == pdTRUE) {
      Serial.printf("[LoRa] RX rssi=%d payload=%s\n", received.rssi, received.payload);
      processIncomingPacket(String(received.payload), received.rssi);
    }
  }
}

void networkTask(void *) {
  uint32_t lastMqttRetryMs = 0;
  for (;;) {
    serviceWifi();

    if (WiFi.status() != WL_CONNECTED && !wifiConnecting &&
        millis() - lastWifiAttemptMs > 10000) {
      connectWifi();
    }

    if (WiFi.status() == WL_CONNECTED &&
        xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      if (!mqtt.connected() && millis() - lastMqttRetryMs > 5000) {
        lastMqttRetryMs = millis();
        connectMqtt();
      }
      if (mqtt.connected()) {
        mqtt.loop();
      }
      xSemaphoreGive(mqttMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void serialTask(void *) {
  for (;;) {
    handleSerialCommand();
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

} // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n[%s GATEWAY] boot\n", Config::PROJECT_TAG);

  loraPacketQueue = xQueueCreate(8, sizeof(ReceivedLoraPacket));
  loraMutex = xSemaphoreCreateMutex();
  mqttMutex = xSemaphoreCreateMutex();
  if (loraPacketQueue == nullptr || loraMutex == nullptr || mqttMutex == nullptr) {
    Serial.println("[RTOS] init failed");
    while (true) {
      delay(1000);
    }
  }

  loraReady = initLora();
  mqtt.setServer(Config::TB_HOST, Config::TB_PORT);
  mqtt.setBufferSize(2048);
  connectWifi();
  printHelp();

  xTaskCreate(loraTask, "lora_rx", 4096, nullptr, 3, nullptr);
  xTaskCreate(processingTask, "analysis", 8192, nullptr, 2, nullptr);
  xTaskCreate(networkTask, "network", 6144, nullptr, 1, nullptr);
  xTaskCreate(serialTask, "serial", 3072, nullptr, 1, nullptr);
  Serial.println("[RTOS] tasks started: lora_rx, analysis, network, serial");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
