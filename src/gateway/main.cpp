#include <Arduino.h>
#include <LoRa.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <WiFi.h>
#include <math.h>

#include "analysis.h"
#include "project_config.h"
#include "protocol.h"

namespace {

struct OfflineTelemetry {
  char payload[Config::GATEWAY_TELEMETRY_MAX_BYTES];
  uint8_t nodeId = 0;
  uint32_t packetId = 0;
};

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

static constexpr uint8_t RECENT_PACKET_HISTORY = 4;
uint32_t recentPacketIds[256][RECENT_PACKET_HISTORY] = {{0}};
uint8_t recentPacketCount[256] = {0};
uint8_t recentPacketWriteIndex[256] = {0};
uint32_t sleepOverrideSeconds = 0;
bool loraReady = false;
bool wifiConnecting = false;
bool wifiWasConnected = false;
bool mqttWasConnected = false;
uint32_t wifiAttemptStartedMs = 0;
uint32_t lastWifiAttemptMs = 0;
uint32_t lastMqttAttemptMs = 0;

OfflineTelemetry offlineQueue[Config::GATEWAY_OFFLINE_QUEUE_CAPACITY];
size_t offlineHead = 0;
size_t offlineCount = 0;
uint32_t offlineDropped = 0;

bool isDuplicatePacket(uint8_t nodeId, uint32_t packetId) {
  for (uint8_t i = 0; i < recentPacketCount[nodeId]; ++i) {
    if (recentPacketIds[nodeId][i] == packetId) {
      return true;
    }
  }
  return false;
}

void rememberPacket(uint8_t nodeId, uint32_t packetId) {
  recentPacketIds[nodeId][recentPacketWriteIndex[nodeId]] = packetId;
  recentPacketWriteIndex[nodeId] =
      (recentPacketWriteIndex[nodeId] + 1) % RECENT_PACKET_HISTORY;
  if (recentPacketCount[nodeId] < RECENT_PACKET_HISTORY) {
    ++recentPacketCount[nodeId];
  }
}

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

void appendJsonSeparator(String &json) {
  if (!json.endsWith("{")) {
    json += ',';
  }
}

void appendJsonNumber(String &json, const char *key, float value, uint8_t decimals) {
  appendJsonSeparator(json);
  json += '"';
  json += key;
  json += "\":";
  json += isfinite(value) ? String(value, static_cast<unsigned int>(decimals)) : "null";
}

void appendJsonNumber(String &json, const char *key, uint32_t value) {
  appendJsonSeparator(json);
  json += '"';
  json += key;
  json += "\":";
  json += String(value);
}

void appendJsonNumber(String &json, const char *key, int32_t value) {
  appendJsonSeparator(json);
  json += '"';
  json += key;
  json += "\":";
  json += String(value);
}

void appendJsonText(String &json, const char *key, const String &value) {
  appendJsonSeparator(json);
  json += '"';
  json += key;
  json += "\":\"";
  json += jsonEscape(value);
  json += '"';
}

void appendJsonBool(String &json, const char *key, bool value) {
  appendJsonSeparator(json);
  json += '"';
  json += key;
  json += "\":";
  json += value ? "true" : "false";
}

bool initLora() {
  SPI.begin(Config::LORA_SCK_PIN,
            Config::LORA_MISO_PIN,
            Config::LORA_MOSI_PIN,
            Config::LORA_CS_PIN);
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
  Serial.println("[LoRa] initialized successfully");
  return true;
}

void startWifiConnection() {
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
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected) {
    wifiConnecting = false;
    if (!wifiWasConnected) {
      Serial.printf("[WiFi] connected ip=%s\n", WiFi.localIP().toString().c_str());
    }
    wifiWasConnected = true;
    return;
  }

  if (wifiWasConnected) {
    Serial.println("[WiFi] disconnected");
    wifiWasConnected = false;
  }

  if (wifiConnecting && millis() - wifiAttemptStartedMs >= 20000UL) {
    wifiConnecting = false;
    WiFi.disconnect();
    Serial.println("[WiFi] connection failed");
  }

  if (!wifiConnecting && millis() - lastWifiAttemptMs >= 10000UL) {
    startWifiConnection();
  }
}

bool connectMqtt() {
  if (mqtt.connected()) {
    return true;
  }
  if (WiFi.status() != WL_CONNECTED || millis() - lastMqttAttemptMs < 5000UL) {
    return false;
  }

  lastMqttAttemptMs = millis();
  const String clientId = String("esp32-gateway-") +
                          String(static_cast<uint32_t>(ESP.getEfuseMac()), HEX);
  const bool connected = mqtt.connect(clientId.c_str(), Config::TB_TOKEN, nullptr);
  if (connected) {
    Serial.println("[MQTT] connected to ThingsBoard");
  } else {
    Serial.printf("[MQTT] connection failed rc=%d\n", mqtt.state());
  }
  return connected;
}

bool enqueueTelemetry(const String &payload, uint8_t nodeId, uint32_t packetId) {
  if (payload.length() >= Config::GATEWAY_TELEMETRY_MAX_BYTES) {
    ++offlineDropped;
    Serial.printf("[BUFFER] telemetry too large node=%u packet_id=%lu dropped=%lu\n",
                  nodeId,
                  static_cast<unsigned long>(packetId),
                  static_cast<unsigned long>(offlineDropped));
    return false;
  }
  if (offlineCount >= Config::GATEWAY_OFFLINE_QUEUE_CAPACITY) {
    ++offlineDropped;
    Serial.printf("[BUFFER] full capacity=%u node=%u packet_id=%lu dropped=%lu\n",
                  static_cast<unsigned int>(Config::GATEWAY_OFFLINE_QUEUE_CAPACITY),
                  nodeId,
                  static_cast<unsigned long>(packetId),
                  static_cast<unsigned long>(offlineDropped));
    return false;
  }

  const size_t tail = (offlineHead + offlineCount) % Config::GATEWAY_OFFLINE_QUEUE_CAPACITY;
  payload.toCharArray(offlineQueue[tail].payload, sizeof(offlineQueue[tail].payload));
  offlineQueue[tail].nodeId = nodeId;
  offlineQueue[tail].packetId = packetId;
  ++offlineCount;
  return true;
}

void serviceNetwork() {
  serviceWifi();
  if (WiFi.status() != WL_CONNECTED) {
    mqttWasConnected = false;
    return;
  }

  if (!mqtt.connected()) {
    if (mqttWasConnected) {
      Serial.println("[MQTT] disconnected");
      mqttWasConnected = false;
    }
    connectMqtt();
    return;
  }

  mqttWasConnected = true;
  mqtt.loop();
  if (offlineCount == 0) {
    return;
  }

  OfflineTelemetry &entry = offlineQueue[offlineHead];
  const bool published = mqtt.publish(Config::TB_TELEMETRY_TOPIC, entry.payload);
  Serial.printf("[MQTT] publish %s node=%u packet_id=%lu queued=%u\n",
                published ? "OK" : "FAILED",
                entry.nodeId,
                static_cast<unsigned long>(entry.packetId),
                static_cast<unsigned int>(offlineCount));
  if (published) {
    offlineHead = (offlineHead + 1) % Config::GATEWAY_OFFLINE_QUEUE_CAPACITY;
    --offlineCount;
  }
}

bool sensorValuesInRange(const Protocol::SensorPacket &data) {
  return isfinite(data.soilPercent) &&
         data.soilPercent >= Config::SOIL_PERCENT_MIN &&
         data.soilPercent <= Config::SOIL_PERCENT_MAX &&
         isfinite(data.betaDeg) && data.betaDeg >= Config::BETA_MIN_DEG &&
         data.betaDeg <= Config::BETA_MAX_DEG &&
         isfinite(data.betaDotDegPerHour) &&
         fabsf(data.betaDotDegPerHour) <= Config::BETA_DOT_SANITY_MAX_DEG_PER_HOUR &&
         isfinite(data.vibrationRmsG) && data.vibrationRmsG >= Config::A_RMS_MIN_G &&
         data.vibrationRmsG <= Config::A_RMS_MAX_G;
}

String buildThingsBoardPayload(const Protocol::SensorPacket &data,
                               const Analysis::Result &analysis) {
  String json;
  json.reserve(Config::GATEWAY_TELEMETRY_MAX_BYTES);
  json += '{';

  appendJsonText(json, "project", Config::PROJECT_TAG);
  appendJsonText(json, "parameter_profile", Config::PARAMETER_PROFILE);
  appendJsonNumber(json, "gateway_id", static_cast<uint32_t>(data.gatewayId));
  appendJsonNumber(json, "node_id", static_cast<uint32_t>(data.nodeId));
  appendJsonNumber(json, "packet_id", data.packetId);
  appendJsonNumber(json, "timestamp_ms", data.timestampMs);
  appendJsonNumber(json, "soil_adc_filtered", static_cast<int32_t>(data.soilAdc));
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
  appendJsonNumber(json, "fs", analysis.factorOfSafety, 5);
  appendJsonNumber(json, "di", analysis.dynamicIndex, 5);
  appendJsonNumber(json, "epsilon_star", analysis.strainIndex, 5);
  appendJsonText(json, "alert_level", analysis.alertLevel);
  appendJsonText(json, "risk_status", analysis.riskStatus);
  appendJsonText(json, "warning_message", analysis.warningMessage);
  appendJsonText(json, "duty_cycle_mode", analysis.dutyCycleMode);
  appendJsonNumber(json, "sleep_duration_sec", analysis.nextSleepSeconds);
  appendJsonText(json, "battery_status", analysis.batteryStatus);
  appendJsonBool(json, "ota_supported", analysis.otaSupported);
  appendJsonBool(json, "ota_locked", analysis.otaLocked);
  json += '}';
  return json;
}

void printAllVariables(const Protocol::SensorPacket &data, const Analysis::Result &result) {
  Serial.printf("[RAW] gateway_id=%u node_id=%u packet_id=%lu timestamp_ms=%lu "
                "soil_adc_filtered=%d h_soil=%.2f beta_deg=%.3f "
                "beta_dot_deg_per_hour=%.4f a_rms_g=%.5f pitch_deg=%.3f "
                "roll_deg=%.3f v_bat=%.3f lora_rssi=%d error_flag=%u error_text=%s\n",
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
                data.errorFlags,
                Analysis::errorFlagsToText(data.errorFlags));

  Serial.print("[ANALYSIS] ");
  Serial.printf("valid=%s u_kpa=%.3f tau_kpa=%.3f sigma_n_kpa=%.3f "
                "sigma_effective_kpa=%.3f tau_f_kpa=%.3f ",
                result.valid ? "true" : "false",
                result.porePressureKpa,
                result.tauDriveKpa,
                result.sigmaNormalKpa,
                result.sigmaEffectiveKpa,
                result.shearStrengthKpa);
  if (result.valid) {
    Serial.printf("fs=%.5f di=%.5f epsilon_star=%.5f\n",
                  result.factorOfSafety,
                  result.dynamicIndex,
                  result.strainIndex);
  } else {
    Serial.println("fs=invalid di=invalid epsilon_star=invalid");
  }

  Serial.printf("[STATE] alert_level=%s risk_status=%s warning_message=%s "
                "battery_status=%s ota_supported=%s ota_locked=%s "
                "duty_cycle_mode=%s sleep_duration_sec=%lu\n",
                result.alertLevel.c_str(),
                result.riskStatus.c_str(),
                result.warningMessage.c_str(),
                result.batteryStatus.c_str(),
                result.otaSupported ? "true" : "false",
                result.otaLocked ? "true" : "false",
                result.dutyCycleMode.c_str(),
                static_cast<unsigned long>(result.nextSleepSeconds));
}

void sendAck(const Protocol::SensorPacket &data,
             const Analysis::Result &analysis,
             const char *status) {
  Protocol::AckPacket ack;
  ack.gatewayId = data.gatewayId;
  ack.nodeId = data.nodeId;
  ack.packetId = data.packetId;
  ack.status = status;
  ack.alertLevel = analysis.alertLevel;
  ack.sleepSeconds = sleepOverrideSeconds > 0 ? sleepOverrideSeconds
                                             : analysis.nextSleepSeconds;

  const String packet = Protocol::buildAckPacket(ack);
  LoRa.idle();
  LoRa.beginPacket();
  LoRa.print(packet);
  LoRa.endPacket();
  LoRa.receive();
  Serial.printf("[LoRa] ACK node=%u packet_id=%lu status=%s sleep_duration_sec=%lu\n",
                ack.nodeId,
                static_cast<unsigned long>(ack.packetId),
                ack.status.c_str(),
                static_cast<unsigned long>(ack.sleepSeconds));
}

void processGatewayPacket(const String &packet, int rssi) {
  Protocol::SensorPacket data;
  if (!Protocol::parseDataPacket(packet, data)) {
    Serial.println("[LoRa] invalid header, CRC, or packet fields");
    return;
  }
  if (data.gatewayId != Config::GATEWAY_ID || data.nodeId == 0) {
    Serial.printf("[LoRa] invalid identity gateway=%u node=%u\n",
                  data.gatewayId,
                  data.nodeId);
    return;
  }

  data.rssi = rssi;
  if (!sensorValuesInRange(data)) {
    data.errorFlags |= Protocol::ERR_DATA_RANGE;
  }
  if (!isfinite(data.batteryV) || data.batteryV < Config::BATTERY_SANITY_MIN_V ||
      data.batteryV > Config::BATTERY_SANITY_MAX_V) {
    data.errorFlags |= Protocol::ERR_BATTERY;
  }

  const bool duplicate = isDuplicatePacket(data.nodeId, data.packetId);
  const Analysis::Result result = Analysis::evaluate(data);
  if (duplicate) {
    Serial.printf("[LoRa] duplicate node=%u packet_id=%lu\n",
                  data.nodeId,
                  static_cast<unsigned long>(data.packetId));
    sendAck(data, result, "DUPLICATE");
    return;
  }

  const String payload = buildThingsBoardPayload(data, result);
  if (!enqueueTelemetry(payload, data.nodeId, data.packetId)) {
    sendAck(data, result, "BUFFER_FULL");
    return;
  }

  rememberPacket(data.nodeId, data.packetId);
  printAllVariables(data, result);
  sendAck(data, result, "OK");
}

void serviceGatewayRadio() {
  if (!loraReady) {
    return;
  }
  const int packetSize = LoRa.parsePacket();
  if (packetSize <= 0) {
    return;
  }

  String packet;
  packet.reserve(static_cast<unsigned int>(packetSize));
  while (LoRa.available()) {
    packet += static_cast<char>(LoRa.read());
  }
  const int rssi = LoRa.packetRssi();
  processGatewayPacket(packet, rssi);
}

void printHelp() {
  Serial.println("Commands:");
  Serial.println("  SET_SLEEP <minutes>  Override sleep returned in ACK");
  Serial.println("  CLEAR_SLEEP          Restore adaptive sleep");
  Serial.println("  STATUS               Print radio/network/buffer state");
}

void serviceSerial() {
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
      Serial.printf("[CMD] sleep override=%lu minutes\n",
                    static_cast<unsigned long>(minutes));
    } else {
      Serial.println("[CMD] invalid sleep; allowed range is 5..30 minutes");
    }
  } else if (line == "CLEAR_SLEEP") {
    sleepOverrideSeconds = 0;
    Serial.println("[CMD] adaptive sleep restored");
  } else if (line == "STATUS") {
    Serial.printf("[STATUS] lora=%s wifi=%s mqtt=%s queue=%u/%u dropped=%lu "
                  "sleep_override_sec=%lu profile=%s\n",
                  loraReady ? "READY" : "FAILED",
                  WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED",
                  mqtt.connected() ? "CONNECTED" : "DISCONNECTED",
                  static_cast<unsigned int>(offlineCount),
                  static_cast<unsigned int>(Config::GATEWAY_OFFLINE_QUEUE_CAPACITY),
                  static_cast<unsigned long>(offlineDropped),
                  static_cast<unsigned long>(sleepOverrideSeconds),
                  Config::PARAMETER_PROFILE);
  } else {
    Serial.println("[CMD] unknown; type HELP");
  }
}

} // namespace

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(20);
  delay(300);
  Serial.printf("\n[%s GATEWAY] boot architecture=SUPERLOOP profile=%s\n",
                Config::PROJECT_TAG,
                Config::PARAMETER_PROFILE);

  loraReady = initLora();
  mqtt.setServer(Config::TB_HOST, Config::TB_PORT);
  mqtt.setBufferSize(2048);
  startWifiConnection();
  printHelp();
}

void loop() {
  serviceSerial();
  serviceGatewayRadio();
  serviceNetwork();
  delay(2);
}
