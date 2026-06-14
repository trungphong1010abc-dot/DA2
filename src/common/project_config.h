#pragma once

// Project: He thong nhung IoT do do gian cua dat
// Board: ESP32 DevKit V1 + RA-02 LoRa + MPU6050 + capacitive soil moisture sensor.

#include <Arduino.h>

namespace Config {

// ---------- Identity ----------
static constexpr uint8_t GATEWAY_ID = 1;
static constexpr uint8_t NODE_ID = 1;
static constexpr const char *PROJECT_TAG = "DA2_SOIL_STRAIN";

// ---------- WiFi / ThingsBoard ----------
static constexpr const char *WIFI_SSID = "Khoa";
static constexpr const char *WIFI_PASSWORD = "12112004";

static constexpr const char *TB_HOST = "mqtt.eu.thingsboard.cloud";
static constexpr uint16_t TB_PORT = 1883;
static constexpr const char *TB_TOKEN = "NAZJoUt0FVRMKKIMkp4R";
static constexpr const char *TB_TELEMETRY_TOPIC = "v1/devices/me/telemetry";
static constexpr const char *PARAMETER_PROFILE = "BASALT_RED_SOIL_V1_PROVISIONAL";

// ---------- LoRa RA-02 / SX1278 ----------
// RA-02 is commonly the 433 MHz SX1278 module. Change if your module is 868/915 MHz.
static constexpr long LORA_FREQUENCY_HZ = 433E6;
static constexpr uint8_t LORA_SYNC_WORD = 0xDA;
static constexpr int LORA_TX_POWER_DBM = 17;
static constexpr long LORA_SIGNAL_BANDWIDTH = 125E3;
static constexpr uint8_t LORA_SPREADING_FACTOR = 9;
static constexpr uint8_t LORA_CODING_RATE_DENOM = 5;

static constexpr int LORA_SCK_PIN = 18;
static constexpr int LORA_MISO_PIN = 19;
static constexpr int LORA_MOSI_PIN = 23;
static constexpr int LORA_CS_PIN = 5;
static constexpr int LORA_RST_PIN = 14;
static constexpr int LORA_DIO0_PIN = 26;

// ---------- Node sensors ----------
static constexpr int I2C_SDA_PIN = 21;
static constexpr int I2C_SCL_PIN = 22;
static constexpr uint8_t MPU6050_ADDR = 0x68;

static constexpr int SOIL_ADC_PIN = 34;     // ADC1_CH6, input only.
static constexpr int BATTERY_ADC_PIN = 35;  // ADC1_CH7, input only.
static constexpr int SENSOR_POWER_PIN = -1; // Set to a GPIO if sensor VCC is switched by MOSFET.

// Calibrate these two values with your real soil sensor.
// For many capacitive probes: dry ADC is high, wet ADC is low.
static constexpr int SOIL_ADC_DRY = 3400;
static constexpr int SOIL_ADC_WET = 1200;
static constexpr int SOIL_ADC_MIN_VALID = 100;
static constexpr int SOIL_ADC_MAX_VALID = 4090;
static constexpr uint8_t SOIL_SAMPLE_COUNT = 11;
static constexpr uint8_t SOIL_DISCARD_SAMPLE_COUNT = 3;
static constexpr uint8_t SOIL_MAX_READ_ERRORS = 3;
static constexpr uint32_t SOIL_RETRY_DELAY_MS = 400;

// Battery divider: VBAT+ -- 220k -- GPIO35 -- 100k -- GND.
static constexpr float BAT_DIVIDER_R_TOP_OHM = 220000.0f;
static constexpr float BAT_DIVIDER_R_BOTTOM_OHM = 100000.0f;
// Adjust only after comparing GPIO35 millivolts and battery voltage with a multimeter.
static constexpr float BATTERY_VOLTAGE_CALIBRATION = 1.000f;
static constexpr float BATTERY_LOW_V = 3.50f;
static constexpr float BATTERY_CRITICAL_V = 3.30f;
static constexpr float BATTERY_SANITY_MIN_V = 3.00f;
static constexpr float BATTERY_SANITY_MAX_V = 4.25f;

// ---------- Sampling / retry ----------
static constexpr uint32_t SENSOR_WARMUP_MS = 800;
static constexpr uint32_t MPU_SAMPLE_INTERVAL_MS = 200;
static constexpr uint32_t MPU_WINDOW_MS = 2000;
static constexpr uint32_t ACK_TIMEOUT_MS = 2500;
static constexpr uint8_t LORA_MAX_RETRY = 3;

static constexpr uint32_t SLEEP_NORMAL_SEC = 30UL * 60UL;
static constexpr uint32_t SLEEP_WARNING_SEC = 20UL * 60UL;
static constexpr uint32_t SLEEP_DANGER_SEC = 5UL * 60UL;
static constexpr uint32_t SLEEP_SENSOR_ERROR_SEC = 10UL * 60UL;
static constexpr bool NODE_ENABLE_DEEP_SLEEP = true;

// ---------- Geotechnical model ----------
// Units: gamma*z -> kPa when gamma is kN/m3 and z is m.
static constexpr float SOIL_GAMMA_KN_M3 = 18.0f;
static constexpr float SLIP_LAYER_DEPTH_M = 1.0f;
static constexpr float SOIL_COHESION_KPA = 5.0f;
static constexpr float SOIL_FRICTION_ANGLE_DEG = 28.0f;
static constexpr float PORE_PRESSURE_MAX_KPA = 10.0f;
static constexpr float MOISTURE_DANGER_START_PERCENT = 65.0f;
static constexpr float MOISTURE_SATURATION_PERCENT = 95.0f;

static constexpr float FS_DANGER = 1.0f;
static constexpr float FS_WARNING = 1.3f;
static constexpr float STRAIN_WARNING = 0.77f;
static constexpr float STRAIN_DANGER = 1.0f;

static constexpr float BETA_DOT_CRIT_DEG_PER_HOUR = 2.0f;
static constexpr float A_RMS_CRIT_G = 0.05f;
static constexpr float DI_WARNING = 0.5f;
static constexpr float DI_DANGER = 1.0f;
static constexpr float DI_WEIGHT_BETA_DOT = 0.70f;
static constexpr float DI_WEIGHT_VIBRATION = 0.30f;

// ---------- Data sanity limits ----------
static constexpr float SOIL_PERCENT_MIN = 0.0f;
static constexpr float SOIL_PERCENT_MAX = 100.0f;
static constexpr float BETA_MIN_DEG = 0.0f;
static constexpr float BETA_MAX_DEG = 60.0f;
static constexpr float BETA_DOT_SANITY_MAX_DEG_PER_HOUR = 360.0f;
static constexpr float A_RMS_MIN_G = 0.0f;
static constexpr float A_RMS_MAX_G = 1.0f;

// OTA is not implemented in the current superloop firmware.
static constexpr bool OTA_SUPPORTED = false;

// Gateway RAM offline queue. Persistence across reset remains a future design item.
static constexpr size_t GATEWAY_OFFLINE_QUEUE_CAPACITY = 8;
static constexpr size_t GATEWAY_TELEMETRY_MAX_BYTES = 1536;

static_assert(SOIL_ADC_DRY != SOIL_ADC_WET, "Soil calibration denominator must not be zero");
static_assert(MOISTURE_SATURATION_PERCENT > MOISTURE_DANGER_START_PERCENT,
              "H_sat must be greater than H_c");
static_assert(BETA_DOT_CRIT_DEG_PER_HOUR > 0.0f && A_RMS_CRIT_G > 0.0f,
              "DI critical values must be positive");

} // namespace Config
