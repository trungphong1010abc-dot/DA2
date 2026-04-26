#include <Arduino.h>

// ===== CONFIG =====
#define SOIL_PIN 34              // ADC1: GPIO32–39. GPIO34 là input-only, hợp để đọc analog
#define SAMPLE_COUNT 11          // N = 10/11, nên dùng số lẻ để lấy median
#define DISCARD_COUNT 3          // Bỏ 3 mẫu đầu
#define READ_DELAY_MS 10
#define LOOP_DELAY_MS 2000
#define SHORT_DELAY_MS 500

// Hiệu chuẩn thực nghiệm: cần thay bằng giá trị đo thật
int ADC_DRY = 3200;              // đất khô / ngoài không khí
int ADC_WET = 1400;              // đất ướt / nước

// Ngưỡng kiểm tra ADC hợp lệ
const int ADC_MIN = 100;
const int ADC_MAX = 4000;
const int MAX_ADC_ERROR = 3;

int adcErrorCount = 0;
float soilBuffer = 0.0;

// ===== FUNCTIONS =====
int readADCOnce() {
  return analogRead(SOIL_PIN);
}

bool isADCValid(int adc) {
  return adc > ADC_MIN && adc < ADC_MAX;
}

int medianFilter(int arr[], int size) {
  for (int i = 0; i < size - 1; i++) {
    for (int j = i + 1; j < size; j++) {
      if (arr[j] < arr[i]) {
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
      }
    }
  }
  return arr[size / 2];
}

int readSoilMedian() {
  // Bỏ vài mẫu đầu để ADC/cảm biến ổn định
  for (int i = 0; i < DISCARD_COUNT; i++) {
    analogRead(SOIL_PIN);
    delay(READ_DELAY_MS);
  }

  int samples[SAMPLE_COUNT];

  for (int i = 0; i < SAMPLE_COUNT; i++) {
    samples[i] = analogRead(SOIL_PIN);
    delay(READ_DELAY_MS);
  }

  return medianFilter(samples, SAMPLE_COUNT);
}

float adcToMoisturePercent(int adcFiltered) {
  float H = (float)(ADC_DRY - adcFiltered) * 100.0 / (ADC_DRY - ADC_WET);

  // Giới hạn H về 0–100%
  if (H < 0) H = 0;
  if (H > 100) H = 100;

  return H;
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(1000);

  analogReadResolution(12);
  analogSetPinAttenuation(SOIL_PIN, ADC_11db);

  Serial.println("=== TEST CAM BIEN DO AM DAT DIEN DUNG ===");
  Serial.println("ESP32 ready.");
  delay(1000);
}

// ===== LOOP =====
void loop() {
  int adcCheck = readADCOnce();

  Serial.println("--------------------------");
  Serial.print("ADC check: ");
  Serial.println(adcCheck);

  if (!isADCValid(adcCheck)) {
    adcErrorCount++;

    Serial.print("ADC invalid. Error count = ");
    Serial.println(adcErrorCount);

    if (adcErrorCount > MAX_ADC_ERROR) {
      Serial.println("HARDWARE ERROR: Soil sensor or wiring problem.");
      Serial.println("SYSTEM STOPPED.");

      while (true) {
        delay(1000);
      }
    }

    delay(SHORT_DELAY_MS);
    return;
  }

  adcErrorCount = 0;

  int adcFiltered = readSoilMedian();
  float H = adcToMoisturePercent(adcFiltered);

  soilBuffer = H;

  Serial.print("ADC_filtered: ");
  Serial.println(adcFiltered);

  Serial.print("Soil moisture H: ");
  Serial.print(H, 1);
  Serial.println(" %");

  Serial.print("Buffer H: ");
  Serial.print(soilBuffer, 1);
  Serial.println(" %");

  delay(LOOP_DELAY_MS);
}