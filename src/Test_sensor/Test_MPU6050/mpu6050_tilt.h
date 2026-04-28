#ifndef MPU6050_TILT_H
#define MPU6050_TILT_H

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#define MPU6050_ADDR 0x68

class MPU6050_Tilt {
public:
  bool begin() {
    // Không Wire.begin() ở đây

    // Test I2C trước
    Wire.beginTransmission(MPU6050_ADDR);
    if (Wire.endTransmission() != 0) {
      Serial.println("I2C device 0x68 not responding.");
      return false;
    }

    // Wake up MPU6050
    writeRegister(0x6B, 0x00);
    delay(300);

    uint8_t whoAmI = readRegister(0x75);

    Serial.print("WHO_AM_I = 0x");
    Serial.println(whoAmI, HEX);

    // Nhiều MPU6050 clone vẫn chạy dù WHO_AM_I không đúng tuyệt đối
    if (whoAmI == 0x00 || whoAmI == 0xFF) {
      return false;
    }

    // Accelerometer ±2g
    writeRegister(0x1C, 0x00);
    delay(50);

    return true;
  }

  bool readAccelRaw(int16_t &ax, int16_t &ay, int16_t &az) {
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(0x3B);

    if (Wire.endTransmission(false) != 0) {
      return false;
    }

    uint8_t n = Wire.requestFrom(MPU6050_ADDR, (uint8_t)6, (uint8_t)true);
    if (n != 6) {
      return false;
    }

    ax = (int16_t)((Wire.read() << 8) | Wire.read());
    ay = (int16_t)((Wire.read() << 8) | Wire.read());
    az = (int16_t)((Wire.read() << 8) | Wire.read());

    return true;
  }

private:
  bool writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission(true) == 0;
  }

  uint8_t readRegister(uint8_t reg) {
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(reg);

    if (Wire.endTransmission(false) != 0) {
      return 0;
    }

    if (Wire.requestFrom(MPU6050_ADDR, (uint8_t)1, (uint8_t)true) != 1) {
      return 0;
    }

    return Wire.read();
  }
};

#endif