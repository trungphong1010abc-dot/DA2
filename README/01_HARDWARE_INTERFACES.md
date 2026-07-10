# 01. Hardware Interfaces - Thiết kế, kết nối và chuẩn giao tiếp phần cứng

Phần này trình bày kiến trúc phần cứng, phương án đấu nối và các yêu cầu về
nguồn của hệ thống nhúng IoT đo gián tiếp biến dạng đất. Hệ thống sử dụng ESP32,
module LoRa RA-02, cảm biến độ ẩm đất điện dung và MPU6050. Các bảng đấu nối là
cơ sở để xây dựng Bảng 3.1 và sơ đồ kết nối tổng thể trong báo cáo.

File này là phụ lục phần cứng của `00_DESIGN_RULES.md`. Khi có thay đổi chân,
chuẩn giao tiếp hoặc điện áp, phải cập nhật cả cấu hình trong
`src/common/project_config.h`.

## 3.3.1 Kiến trúc phần cứng

- Node: ESP32 + RA-02 LoRa + MPU6050 + cảm biến độ ẩm đất điện dung.
- Gateway: ESP32 + RA-02 LoRa + WiFi, nhận dữ liệu node và đẩy MQTT lên ThingsBoard.
- Nguồn logic toàn hệ: 3.3 V. RA-02 không chịu logic 5 V.

**Bảng 3.1. Tổng hợp module, chuẩn giao tiếp và chân ESP32**

| Khối | Chuẩn giao tiếp với ESP32 | Chân ESP32 | Dùng ở | Ghi chú |
| --- | --- | --- | --- | --- |
| RA-02/SX1278 LoRa | SPI + DIO0 interrupt | SCK GPIO18, MISO GPIO19, MOSI GPIO23, CS GPIO5, RST GPIO14, DIO0 GPIO26 | Node và gateway | 433 MHz, sync word `0xDA`, SF9, BW 125 kHz, CR 4/5 |
| MPU6050 | I2C | SDA GPIO21, SCL GPIO22, địa chỉ `0x68` khi AD0 nối GND | Node | Đọc WHO_AM_I, cấu hình accelerometer `+-2 g` |
| Cảm biến độ ẩm đất điện dung | Analog ADC1 | AO -> GPIO34 | Node | ADC 12 bit, median 11 mẫu, DO không dùng |
| Wi-Fi ESP32 gateway | Wi-Fi 2.4 GHz tích hợp | Không dùng GPIO ngoài | Gateway | MQTT publish lên ThingsBoard |

## 3.3.2 Kết nối LoRa RA-02 với ESP32

Node và gateway sử dụng cùng cấu hình chân SPI cho RA-02 như trình bày trong
bảng sau.

**Bảng 3.1a. Kết nối giữa RA-02 và ESP32**

| RA-02 | ESP32 | Ghi chú |
|---|---:|---|
| VCC | 3V3 | Nguồn 3.3 V ổn định, nên cấp đủ dòng >= 500 mA |
| GND | GND | GND chung với ESP32 và cảm biến |
| SCK | GPIO18 | SPI SCK |
| MISO | GPIO19 | SPI MISO |
| MOSI | GPIO23 | SPI MOSI |
| NSS/CS | GPIO5 | Chip select |
| RST | GPIO14 | Reset LoRa |
| DIO0 | GPIO26 | Ngắt RX/TX done |

Thông số firmware mặc định: 433 MHz, sync word `0xDA`, SF9, BW 125 kHz, CR 4/5, TX power 17 dBm.

## 3.3.3 Thiết kế phần cứng node

### 3.3.3.1 Kết nối MPU6050

**Bảng 3.1b. Kết nối giữa MPU6050 và ESP32 node**

| MPU6050 | ESP32 |
|---|---:|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO21 |
| SCL | GPIO22 |
| AD0 | GND |
| INT | Không bắt buộc |

Nếu module MPU6050 không có điện trở kéo lên I2C, gắn thêm 2 điện trở 4.7 kOhm từ SDA lên 3V3 và từ SCL lên 3V3.

### 3.3.3.2 Kết nối cảm biến độ ẩm đất điện dung

**Bảng 3.1c. Kết nối cảm biến độ ẩm với ESP32 node**

| Cảm biến | ESP32 |
|---|---:|
| VCC | 3V3 |
| GND | GND |
| AO | GPIO34 |
| DO | Không dùng |

GPIO34 là chân input-only ADC1. Firmware đọc ADC 12 bit, lọc median 11 mẫu, sau đó tính:

`H_soil = (ADC_dry - ADC_filtered) * 100 / (ADC_dry - ADC_wet)`

Kết quả hiệu chuẩn hiện tại do người dùng cung cấp:

```text
ADC_dry = 3400
ADC_wet = 1400
H_soil = (3400 - ADC_filtered) * 100 / 2000
```

Các giá trị này đang được cấu hình trong `SOIL_ADC_DRY` và `SOIL_ADC_WET`. Hồ
sơ hiệu chuẩn nên ghi ngày đo, cảm biến, điện áp cấp, loại đất, độ sâu cắm và
thống kê các mẫu ADC tại hai trạng thái wet/dry.

## 3.3.4 Thiết kế phần cứng gateway

Gateway gồm ESP32 và RA-02 theo cấu hình tại mục 3.3.2. ESP32 gateway kết nối
vào mạng Wi-Fi được khai báo trong cấu hình hệ thống, sau đó publish telemetry
lên ThingsBoard bằng MQTT. SSID, mật khẩu và token thiết bị không được ghi trực
tiếp trong báo cáo hoặc tài liệu công khai.

## 3.3.5 Thiết kế lọc nguồn và chống nhiễu

### 3.3.5.1 Nguyên tắc lựa chọn tụ

- Tụ gốm `100 nF`: lọc nhiễu cao tần, nên đặt sát chân nguồn từng module.
- Tụ hóa `10 uF` đến `100 uF`: giảm dao động nguồn cục bộ cho module, đặc biệt là RA-02 khi phát LoRa.
- Tụ hóa `470 uF` đến `1000 uF`: dùng làm tụ bulk trên rail nguồn chính khi test breadboard hoặc khi nguồn 3.3 V bị sụt áp.
- Tụ hóa có phân cực: chân `+` vào nguồn dương, chân `-` vào GND.
- Tụ 1000 uF không thay thế tụ gốm 100 nF; nên dùng cả hai loại.

### 3.3.5.2 Bố trí khi thử nghiệm trên breadboard

Breadboard dễ sụt áp và nhiễu hơn PCB, nhất là khi RA-02 phát. Khi test nhanh, nên gắn tối thiểu:

- Trên rail nguồn chính `3V3-GND`: 1 tụ gốm `100 nF` và 1 tụ hóa `470 uF` hoặc `1000 uF`.
- Gần RA-02: 1 tụ gốm `100 nF` sát VCC/GND và 1 tụ hóa `47 uF` hoặc `100 uF` sát module.
- Gần MPU6050: 1 tụ gốm `100 nF` giữa VCC và GND; thêm `10 uF` nếu dây I2C dài hoặc góc/rung bị nhiễu.
- Gần cảm biến độ ẩm đất: 1 tụ gốm `100 nF` giữa VCC và GND.
- Tại chân AO/GPIO34: 1 tụ gốm `100 nF` từ GPIO34 xuống GND để lọc nhiễu ADC.
Nếu LoRa phát làm ESP32 reset hoặc gateway/node mất gói nhiều, ưu tiên kiểm tra nguồn 3.3 V trước, sau đó tăng tụ bulk rail chính lên `1000 uF`.

### 3.3.5.3 Lọc nguồn ESP32

- Gắn 1 tụ gốm `100 nF` giữa 3V3 và GND, càng gần chân cấp nguồn ESP32 càng tốt.
- Gắn thêm 1 tụ hóa `100 uF` đến `470 uF` giữa 3V3 và GND trên rail nguồn chính.
- Nếu test breadboard hoặc nguồn yếu, có thể dùng tụ hóa `1000 uF` trên rail `3V3-GND`.
- Chọn tụ hóa chịu áp tối thiểu `6.3 V`; nên dùng `10 V` hoặc `16 V` nếu có.

### 3.3.5.4 Lọc nguồn RA-02

- Gắn 1 tụ gốm `100 nF` sát chân VCC/GND của RA-02.
- Gắn 1 tụ hóa `47 uF` đến `100 uF` sát module RA-02.
- Nếu LoRa reset ngẫu nhiên khi phát, giữ tụ sát RA-02 và tăng tụ bulk nguồn chính lên `1000 uF`.

### 3.3.5.5 Lọc nguồn MPU6050

- Gắn 1 tụ gốm `100 nF` giữa VCC và GND sát module.
- Nếu dây I2C dài, gắn thêm `10 uF` giữa VCC và GND gần MPU6050.

### 3.3.5.6 Lọc tín hiệu cảm biến độ ẩm

- Gắn 1 tụ gốm `100 nF` giữa VCC và GND gần cảm biến.
- Gắn 1 tụ gốm `100 nF` từ chân AO/GPIO34 xuống GND gần ESP32 để lọc nhiễu ADC.
- Nếu tín hiệu AO còn nhiễu, có thể thêm điện trở nối tiếp `1 kOhm` giữa AO và GPIO34; tụ `100 nF` vẫn đặt từ GPIO34 xuống GND.

## 3.3.6 Yêu cầu thi công và an toàn nguồn

- Tất cả GND phải nối chung: ESP32, RA-02, MPU6050 và cảm biến độ ẩm.
- Không cấp RA-02 từ nguồn 5 V.
- Dây SPI LoRa nên ngắn. Dây antenna phải đúng loại 433 MHz nếu firmware để 433 MHz.
- Cảm biến độ ẩm đất điện dung nên bọc/giữ đầu nối khô, tránh nước chạm mạch.

## 3.3.7 Kết luận mục

Thiết kế phần cứng sử dụng chung nền tảng ESP32 và RA-02 cho cả node và gateway,
giúp thống nhất giao tiếp LoRa và giảm số loại linh kiện. Node được bổ sung
MPU6050 và cảm biến độ ẩm, trong khi gateway đảm nhiệm kết nối
Internet. Yêu cầu quan trọng nhất khi lắp ráp là sử dụng nguồn 3,3 V ổn định,
nối chung GND và bố trí tụ lọc gần các module tiêu thụ dòng xung lớn.
