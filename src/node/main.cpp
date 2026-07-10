#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>
#include <Wire.h>
#include <esp_sleep.h>
#include <math.h>

#include "project_config.h"
#include "protocol.h"
#include "analysis.h"

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
RTC_DATA_ATTR bool rtcPendingPacketValid = false;
RTC_DATA_ATTR char rtcPendingPacket[640] = {0};

bool loraReady = false;
bool mpuReady = false;
uint32_t previousBetaSampleMs = 0;

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
}

void printMpuCalibrationProfile() {
  Serial.println("[CALIB] MPU6050 configured calibration profile");
  Serial.printf("[CALIB] accel_offset_g ax=%.3f ay=%.3f az=%.3f\n",
                Config::MPU_AX_OFFSET_G,
                Config::MPU_AY_OFFSET_G,
                Config::MPU_AZ_OFFSET_G);
  Serial.printf("[CALIB] gyro_offset_dps gx=%.3f gy=%.3f gz=%.3f\n",
                Config::MPU_GX_OFFSET_DPS,
                Config::MPU_GY_OFFSET_DPS,
                Config::MPU_GZ_OFFSET_DPS);
  Serial.printf("[CALIB] angle_zero_deg roll0=%.3f pitch0=%.3f\n",
                Config::MPU_ROLL_ZERO_DEG,
                Config::MPU_PITCH_ZERO_DEG);
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
  uint8_t consecutiveErrors = 0;

  powerSensors(true);
  delay(Config::SENSOR_WARMUP_MS);

  for (uint8_t i = 0; i < Config::SOIL_DISCARD_SAMPLE_COUNT; ++i) {
    analogRead(Config::SOIL_ADC_PIN);
    delay(25);
  }

  while (validCount < Config::SOIL_SAMPLE_COUNT) {
    const int raw = analogRead(Config::SOIL_ADC_PIN);
    if (raw > Config::SOIL_ADC_MIN_VALID && raw < Config::SOIL_ADC_MAX_VALID) {
      samples[validCount++] = raw;
      consecutiveErrors = 0;
      delay(25);
      continue;
    }

    ++consecutiveErrors;
    if (consecutiveErrors > Config::SOIL_MAX_READ_ERRORS) {
      Serial.printf("[Soil] read failed raw=%d errors=%u\n", raw, consecutiveErrors);
      return result;
    }
    delay(Config::SOIL_RETRY_DELAY_MS);
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

  axG = static_cast<float>(axRaw) / 16384.0f - Config::MPU_AX_OFFSET_G;
  ayG = static_cast<float>(ayRaw) / 16384.0f - Config::MPU_AY_OFFSET_G;
  azG = static_cast<float>(azRaw) / 16384.0f - Config::MPU_AZ_OFFSET_G;
  return true;
}

bool initMpu6050() {
  Wire.begin(Config::I2C_SDA_PIN, Config::I2C_SCL_PIN);
  Wire.setClock(400000);

  uint8_t whoAmI = 0;
  bool identified = false;
  for (uint8_t attempt = 0; attempt <= 3; ++attempt) {
    if (mpuReadReg(0x75, whoAmI) && (whoAmI == 0x68 || whoAmI == 0x70)) {
      identified = true;
      break;
    }
    delay(100);
  }
  if (!identified) {
    Serial.printf("[MPU] init failed WHO_AM_I=0x%02X\n", whoAmI);
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

    bool sampleRead = false;
    for (uint8_t attempt = 0; attempt <= 3; ++attempt) {
      if (readAccel(ax, ay, az)) {
        sampleRead = true;
        break;
      }
      delay(20);
    }
    if (!sampleRead) {
      Serial.println("[MPU] sample read failed after retries");
      return result;
    }

    axFiltered = alpha * axFiltered + (1.0f - alpha) * ax;
    ayFiltered = alpha * ayFiltered + (1.0f - alpha) * ay;
    azFiltered = alpha * azFiltered + (1.0f - alpha) * az;

    const float pitchRad = atan2f(-axFiltered, sqrtf(ayFiltered * ayFiltered + azFiltered * azFiltered));
    const float rollRad = atan2f(ayFiltered, sqrtf(axFiltered * axFiltered + azFiltered * azFiltered));
    const float pitchDeg = pitchRad * 180.0f / PI - Config::MPU_PITCH_ZERO_DEG;
    const float rollDeg = rollRad * 180.0f / PI - Config::MPU_ROLL_ZERO_DEG;
    const float betaDeg = sqrtf(pitchDeg * pitchDeg + rollDeg * rollDeg);

    const float axVib = ax - axFiltered;
    const float ayVib = ay - ayFiltered;
    const float azVib = az - azFiltered;
    const float vibration = sqrtf(axVib * axVib + ayVib * ayVib + azVib * azVib);
    vibrationSquaredSum += vibration * vibration;

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

  if (rtcHasPreviousBeta) {
    const uint32_t endMs = millis();
    float dtSeconds = static_cast<float>(rtcSleepSeconds) + Config::MPU_WINDOW_MS / 1000.0f;
    if (previousBetaSampleMs != 0 && endMs > previousBetaSampleMs) {
      dtSeconds = static_cast<float>(endMs - previousBetaSampleMs) / 1000.0f;
    }
    result.betaDotDegPerHour = (lastBeta - rtcPreviousBetaDeg) * 3600.0f / dtSeconds;
  } else {
    result.betaDotDegPerHour = 0.0f;
  }

  result.ok = true;
  return result;
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

bool sendPacketAndWaitAck(const String &packet,
                          uint32_t expectedPacketId,
                          Protocol::AckPacket &ack) {
  if (!loraReady) {
    return false;
  }

  Serial.printf("[LoRa] TX packet_id=%lu\n",
                static_cast<unsigned long>(expectedPacketId));
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
        parsedAck.packetId == expectedPacketId &&
        (parsedAck.status == "OK" || parsedAck.status == "DUPLICATE")) {
      ack = parsedAck;
      Serial.printf("[LoRa] ACK status=%s sleep=%.1f minutes alert=%s\n",
                    ack.status.c_str(),
                    ack.sleepSeconds / 60.0f,
                    ack.alertLevel.c_str());
      return true;
    }

    Serial.printf("[LoRa] ignored packet: %s\n", incoming.c_str());
  }

  return false;
}

bool sendTelemetryAndWaitAck(const Protocol::SensorPacket &data, Protocol::AckPacket &ack) {
  return sendPacketAndWaitAck(Protocol::buildDataPacket(data), data.packetId, ack);
}

void storePendingPacket(const String &packet) {
  if (packet.length() >= sizeof(rtcPendingPacket)) {
    Serial.println("[BUFFER] packet too large, cannot save pending telemetry");
    return;
  }
  if (rtcPendingPacketValid) {
    Serial.println("[BUFFER] full, newest unsent packet dropped");
    return;
  }
  packet.toCharArray(rtcPendingPacket, sizeof(rtcPendingPacket));
  rtcPendingPacketValid = true;
  Serial.println("[BUFFER] telemetry saved in RTC pending slot");
}

void flushPendingPacket() {
  if (!rtcPendingPacketValid || !loraReady) {
    return;
  }

  const String packet(rtcPendingPacket);
  Protocol::SensorPacket pendingData;
  if (!Protocol::parseDataPacket(packet, pendingData)) {
    Serial.println("[BUFFER] invalid pending packet discarded");
    rtcPendingPacketValid = false;
    rtcPendingPacket[0] = '\0';
    return;
  }

  Protocol::AckPacket ack;
  Serial.printf("[BUFFER] retry packet_id=%lu\n",
                static_cast<unsigned long>(pendingData.packetId));
  if (sendPacketAndWaitAck(packet, pendingData.packetId, ack)) {
    rtcPendingPacketValid = false;
    rtcPendingPacket[0] = '\0';
    Serial.println("[BUFFER] pending telemetry acknowledged");
  } else {
    Serial.println("[BUFFER] pending telemetry still waiting for ACK");
  }
}

void validateSnapshot(Protocol::SensorPacket &data) {
  const bool sensorValuesInRange =
      isfinite(data.soilPercent) && data.soilPercent >= Config::SOIL_PERCENT_MIN &&
      data.soilPercent <= Config::SOIL_PERCENT_MAX && isfinite(data.betaDeg) &&
      data.betaDeg >= Config::BETA_MIN_DEG && data.betaDeg <= Config::BETA_MAX_DEG &&
      isfinite(data.betaDotDegPerHour) &&
      fabsf(data.betaDotDegPerHour) <= Config::BETA_DOT_SANITY_MAX_DEG_PER_HOUR &&
      isfinite(data.vibrationRmsG) && data.vibrationRmsG >= Config::A_RMS_MIN_G &&
      data.vibrationRmsG <= Config::A_RMS_MAX_G;
  if (!sensorValuesInRange) {
    data.errorFlags |= Protocol::ERR_DATA_RANGE;
  }
}

uint32_t localSleepFallback(const Protocol::SensorPacket &data) {
  return Analysis::evaluate(data).nextSleepSeconds;
}

void rememberMpuBaseline(float betaDeg) {
  rtcPreviousBetaDeg = betaDeg;
  rtcHasPreviousBeta = true;
  previousBetaSampleMs = millis();
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
  flushPendingPacket();

  uint32_t nextSleep = Config::SLEEP_NORMAL_SEC;

  for (uint8_t sampleIndex = 0; sampleIndex < Config::NODE_MEASUREMENTS_PER_WAKE; ++sampleIndex) {
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
    } else {
      rememberMpuBaseline(data.betaDeg);
    }

    validateSnapshot(data);

    Protocol::AckPacket ack;
    bool ackOk = sendTelemetryAndWaitAck(data, ack);
    nextSleep = localSleepFallback(data);
    if (!ackOk) {
      data.errorFlags |= Protocol::ERR_LORA;
      storePendingPacket(Protocol::buildDataPacket(data));
      Serial.printf("[LoRa] no ACK; error_flag=%u (%s), using local sleep fallback\n",
                    data.errorFlags,
                    Analysis::errorFlagsToText(data.errorFlags));
    } else if (ack.sleepSeconds > 0) {
      nextSleep = ack.sleepSeconds;
    }

    Serial.printf("[DATA] sample=%u/%u packet_id=%lu soil_adc_filtered=%d h_soil=%.2f beta_deg=%.3f "
                  "beta_dot_deg_per_hour=%.4f a_rms_g=%.5f pitch_deg=%.3f "
                  "roll_deg=%.3f error_flag=%u error_text=%s\n",
                  sampleIndex + 1,
                  Config::NODE_MEASUREMENTS_PER_WAKE,
                  static_cast<unsigned long>(data.packetId),
                  data.soilAdc,
                  data.soilPercent,
                  data.betaDeg,
                  data.betaDotDegPerHour,
                  data.vibrationRmsG,
                  data.pitchDeg,
                  data.rollDeg,
                  data.errorFlags,
                  Analysis::errorFlagsToText(data.errorFlags));

    if (sampleIndex + 1 < Config::NODE_MEASUREMENTS_PER_WAKE) {
      delay(Config::NODE_INTER_MEASUREMENT_DELAY_MS);
    }
  }

  sleepOrDelay(nextSleep);
}

} // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n[%s NODE] boot packetId=%lu\n",
                Config::PROJECT_TAG,
                static_cast<unsigned long>(rtcPacketId));
  printMpuCalibrationProfile();

  powerSensors(true);
  configureAdc();
  mpuReady = initMpu6050();
  loraReady = initLora();
}

void loop() {
  runCycle();
}
