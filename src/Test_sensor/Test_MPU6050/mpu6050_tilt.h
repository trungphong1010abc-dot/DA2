#ifndef MPU6050_TILT_H
#define MPU6050_TILT_H

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

// ================== CONFIG ==================
#define MPU_ADDR 0x68

const unsigned long SAMPLE_TIME_MS = 200;
const float ALPHA_ACC = 0.85;
const float ALPHA_BETA_DOT = 0.85;

const int MAX_SENSOR_ERROR = 3;

// ================== DATA STRUCT ==================
struct MPU6050TiltData {
  float Ax;
  float Ay;
  float Az;

  float Axf;
  float Ayf;
  float Azf;

  float pitch;
  float roll;
  float beta;
  float beta_dot;
  float beta_dot_f;
  float A_rms;
};

// ================== FUNCTION DECLARE ==================
void mpuTestSetup();
void mpuTestLoop();

void mpuInit();
bool readMPU6050(float &ax, float &ay, float &az);
bool isAccelValid(float ax, float ay, float az);

void updateLowPassFilter(float ax, float ay, float az);
void calculatePitchRoll();
void calculateBeta();
void calculateBetaDot();
void filterBetaDot();
void calculateArms();

void updateBuffer();
void printMPUData();

#endif