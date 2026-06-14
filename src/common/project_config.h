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
static constexpr const char *WIFI_SSID = "Tuan";
static constexpr const char *WIFI_PASSWORD = "88888888";

static constexpr const char *TB_HOST = "mqtt.eu.thingsboard.cloud";
static constexpr uint16_t TB_PORT = 1883;
static constexpr const char *TB_TOKEN = "NAZJoUt0FVRMKKIMkp4R";
static constexpr const char *TB_TELEMETRY_TOPIC = "v1/devices/me/telemetry";

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
static constexpr int SOIL_ADC_DRY = 3000;
static constexpr int SOIL_ADC_WET = 1200;
static constexpr int SOIL_ADC_MIN_VALID = 100;
static constexpr int SOIL_ADC_MAX_VALID = 4090;
static constexpr uint8_t SOIL_SAMPLE_COUNT = 11;

// Battery divider: VBAT+ -- 220k -- ADC -- 100k -- GND.
static constexpr float ADC_REFERENCE_V = 3.30f;
static constexpr float BAT_DIVIDER_R_TOP_OHM = 220000.0f;
static constexpr float BAT_DIVIDER_R_BOTTOM_OHM = 100000.0f;
static constexpr float BATTERY_LOW_V = 3.50f;
static constexpr float BATTERY_CRITICAL_V = 3.30f;

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
static constexpr float PORE_PRESSURE_MAX_KPA = 12.0f;
static constexpr float MOISTURE_DANGER_START_PERCENT = 65.0f;
static constexpr float MOISTURE_SATURATION_PERCENT = 95.0f;

static constexpr float FS_DANGER = 1.0f;
static constexpr float FS_WARNING = 1.3f;
static constexpr float STRAIN_WARNING = 0.77f;
static constexpr float STRAIN_DANGER = 1.0f;

static constexpr float BETA_DOT_CRIT_DEG_PER_HOUR = 3.0f;
static constexpr float A_RMS_CRIT_G = 0.08f;
static constexpr float DI_WARNING = 0.5f;
static constexpr float DI_DANGER = 1.0f;
static constexpr float DI_WEIGHT_BETA_DOT = 0.55f;
static constexpr float DI_WEIGHT_VIBRATION = 0.45f;

} // namespace Config
