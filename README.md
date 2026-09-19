# Hệ thống nhúng IoT đo độ giãn của đất

Đồ án xây dựng hệ thống giám sát độ ẩm, độ nghiêng và rung động của đất sử dụng **ESP32**, hướng tới đánh giá mức độ ổn định và cảnh báo nguy cơ sạt lở. Dữ liệu được truyền qua **LoRa 433 MHz** về Gateway, sau đó gửi lên **ThingsBoard qua Wi-Fi/MQTT** để theo dõi từ xa.

## Mô hình hệ thống

`Cảm biến → Node ESP32 → LoRa → Gateway ESP32 → Wi-Fi/MQTT → ThingsBoard`

- **Node:** đọc cảm biến độ ẩm đất điện dung và MPU6050; lọc dữ liệu, tính góc nghiêng, tốc độ thay đổi góc nghiêng và rung động RMS. Node sử dụng deep sleep để tiết kiệm năng lượng; cấu hình hiện tại đo ngay khi khởi động, sau đó khoảng 30 phút/lần.
- **Gateway:** nhận và kiểm tra gói tin, tính hệ số an toàn **FS**, chỉ số động **DI** và chỉ số **ε* = 1/FS**; phân loại trạng thái `NORMAL`, `WARNING`, `DANGER` và gửi kết quả lên ThingsBoard.
- **Truyền dữ liệu:** kiểm tra CRC, xác nhận ACK, phát hiện gói trùng; Node lưu một gói chưa được xác nhận để gửi lại, Gateway có bộ đệm RAM tối đa 8 bản tin khi mất kết nối mạng.

## Phần cứng và phần mềm

- Hai ESP32 DevKit V1 và hai module LoRa RA-02 (SX1278).
- Một cảm biến độ ẩm đất điện dung và một MPU6050 tại Node.
- Firmware C++ sử dụng Arduino framework, quản lý và biên dịch bằng PlatformIO.

## Cấu trúc mã nguồn

- [`src/node/main.cpp`](src/node/main.cpp): thu thập cảm biến, truyền LoRa và quản lý deep sleep.
- [`src/gateway/main.cpp`](src/gateway/main.cpp): nhận dữ liệu, xử lý và kết nối ThingsBoard.
- [`src/common/`](src/common/): cấu hình phần cứng, giao thức truyền và mô hình phân tích.

## Chạy dự án

Cập nhật Wi-Fi, token ThingsBoard và thông số hiệu chuẩn trong [`src/common/project_config.h`](src/common/project_config.h), sau đó nạp firmware tương ứng cho từng ESP32:

```bash
pio run -e node -t upload
pio run -e gateway -t upload
```

Serial Monitor sử dụng tốc độ **115200 baud**.

**Phạm vi mô hình:** ε* là chỉ số suy ra từ mô hình, không phải phép đo trực tiếp độ giãn dài của đất. Các thông số đất và hiệu chuẩn hiện tại là giá trị tạm, cần được xác định lại bằng thực nghiệm.
