#ifndef MPU6050_TILT_H
#define MPU6050_TILT_H

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

// ================== CONFIG ==================
#define MPU_ADDR 0x69

const unsigned long SAMPLE_TIME_MS = 200;    // chu kỳ lấy mẫu 200ms
const unsigned long PRINT_TIME_MS  = 2000;   // chu kỳ in Serial 2000ms

const float ALPHA_ACC = 0.85;                // hệ số lọc low-pass
const int MAX_CONNECT_RETRY = 3;

// ================== FUNCTION DECLARE ==================
void mpuTestSetup();
void mpuTestLoop();

bool mpuInit();
bool checkMPUConnection();
bool readMPU6050(float &ax, float &ay, float &az);

void initFilter();
void updateLowPassFilter();
void calculatePitchRoll();
void calculateBeta();
void calculateBetaDot();
void calculateArms();

void updateBuffer();
void printMPUData();

#endif