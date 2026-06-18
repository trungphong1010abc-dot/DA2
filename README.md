# Hệ thống nhúng IoT đo độ giãn của đất

Đề tài: thiết kế hệ thống nhúng IoT đo độ giãn của đất sử dụng ESP32, cảm biến độ ẩm đất điện dung và MPU6050. Bản hiện tại không dùng PT100.

## Firmware

Project có 2 firmware PlatformIO:

- `gateway`: ESP32 + RA-02 LoRa + WiFi, nhận dữ liệu node và publish telemetry lên ThingsBoard.
- `node`: ESP32 + RA-02 LoRa + MPU6050 + cảm biến độ ẩm đất điện dung + mạch chia áp pin.

## Build nhanh

```bash
pio run -e gateway
pio run -e node
```

Upload tùy board đang cắm USB:

```bash
pio run -e gateway -t upload
pio run -e node -t upload
```

Mặc định `platformio.ini` build env `gateway`. Đổi `default_envs` nếu muốn build node bằng nút Build của PlatformIO.

## Cấu hình chính

Sửa trong `src/common/project_config.h`:

- WiFi và ThingsBoard MQTT.
- Chân RA-02, MPU6050, soil ADC, battery ADC.
- `SOIL_ADC_DRY`, `SOIL_ADC_WET` sau khi hiệu chuẩn cảm biến độ ẩm đất.
- Tham số cơ học đất: gamma, chiều dày lớp trượt, lực dính, góc ma sát trong, ngưỡng cảnh báo.

## Bộ tài liệu thiết kế

Đọc theo thứ tự:

1. `README/00_DESIGN_RULES.md` - đặc tả gốc, ngưỡng, công thức, giao thức và tiêu chí kiểm thử.
2. `README/01_HARDWARE_INTERFACES.md` - đấu nối ESP32, chuẩn giao tiếp từng module, nguồn và lọc nhiễu.
3. `README/02_SYSTEM_FLOW.md` - luồng node, gateway, ACK, ThingsBoard và deep sleep.
4. `README/03_SOIL_PARAMETER_PROFILE.md` - cơ sở chọn bộ tham số đất đỏ bazan.
