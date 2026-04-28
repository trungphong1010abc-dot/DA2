#include <Arduino.h>
#include <Wire.h>
#include "MPU6050_Tilt.h"

MPU6050_Tilt mpu;

int16_t Ax_raw = 0, Ay_raw = 0, Az_raw = 0;

float Axf = 0, Ayf = 0, Azf = 0;
float Axf_prev = 0, Ayf_prev = 0, Azf_prev = 0;

float pitch = 0, roll = 0;
float pitch_deg = 0, roll_deg = 0;

float beta = 0;
float beta_prev = 0;
float beta_dot = 0;

float A_rms_raw = 0;
float A_rms_g = 0;

unsigned long t = 0;
unsigned long t_prev = 0;
unsigned long t_print_prev = 0;

float dt = 0;

const float alpha = 0.85;

const unsigned long SAMPLE_TIME = 200;   // ms
const unsigned long PRINT_TIME  = 2000;  // ms

const float ACC_SCALE = 16384.0; // MPU6050 ±2g: 1g ≈ 16384

bool connectMPU6050() {
  for (int retry = 1; retry <= 3; retry++) {
    if (mpu.begin()) {
      Serial.println("MPU6050 connected successfully.");
      return true;
    }

    Serial.print("MPU6050 connection failed. Retry ");
    Serial.println(retry);
    delay(500);
  }

  return false;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // ESP32 I2C: SDA = GPIO21, SCL = GPIO22
  Wire.begin(21, 22);
  Wire.setClock(100000);

  Serial.println("Start MPU6050 Tilt Test");

  if (!connectMPU6050()) {
    Serial.println("Hardware error: MPU6050 not found.");
    while (true) {
      delay(1000);
    }
  }

  // Đọc lần đầu để khởi tạo bộ lọc
  if (!mpu.readAccelRaw(Ax_raw, Ay_raw, Az_raw)) {
    Serial.println("First read MPU6050 failed.");
    while (true) {
      delay(1000);
    }
  }

  Axf = Ax_raw;
  Ayf = Ay_raw;
  Azf = Az_raw;

  Axf_prev = Axf;
  Ayf_prev = Ayf;
  Azf_prev = Azf;

  t_prev = millis();
  t_print_prev = millis();

  Serial.println("MPU6050 first read OK.");
}

void loop() {
  // 6. Lấy thời gian hiện tại
  t = millis();

  // 7. Kiểm tra chu kỳ lấy mẫu 200ms
  if (t - t_prev < SAMPLE_TIME) {
    return;
  }

  // 14. Tính dt trước khi cập nhật t_prev
  dt = (t - t_prev) / 1000.0;
  t_prev = t;

  // 8. Đọc MPU6050
  if (!mpu.readAccelRaw(Ax_raw, Ay_raw, Az_raw)) {
    Serial.println("Read MPU6050 error. Skip this loop.");
    return;
  }

  // 10. Lọc low-pass
  Axf = alpha * Axf_prev + (1.0 - alpha) * Ax_raw;
  Ayf = alpha * Ayf_prev + (1.0 - alpha) * Ay_raw;
  Azf = alpha * Azf_prev + (1.0 - alpha) * Az_raw;

  // 11. Tính pitch, roll theo radian
  pitch = atan2(-Axf, sqrt(Ayf * Ayf + Azf * Azf));
  roll  = atan2(Ayf, sqrt(Axf * Axf + Azf * Azf));

  // 12. Đổi sang độ
  pitch_deg = pitch * 180.0 / PI;
  roll_deg  = roll  * 180.0 / PI;

  // 13. Tính độ nghiêng tổng beta
  beta = sqrt(pitch_deg * pitch_deg + roll_deg * roll_deg);

  // 15. Tính tốc độ thay đổi góc beta
  if (dt > 0) {
    beta_dot = (beta - beta_prev) / dt;
  } else {
    beta_dot = 0;
  }

  // 16. Tính độ lớn vector gia tốc
  A_rms_raw = sqrt(Axf * Axf + Ayf * Ayf + Azf * Azf);
  A_rms_g = A_rms_raw / ACC_SCALE;

  // 18. In Serial mỗi 2 giây
  if (t - t_print_prev >= PRINT_TIME) {
    Serial.println("===== MPU6050 DATA =====");

    Serial.print("Ax_raw Ay_raw Az_raw: ");
    Serial.print(Ax_raw);
    Serial.print(" ");
    Serial.print(Ay_raw);
    Serial.print(" ");
    Serial.println(Az_raw);

    Serial.print("Axf Ayf Azf: ");
    Serial.print(Axf);
    Serial.print(" ");
    Serial.print(Ayf);
    Serial.print(" ");
    Serial.println(Azf);

    Serial.print("Pitch: ");
    Serial.print(pitch_deg);
    Serial.println(" deg");

    Serial.print("Roll: ");
    Serial.print(roll_deg);
    Serial.println(" deg");

    Serial.print("Beta: ");
    Serial.print(beta);
    Serial.println(" deg");

  Serial.print("Beta_dot: ");
  Serial.print(fabs(beta_dot)); // quan tâm độ lớn tuyệt đối
  Serial.println(" deg/s");

    Serial.print("A_rms_g: ");
    Serial.print(A_rms_g);
    Serial.println(" g");

    Serial.println("========================");

    t_print_prev = t;
  }

  // 19. Cập nhật biến cho vòng sau
  beta_prev = beta;

  Axf_prev = Axf;
  Ayf_prev = Ayf;
  Azf_prev = Azf;
}

/*
Giải thích output MPU6050:

- Ax_raw, Ay_raw, Az_raw:
  Gia tốc thô từ cảm biến (đơn vị ADC, 1g ≈ 16384).
  Dùng để debug, không dùng trực tiếp vì nhiễu.

- Axf, Ayf, Azf:
  Gia tốc sau lọc low-pass.
  Đây là dữ liệu chính dùng để tính toán góc.

- Pitch (deg): (xét đầu đối diện với đầu có header)
  Góc cúi/ngửa theo trục X, dương khi cúi xuống, âm khi ngửa lên.

- Roll (deg):
  Góc nghiêng trái/phải theo trục Y, âm khi nghiêng trái, dương khi nghiêng phải.

- Beta (β):
  β = sqrt(pitch² + roll²)
  → Góc nghiêng tổng.
  → Đại lượng QUAN TRỌNG NHẤT để đánh giá độ nghiêng/biến dạng đất.

- Beta_dot (β_dot):
  Tốc độ thay đổi góc nghiêng (deg/s), quan tâm độ lớn tuyệt đối |β_dot|.
  → ≈ 0: ổn định
  → lớn: có chuyển động (lún, trượt)
    |β_dot| rất lớn (ví dụ > 10–20 deg/s) → có rung mạnh / chuyển động thật
  → QUAN TRỌNG để phát hiện nguy cơ sớm.

- A_rms_g:
  Độ lớn vector gia tốc.
  → > 1.2g: dao động mạnh
  → 0.9g ≤ A_rms ≤ 1.1g → ổn định
  → A_rms > 1.1g       → có rung
  → A_rms < 0.9g       → cần kiểm tra / lệch, sai số 
  → Nếu A_rms_g tăng đột biến cùng lúc với |β_dot| tăng mạnh → có thể đang có chuyển động mạnh (lún nhanh, trượt mạnh).
  → Dùng hỗ trợ phát hiện bất ổn.

=> Đại lượng quan trọng trong hệ:
1. β      → mức độ nghiêng đất (chính)
2. β_dot  → tốc độ biến dạng (cảnh báo sớm)
3. A_rms  → rung động (bổ trợ)

=> Kết luận:
MPU6050 không đo trực tiếp độ giãn đất,
mà suy ra trạng thái nghiêng và chuyển động của đất thông qua β, β_dot và A_rms.
*/