#include "mpu6050_tilt.h"

// ================== GLOBAL VARIABLES ==================
float Ax = 0, Ay = 0, Az = 0;
float Ax_raw = 0, Ay_raw = 0, Az_raw = 0;

float Axf = 0, Ayf = 0, Azf = 0;
float Axf_prev = 0, Ayf_prev = 0, Azf_prev = 0;

float pitch = 0, roll = 0;
float pitch_deg = 0, roll_deg = 0;

float beta = 0;
float beta_prev = 0;
float beta_dot = 0;

float A_rms = 0;

float betaBuffer = 0;
float betaDotBuffer = 0;
float armsBuffer = 0;

unsigned long t = 0;
unsigned long t_prev = 0;
unsigned long t_print_prev = 0;

float dt = 0;

// ================== SETUP / LOOP ==================
void setup() {
  mpuTestSetup();
}

void loop() {
  mpuTestLoop();
}

// ================== MPU TEST SETUP ==================
void mpuTestSetup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(21, 22);
  Wire.setClock(400000);

  Serial.println("=================================");
  Serial.println("TEST MPU6050 TILT SENSOR");
  Serial.println("ESP32 + MPU6050");
  Serial.println("Accelerometer only");
  Serial.println("=================================");

  bool connected = false;

  for (int i = 1; i <= MAX_CONNECT_RETRY; i++) {
    Serial.print("Checking MPU6050... Try ");
    Serial.println(i);

    if (mpuInit() && checkMPUConnection()) {
      connected = true;
      break;
    }

    delay(500);
  }

  if (!connected) {
    Serial.println("ERROR: Cannot connect to MPU6050.");
    Serial.println("Check VCC, GND, SDA, SCL.");
    Serial.println("SYSTEM STOPPED.");

    while (true) {
      delay(1000);
    }
  }

  Serial.println("MPU6050 connected.");

  if (!readMPU6050(Ax_raw, Ay_raw, Az_raw)) {
    Serial.println("ERROR: Cannot read MPU6050 first data.");
    Serial.println("SYSTEM STOPPED.");

    while (true) {
      delay(1000);
    }
  }

  initFilter();

  beta_prev = 0;
  beta_dot = 0;
  A_rms = 0;

  t_prev = millis();
  t_print_prev = millis();

  Serial.println("System ready.");
}

// ================== MPU TEST LOOP ==================
void mpuTestLoop() {
  t = millis();

  // Block 7: kiểm tra chu kỳ lấy mẫu
  if (t - t_prev < SAMPLE_TIME_MS) {
    return;
  }

  // Block 8 + 9: đọc MPU6050 và kiểm tra lỗi
  if (!readMPU6050(Ax_raw, Ay_raw, Az_raw)) {
    Serial.println("Read MPU6050 error. Skip this loop.");
    return;
  }

  // Block 10
  updateLowPassFilter();

  // Block 11 + 12
  calculatePitchRoll();

  // Block 13
  calculateBeta();

  // Block 14 + 15
  calculateBetaDot();

  // Block 16
  calculateArms();

  // Block 17
  updateBuffer();

  // Block 18: in Serial mỗi 2000ms
  if (t - t_print_prev >= PRINT_TIME_MS) {
    printMPUData();
    t_print_prev = t;
  }

  // Block 19: cập nhật biến
  beta_prev = beta;
  t_prev = t;

  Axf_prev = Axf;
  Ayf_prev = Ayf;
  Azf_prev = Azf;
}

// ================== INIT MPU6050 ==================
bool mpuInit() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);       // Power Management 1
  Wire.write(0x00);       // Wake up MPU6050
  if (Wire.endTransmission(true) != 0) {
    return false;
  }

  delay(100);

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C);       // Accelerometer config
  Wire.write(0x00);       // ±2g
  if (Wire.endTransmission(true) != 0) {
    return false;
  }

  return true;
}

// ================== CHECK CONNECTION ==================
bool checkMPUConnection() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x75);       // WHO_AM_I register

  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  Wire.requestFrom(MPU_ADDR, 1, true);

  if (Wire.available() < 1) {
    return false;
  }

  byte whoAmI = Wire.read();

  return (whoAmI == 0x68);
}

// ================== READ ACCEL ==================
bool readMPU6050(float &ax, float &ay, float &az) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);       // ACCEL_XOUT_H

  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  Wire.requestFrom(MPU_ADDR, 6, true);

  if (Wire.available() < 6) {
    return false;
  }

  int16_t rawAx = (Wire.read() << 8) | Wire.read();
  int16_t rawAy = (Wire.read() << 8) | Wire.read();
  int16_t rawAz = (Wire.read() << 8) | Wire.read();

  ax = rawAx / 16384.0;
  ay = rawAy / 16384.0;
  az = rawAz / 16384.0;

  return true;
}

// ================== INIT FILTER ==================
void initFilter() {
  Axf = Ax_raw;
  Ayf = Ay_raw;
  Azf = Az_raw;

  Axf_prev = Axf;
  Ayf_prev = Ayf;
  Azf_prev = Azf;
}

// ================== LOW PASS FILTER ==================
void updateLowPassFilter() {
  Axf = ALPHA_ACC * Axf_prev + (1.0 - ALPHA_ACC) * Ax_raw;
  Ayf = ALPHA_ACC * Ayf_prev + (1.0 - ALPHA_ACC) * Ay_raw;
  Azf = ALPHA_ACC * Azf_prev + (1.0 - ALPHA_ACC) * Az_raw;
}

// ================== PITCH / ROLL ==================
void calculatePitchRoll() {
  pitch = atan2(-Axf, sqrt(Ayf * Ayf + Azf * Azf));
  roll  = atan2(Ayf, sqrt(Axf * Axf + Azf * Azf));

  pitch_deg = pitch * 180.0 / PI;
  roll_deg  = roll  * 180.0 / PI;
}

// ================== BETA ==================
void calculateBeta() {
  beta = sqrt(pitch_deg * pitch_deg + roll_deg * roll_deg);
}

// ================== BETA DOT ==================
void calculateBetaDot() {
  dt = (t - t_prev) / 1000.0;

  if (dt > 0) {
    beta_dot = (beta - beta_prev) / dt;
  } else {
    beta_dot = 0;
  }
}

// ================== A RMS ==================
void calculateArms() {
  A_rms = sqrt(Axf * Axf + Ayf * Ayf + Azf * Azf);
}

// ================== BUFFER ==================
void updateBuffer() {
  betaBuffer = beta;
  betaDotBuffer = beta_dot;
  armsBuffer = A_rms;
}

// ================== PRINT DATA ==================
void printMPUData() {
  Serial.println("=================================");

  Serial.print("Ax_raw: ");
  Serial.print(Ax_raw, 3);
  Serial.print(" g | Ay_raw: ");
  Serial.print(Ay_raw, 3);
  Serial.print(" g | Az_raw: ");
  Serial.print(Az_raw, 3);
  Serial.println(" g");

  Serial.print("Axf: ");
  Serial.print(Axf, 3);
  Serial.print(" g | Ayf: ");
  Serial.print(Ayf, 3);
  Serial.print(" g | Azf: ");
  Serial.print(Azf, 3);
  Serial.println(" g");

  Serial.print("Pitch: ");
  Serial.print(pitch_deg, 2);
  Serial.print(" deg | Roll: ");
  Serial.print(roll_deg, 2);
  Serial.println(" deg");

  Serial.print("Beta: ");
  Serial.print(betaBuffer, 2);
  Serial.println(" deg");

  Serial.print("Beta_dot: ");
  Serial.print(betaDotBuffer, 2);
  Serial.println(" deg/s");

  Serial.print("A_rms: ");
  Serial.print(armsBuffer, 3);
  Serial.println(" g");

  Serial.println("=================================");
}