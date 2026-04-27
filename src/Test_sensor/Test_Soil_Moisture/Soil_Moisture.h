#ifndef SOIL_MOISTURE_H
#define SOIL_MOISTURE_H

#include <Arduino.h>

// Chân cảm biến
#define SOIL_PIN 34

// Cấu hình đọc mẫu
#define SAMPLE_COUNT 11
#define DISCARD_COUNT 3
#define READ_DELAY_MS 10
#define LOOP_DELAY_MS 2000
#define SHORT_DELAY_MS 500

// Giá trị hiệu chuẩn
extern int ADC_DRY;
extern int ADC_WET;

// Hàm dùng chung
void soilInit();
int readADCOnce();
bool isADCValid(int adc);
int medianFilter(int arr[], int size);
int readSoilMedian();
float adcToMoisturePercent(int adcFiltered);

// Chế độ test
void soilTestSetup();
void soilTestLoop();

// Chế độ calib
void soilCalibSetup();
void soilCalibLoop();

#endif