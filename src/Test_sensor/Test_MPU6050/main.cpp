#include "mpu6050_tilt.h"

// ================== GLOBAL VARIABLES ==================

// Raw acceleration
float Ax = 0.0;
float Ay = 0.0;
float Az = 0.0;

// Filtered acceleration
float Axf = 0.0;
float Ayf = 0.0;
float Azf = 0.0;

// Angles
float pitch = 0.0;
float roll = 0.0;
float beta = 0.0;
float beta_prev = 0.0;

// Angular changing speed
float beta_dot = 0.0;
float beta_dot_f = 0.0;
float beta_dot_prev = 0.0;

// Vibration index
float A_rms = 0.0;

// Last-value buffer
float betaBuffer = 0.0;
float betaDotBuffer = 0.0;
float armsBuffer = 0.0;

// Timing
unsigned long t_now = 0;
unsigned long t_prev = 0;

// Error count
int sensorErrorCount = 0;

// ================== SETUP TEST ==================

void mpuTestSetup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin();
  mpuInit();

  Serial.println("=================================");
  Serial.println("TEST CAM BIEN MPU6050 DO DO NGHIENG");
  Serial.println("ESP32 + MPU6050");
  Serial.println("=================================");

  if (!readMPU6050(Ax, Ay, Az)) {
    Serial.println("ERROR: Cannot read MPU6050 at startup.");
    Serial.println("Check wiring: VCC, GND, SDA, SCL.");
    while (true) delay(1000);
  }

  // Khoi tao filter ban dau
  Axf = Ax;
  Ayf = Ay;
  Azf = Az;

  calculatePitchRoll();
  calculateBeta();

  beta_prev = beta;
  beta_dot_prev = 0.0;

  t_prev = millis();

  Serial.println("System ready.");
}

// ================== LOOP TEST ==================

void mpuTestLoop() {
  t_now = millis();

  if (t_now - t_prev < SAMPLE_TIME_MS) {
    return;
  }

  Serial.println("-----------------------------");

  if (!readMPU6050(Ax, Ay, Az)) {
    sensorErrorCount++;

    Serial.print("MPU6050 read failed. Error count = ");
    Serial.println(sensorErrorCount);

    if (sensorErrorCount > MAX_SENSOR_ERROR) {
      Serial.println("HARDWARE ERROR!");
      Serial.println("SYSTEM STOPPED.");
      while (true) delay(1000);
    }

    return;
  }

  sensorErrorCount = 0;

  if (!isAccelValid(Ax, Ay, Az)) {
    Serial.println("Acceleration data invalid.");
    return;
  }

  updateLowPassFilter(Ax, Ay, Az);

  calculatePitchRoll();
  calculateBeta();
  calculateBetaDot();
  filterBetaDot();
  calculateArms();

  updateBuffer();
  printMPUData();

  beta_prev = beta;
  beta_dot_prev = beta_dot_f;
  t_prev = t_now;
}

// ================== MPU INIT ==================

void mpuInit() {
  // Wake up MPU6050
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission(true);

  delay(100);

  // Accelerometer range: +/- 2g
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C);
  Wire.write(0x00);
  Wire.endTransmission(true);
}

// ================== READ MPU6050 ==================

bool readMPU6050(float &ax, float &ay, float &az) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);

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

  // For +/-2g range: 16384 LSB/g
  ax = rawAx / 16384.0;
  ay = rawAy / 16384.0;
  az = rawAz / 16384.0;

  return true;
}

// ================== CHECK DATA ==================

bool isAccelValid(float ax, float ay, float az) {
  float a_total = sqrt(ax * ax + ay * ay + az * az);

  // Khi cam bien dung yen, tong gia toc thuong quanh 1g.
  // Cho phep rong de test: 0.3g -> 2.5g.
  if (a_total < 0.3 || a_total > 2.5) {
    return false;
  }

  return true;
}

// ================== LOW PASS FILTER ==================

void updateLowPassFilter(float ax, float ay, float az) {
  Axf = ALPHA_ACC * Axf + (1.0 - ALPHA_ACC) * ax;
  Ayf = ALPHA_ACC * Ayf + (1.0 - ALPHA_ACC) * ay;
  Azf = ALPHA_ACC * Azf + (1.0 - ALPHA_ACC) * az;
}

// ================== CALCULATE ANGLES ==================

void calculatePitchRoll() {
  float pitchRad = atan2(-Axf, sqrt(Ayf * Ayf + Azf * Azf));
  float rollRad  = atan2(Ayf, sqrt(Axf * Axf + Azf * Azf));

  pitch = pitchRad * 180.0 / PI;
  roll  = rollRad  * 180.0 / PI;
}

void calculateBeta() {
  beta = sqrt(pitch * pitch + roll * roll);
}

// ================== CALCULATE BETA DOT ==================

void calculateBetaDot() {
  float dt = (t_now - t_prev) / 1000.0;

  if (dt > 0) {
    beta_dot = (beta - beta_prev) / dt;
  } else {
    beta_dot = 0.0;
  }
}

void filterBetaDot() {
  beta_dot_f = ALPHA_BETA_DOT * beta_dot_prev +
               (1.0 - ALPHA_BETA_DOT) * beta_dot;
}

// ================== CALCULATE A RMS ==================

void calculateArms() {
  A_rms = sqrt((Axf * Axf + Ayf * Ayf + Azf * Azf) / 3.0);
}

// ================== BUFFER ==================

void updateBuffer() {
  betaBuffer = beta;
  betaDotBuffer = beta_dot_f;
  armsBuffer = A_rms;
}

// ================== PRINT DATA ==================

void printMPUData() {
  Serial.print("Ax: ");
  Serial.print(Ax, 3);
  Serial.print(" g | Ay: ");
  Serial.print(Ay, 3);
  Serial.print(" g | Az: ");
  Serial.print(Az, 3);
  Serial.println(" g");

  Serial.print("Pitch: ");
  Serial.print(pitch, 2);
  Serial.print(" deg | Roll: ");
  Serial.print(roll, 2);
  Serial.println(" deg");

  Serial.print("Beta: ");
  Serial.print(beta, 2);
  Serial.println(" deg");

  Serial.print("Beta_dot raw: ");
  Serial.print(beta_dot, 2);
  Serial.println(" deg/s");

  Serial.print("Beta_dot filtered: ");
  Serial.print(betaDotBuffer, 2);
  Serial.println(" deg/s");

  Serial.print("A_rms: ");
  Serial.print(armsBuffer, 3);
  Serial.println(" g");
}

// ================== ARDUINO MAIN ==================

void setup() {
  mpuTestSetup();
}

void loop() {
  mpuTestLoop();
}