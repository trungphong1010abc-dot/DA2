#include "mpu6050_tilt.h"

// ================== GLOBAL VARIABLES ==================
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

// ================== SETUP TEST ==================
void mpuTestSetup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);   // dùng 100kHz cho ổn định khi test

  Serial.println("=================================");
  Serial.println("TEST MPU6050 TILT SENSOR");
  Serial.println("ESP32 + MPU6050");
  Serial.println("Accelerometer only");
  Serial.println("=================================");

  bool connected = false;

  for (int i = 1; i <= MAX_CONNECT_RETRY; i++) {
    Serial.print("Checking MPU6050... Try ");
    Serial.println(i);

    if (checkMPUConnection()) {
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

  Serial.println("MPU6050 I2C detected at 0x68.");

  if (!mpuInit()) {
    Serial.println("ERROR: MPU6050 init failed.");
    Serial.println("SYSTEM STOPPED.");

    while (true) {
      delay(1000);
    }
  }

  Serial.println("MPU6050 initialized.");

  if (!readMPU6050(Ax_raw, Ay_raw, Az_raw)) {
    Serial.println("ERROR: Cannot read first MPU6050 data.");
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

// ================== LOOP TEST ==================
void mpuTestLoop() {
  t = millis();

  if (t - t_prev < SAMPLE_TIME_MS) {
    return;
  }

  if (!readMPU6050(Ax_raw, Ay_raw, Az_raw)) {
    Serial.println("Read MPU6050 error. Skip this loop.");
    return;
  }

  updateLowPassFilter();
  calculatePitchRoll();
  calculateBeta();
  calculateBetaDot();
  calculateArms();
  updateBuffer();

  if (t - t_print_prev >= PRINT_TIME_MS) {
    printMPUData();
    t_print_prev = t;
  }

  beta_prev = beta;
  t_prev = t;

  Axf_prev = Axf;
  Ayf_prev = Ayf;
  Azf_prev = Azf;
}

// ================== CHECK CONNECTION ==================
bool checkMPUConnection() {
  Wire.beginTransmission(MPU_ADDR);
  byte error = Wire.endTransmission();

  return (error == 0);
}

// ================== INIT MPU6050 ==================
bool mpuInit() {
  // Wake up MPU6050
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  if (Wire.endTransmission(true) != 0) {
    return false;
  }

  delay(100);

  // Accelerometer config: ±2g
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C);
  Wire.write(0x00);
  if (Wire.endTransmission(true) != 0) {
    return false;
  }

  delay(50);

  return true;
}

// ================== READ ACCEL ==================
bool readMPU6050(float &ax, float &ay, float &az) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);   // ACCEL_XOUT_H

  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  uint8_t bytesRead = Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)6, (uint8_t)true);

  if (bytesRead < 6) {
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

/*
Ax, Ay, Az (g):
- Gia tốc theo 3 trục của MPU6050 (đã chuẩn hóa về đơn vị g)
- Khi đứng yên: tổng vector ≈ 1g (trọng lực)
- Dùng để suy ra góc nghiêng

Axf, Ayf, Azf:
- Giá trị sau low-pass filter
- Giảm nhiễu, ổn định tín hiệu trước khi tính toán

pitch (deg):
- Góc nghiêng theo trục X (cúi/ngửa)
- Tính từ thành phần gia tốc và trọng lực

roll (deg):
- Góc nghiêng theo trục Y (nghiêng trái/phải)

β = sqrt(pitch² + roll²):
- Góc nghiêng tổng (độ dốc của đất)
- Đại lượng chính để đánh giá trạng thái nghiêng

β_dot (deg/s):
- Tốc độ thay đổi góc nghiêng
- β_dot ≈ 0 → ổn định
- β_dot lớn → đang có chuyển động (trượt, lún)

A_rms (g):
- Độ lớn tổng của vector gia tốc
- ≈ 1g → đứng yên
- >1g → có rung/lắc (dao động cơ học)

=> Ý nghĩa hệ thống:
- β      → mức độ nghiêng đất
- β_dot  → tốc độ biến dạng (nguy cơ trượt)
- A_rms  → mức độ rung động (bất ổn)
*/