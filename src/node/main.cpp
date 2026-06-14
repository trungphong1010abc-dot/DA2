#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>
#include <Wire.h>
#include <esp_sleep.h>
#include <math.h>

#include "project_config.h"
#include "protocol.h"

namespace {

struct SoilReadout {
  int adcFiltered = 0;
  float moisturePercent = 0.0f;
  bool ok = false;
};

struct MpuReadout {
  float betaDeg = 0.0f;
  float betaDotDegPerHour = 0.0f;
  float vibrationRmsG = 0.0f;
  float pitchDeg = 0.0f;
  float rollDeg = 0.0f;
  bool ok = false;
};

RTC_DATA_ATTR uint32_t rtcPacketId = 0;
RTC_DATA_ATTR uint32_t rtcSleepSeconds = Config::SLEEP_NORMAL_SEC;
RTC_DATA_ATTR float rtcPreviousBetaDeg = 0.0f;
RTC_DATA_ATTR bool rtcHasPreviousBeta = false;

bool loraReady = false;
bool mpuReady = false;

float clampFloat(float value, float minValue, float maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

void powerSensors(bool enabled) {
  if (Config::SENSOR_POWER_PIN < 0) {
    return;
  }
  pinMode(Config::SENSOR_POWER_PIN, OUTPUT);
  digitalWrite(Config::SENSOR_POWER_PIN, enabled ? HIGH : LOW);
}

void configureAdc() {
  analogReadResolution(12);
  analogSetPinAttenuation(Config::SOIL_ADC_PIN, ADC_11db);
  analogSetPinAttenuation(Config::BATTERY_ADC_PIN, ADC_11db);
}

void sortIntArray(int *values, uint8_t count) {
  for (uint8_t i = 1; i < count; ++i) {
    const int key = values[i];
    int j = i - 1;
    while (j >= 0 && values[j] > key) {
      values[j + 1] = values[j];
      --j;
    }
    values[j + 1] = key;
  }
}

SoilReadout readSoil() {
  SoilReadout result;
  int samples[Config::SOIL_SAMPLE_COUNT];
  uint8_t validCount = 0;

  powerSensors(true);
  delay(Config::SENSOR_WARMUP_MS);

  for (uint8_t i = 0; i < Config::SOIL_SAMPLE_COUNT; ++i) {
    const int raw = analogRead(Config::SOIL_ADC_PIN);
    if (raw >= Config::SOIL_ADC_MIN_VALID && raw <= Config::SOIL_ADC_MAX_VALID) {
      samples[validCount++] = raw;
    }
    delay(25);
  }

  if (validCount < 3) {
    result.ok = false;
    return result;
  }

  sortIntArray(samples, validCount);
  result.adcFiltered = samples[validCount / 2];

  const float denominator = static_cast<float>(Config::SOIL_ADC_DRY - Config::SOIL_ADC_WET);
  if (fabsf(denominator) < 1.0f) {
    result.ok = false;
    return result;
  }

  result.moisturePercent =
      (static_cast<float>(Config::SOIL_ADC_DRY - result.adcFiltered) * 100.0f) / denominator;
  result.moisturePercent = clampFloat(result.moisturePercent, 0.0f, 100.0f);
  result.ok = true;
  return result;
}

bool mpuWriteReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(Config::MPU6050_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool mpuReadReg(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(Config::MPU6050_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(Config::MPU6050_ADDR, static_cast<uint8_t>(1)) != 1) {
    return false;
  }
  value = Wire.read();
  return true;
}

bool readAccel(float &axG, float &ayG, float &azG) {
  Wire.beginTransmission(Config::MPU6050_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(Config::MPU6050_ADDR, static_cast<uint8_t>(6)) != 6) {
    return false;
  }

  const int16_t axRaw = static_cast<int16_t>((Wire.read() << 8) | Wire.read());
  const int16_t ayRaw = static_cast<int16_t>((Wire.read() << 8) | Wire.read());
  const int16_t azRaw = static_cast<int16_t>((Wire.read() << 8) | Wire.read());

  axG = static_cast<float>(axRaw) / 16384.0f;
  ayG = static_cast<float>(ayRaw) / 16384.0f;
  azG = static_cast<float>(azRaw) / 16384.0f;
  return true;
}

bool initMpu6050() {
  Wire.begin(Config::I2C_SDA_PIN, Config::I2C_SCL_PIN);
  Wire.setClock(400000);

  uint8_t whoAmI = 0;
  if (!mpuReadReg(0x75, whoAmI) || (whoAmI != 0x68 && whoAmI != 0x70)) {
    Serial.printf("[MPU] WHO_AM_I invalid: 0x%02X\n", whoAmI);
    return false;
  }

  bool ok = true;
  ok &= mpuWriteReg(0x6B, 0x00); // Wake up.
  ok &= mpuWriteReg(0x1C, 0x00); // ACCEL_CONFIG: +-2g.
  ok &= mpuWriteReg(0x1A, 0x03); // DLPF ~44 Hz.
  ok &= mpuWriteReg(0x19, 0x04); // Sample rate divider.
  delay(100);
  return ok;
}

MpuReadout readMpuWindow() {
  MpuReadout result;
  if (!mpuReady) {
    return result;
  }

  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  if (!readAccel(ax, ay, az)) {
    return result;
  }

  const float alpha = 0.85f;
  float axFiltered = ax;
  float ayFiltered = ay;
  float azFiltered = az;
  float firstBeta = 0.0f;
  float lastBeta = 0.0f;
  float vibrationSquaredSum = 0.0f;
  uint16_t sampleCount = 0;
  const uint32_t startMs = millis();
  uint32_t lastSampleMs = startMs;

  while (millis() - startMs <= Config::MPU_WINDOW_MS) {
    const uint32_t nowMs = millis();
    if (nowMs - lastSampleMs < Config::MPU_SAMPLE_INTERVAL_MS) {
      delay(5);
      continue;
    }
    lastSampleMs = nowMs;

    if (!readAccel(ax, ay, az)) {
      result.ok = false;
      return result;
    }

    axFiltered = alpha * axFiltered + (1.0f - alpha) * ax;
    ayFiltered = alpha * ayFiltered + (1.0f - alpha) * ay;
    azFiltered = alpha * azFiltered + (1.0f - alpha) * az;

    const float pitchRad = atan2f(-axFiltered, sqrtf(ayFiltered * ayFiltered + azFiltered * azFiltered));
    const float rollRad = atan2f(ayFiltered, sqrtf(axFiltered * axFiltered + azFiltered * azFiltered));
    const float pitchDeg = pitchRad * 180.0f / PI;
    const float rollDeg = rollRad * 180.0f / PI;
    const float betaDeg = sqrtf(pitchDeg * pitchDeg + rollDeg * rollDeg);

    const float axVib = ax - axFiltered;
    const float ayVib = ay - ayFiltered;
    const float azVib = az - azFiltered;
    const float vibration = sqrtf(axVib * axVib + ayVib * ayVib + azVib * azVib);
    vibrationSquaredSum += vibration * vibration;

    if (sampleCount == 0) {
      firstBeta = betaDeg;
    }
    lastBeta = betaDeg;
    result.pitchDeg = pitchDeg;
    result.rollDeg = rollDeg;
    ++sampleCount;
  }

  if (sampleCount < 3) {
    result.ok = false;
    return result;
  }

  result.betaDeg = lastBeta;
  result.vibrationRmsG = sqrtf(vibrationSquaredSum / static_cast<float>(sampleCount));

  const float windowHours = (Config::MPU_WINDOW_MS / 1000.0f) / 3600.0f;
  float betaDotWindow = 0.0f;
  if (windowHours > 0.0f) {
    betaDotWindow = (lastBeta - firstBeta) / windowHours;
  }

  if (rtcHasPreviousBeta) {
    const float cycleHours =
        (static_cast<float>(rtcSleepSeconds) + Config::MPU_WINDOW_MS / 1000.0f) / 3600.0f;
    result.betaDotDegPerHour = (lastBeta - rtcPreviousBetaDeg) / cycleHours;
  } else {
    result.betaDotDegPerHour = betaDotWindow;
  }

  result.ok = true;
  return result;
}

float readBatteryVoltage() {
  uint32_t sum = 0;
  constexpr uint8_t sampleCount = 16;
  for (uint8_t i = 0; i < sampleCount; ++i) {
    sum += analogRead(Config::BATTERY_ADC_PIN);
    delay(5);
  }
  const float raw = static_cast<float>(sum) / static_cast<float>(sampleCount);
  const float adcVoltage = raw * Config::ADC_REFERENCE_V / 4095.0f;
  return adcVoltage *
         ((Config::BAT_DIVIDER_R_TOP_OHM + Config::BAT_DIVIDER_R_BOTTOM_OHM) /
          Config::BAT_DIVIDER_R_BOTTOM_OHM);
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

String readLoraPacket() {
  String packet;
  const int packetSize = LoRa.parsePacket();
  if (packetSize <= 0) {
    return packet;
  }

  while (LoRa.available()) {
    packet += static_cast<char>(LoRa.read());
  }
  return packet;
}

bool sendTelemetryAndWaitAck(const Protocol::SensorPacket &data, Protocol::AckPacket &ack) {
  if (!loraReady) {
    return false;
  }

  const String packet = Protocol::buildDataPacket(data);
  for (uint8_t retry = 0; retry < Config::LORA_MAX_RETRY; ++retry) {
    Serial.printf("[LoRa] TX retry=%u payload=%s\n", retry, packet.c_str());
    LoRa.idle();
    LoRa.beginPacket();
    LoRa.print(packet);
    LoRa.endPacket();
    LoRa.receive();

    const uint32_t start = millis();
    while (millis() - start < Config::ACK_TIMEOUT_MS) {
      const String incoming = readLoraPacket();
      if (incoming.length() == 0) {
        delay(10);
        continue;
      }

      Protocol::AckPacket parsedAck;
      if (Protocol::parseAckPacket(incoming, parsedAck) &&
          parsedAck.gatewayId == Config::GATEWAY_ID &&
          parsedAck.nodeId == Config::NODE_ID &&
          parsedAck.packetId == data.packetId) {
        ack = parsedAck;
        Serial.printf("[LoRa] ACK status=%s sleep=%.1f minutes alert=%s\n",
                      ack.status.c_str(),
                      ack.sleepSeconds / 60.0f,
                      ack.alertLevel.c_str());
        return true;
      }

      Serial.printf("[LoRa] ignored packet: %s\n", incoming.c_str());
    }
  }

  return false;
}

uint32_t localSleepFallback(const Protocol::SensorPacket &data) {
  if ((data.errorFlags & (Protocol::ERR_SOIL | Protocol::ERR_MPU | Protocol::ERR_LORA)) != 0) {
    return Config::SLEEP_SENSOR_ERROR_SEC;
  }
  if (data.batteryV > 0.1f && data.batteryV < Config::BATTERY_CRITICAL_V) {
    return Config::SLEEP_SENSOR_ERROR_SEC;
  }
  if (data.batteryV > 0.1f && data.batteryV < Config::BATTERY_LOW_V) {
    return Config::SLEEP_WARNING_SEC;
  }
  if (data.soilPercent >= 85.0f ||
      fabsf(data.betaDotDegPerHour) >= Config::BETA_DOT_CRIT_DEG_PER_HOUR ||
      data.vibrationRmsG >= Config::A_RMS_CRIT_G) {
    return Config::SLEEP_DANGER_SEC;
  }
  if (data.soilPercent >= Config::MOISTURE_DANGER_START_PERCENT ||
      fabsf(data.betaDotDegPerHour) >= Config::BETA_DOT_CRIT_DEG_PER_HOUR * 0.5f ||
      data.vibrationRmsG >= Config::A_RMS_CRIT_G * 0.5f) {
    return Config::SLEEP_WARNING_SEC;
  }
  return Config::SLEEP_NORMAL_SEC;
}

void sleepOrDelay(uint32_t sleepSeconds) {
  sleepSeconds = constrain(sleepSeconds, Config::SLEEP_DANGER_SEC, Config::SLEEP_NORMAL_SEC);
  rtcSleepSeconds = sleepSeconds;

  Serial.printf("[POWER] next cycle in %.1f minutes\n", sleepSeconds / 60.0f);
  Serial.flush();

  powerSensors(false);
  if (Config::NODE_ENABLE_DEEP_SLEEP) {
    LoRa.sleep();
    esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(sleepSeconds) * 1000000ULL);
    esp_deep_sleep_start();
  }

  delay(sleepSeconds * 1000UL);
}

void runCycle() {
  Protocol::SensorPacket data;
  data.gatewayId = Config::GATEWAY_ID;
  data.nodeId = Config::NODE_ID;
  data.packetId = ++rtcPacketId;
  data.timestampMs = millis();

  const SoilReadout soil = readSoil();
  data.soilAdc = soil.adcFiltered;
  data.soilPercent = soil.moisturePercent;
  if (!soil.ok) {
    data.errorFlags |= Protocol::ERR_SOIL;
  }

  const MpuReadout mpu = readMpuWindow();
  data.betaDeg = mpu.betaDeg;
  data.betaDotDegPerHour = mpu.betaDotDegPerHour;
  data.vibrationRmsG = mpu.vibrationRmsG;
  data.pitchDeg = mpu.pitchDeg;
  data.rollDeg = mpu.rollDeg;
  if (!mpu.ok) {
    data.errorFlags |= Protocol::ERR_MPU;
  }

  data.batteryV = readBatteryVoltage();
  if (data.batteryV > 0.1f && data.batteryV < Config::BATTERY_CRITICAL_V) {
    data.errorFlags |= Protocol::ERR_BATTERY;
  }

  Protocol::AckPacket ack;
  bool ackOk = sendTelemetryAndWaitAck(data, ack);
  uint32_t nextSleep = localSleepFallback(data);
  if (!ackOk) {
    data.errorFlags |= Protocol::ERR_LORA;
    Serial.println("[LoRa] no ACK, using local sleep fallback");
  } else if (ack.sleepSeconds > 0) {
    nextSleep = ack.sleepSeconds;
  }

  if (mpu.ok) {
    rtcPreviousBetaDeg = data.betaDeg;
    rtcHasPreviousBeta = true;
  }

  Serial.printf("[DATA] adc=%d h=%.2f beta=%.3f bdot=%.4f arms=%.5f vbat=%.3f err=%u\n",
                data.soilAdc,
                data.soilPercent,
                data.betaDeg,
                data.betaDotDegPerHour,
                data.vibrationRmsG,
                data.batteryV,
                data.errorFlags);
  sleepOrDelay(nextSleep);
}

} // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n[%s NODE] boot packetId=%lu\n",
                Config::PROJECT_TAG,
                static_cast<unsigned long>(rtcPacketId));

  powerSensors(true);
  configureAdc();
  mpuReady = initMpu6050();
  loraReady = initLora();
}

void loop() {
  runCycle();
}
