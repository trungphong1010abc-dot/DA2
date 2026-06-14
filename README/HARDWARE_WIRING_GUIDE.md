# Hướng dẫn kết nối phần cứng

Đề tài: thiết kế hệ thống nhúng IoT đo độ giãn của đất sử dụng ESP32, cảm biến độ ẩm đất điện dung và MPU6050.

## 1. Kiến trúc phần cứng

- Node: ESP32 + RA-02 LoRa + MPU6050 + cảm biến độ ẩm đất điện dung + mạch chia áp pin.
- Gateway: ESP32 + RA-02 LoRa + WiFi, nhận dữ liệu node và đẩy MQTT lên ThingsBoard.
- Nguồn logic toàn hệ: 3.3 V. RA-02 không chịu logic 5 V.

## 2. Đấu nối LoRa RA-02 với ESP32

Dùng cùng một sơ đồ chân cho node và gateway:

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

## 3. Đấu nối node

### MPU6050

| MPU6050 | ESP32 |
|---|---:|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO21 |
| SCL | GPIO22 |
| AD0 | GND |
| INT | Không bắt buộc |

Nếu module MPU6050 không có điện trở kéo lên I2C, gắn thêm 2 điện trở 4.7 kOhm từ SDA lên 3V3 và từ SCL lên 3V3.

### Cảm biến độ ẩm đất điện dung

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
ADC_wet = 1200
H_soil = (3400 - ADC_filtered) * 100 / 2200
```

Các giá trị này chưa được cập nhật vào code trong bước tài liệu hiện tại. Khi
triển khai firmware, cần cập nhật `SOIL_ADC_DRY` và `SOIL_ADC_WET`. Hồ sơ hiệu
chuẩn nên ghi ngày đo, cảm biến, điện áp cấp, loại đất, độ sâu cắm và thống kê
các mẫu ADC tại hai trạng thái wet/dry.

### Mạch chia áp pin

Mắc như sau:

`VBAT+ -> 220k -> GPIO35 -> 100k -> GND`

| Điểm mạch | Kết nối |
|---|---|
| Đầu trên 220 kOhm | Cực dương pin |
| Điểm giữa 220 kOhm và 100 kOhm | GPIO35 |
| Đầu dưới 100 kOhm | GND |
| Cực âm pin | GND chung |

Tỉ lệ chia áp: `V_adc = V_bat * 100 / (220 + 100)`. Pin Li-ion 4.2 V sẽ còn khoảng 1.31 V tại ADC, an toàn cho ESP32.

## 4. Đấu nối gateway

Gateway chỉ cần ESP32 + RA-02 theo bảng LoRa ở mục 2. Gateway kết nối WiFi `Khoa` và publish MQTT lên ThingsBoard bằng token đã cấu hình trong `src/common/project_config.h`.

## 5. Tụ lọc nhiễu nên gắn

### Nguyên tắc chọn tụ

- Tụ gốm `100 nF`: lọc nhiễu cao tần, nên đặt sát chân nguồn từng module.
- Tụ hóa `10 uF` đến `100 uF`: giảm dao động nguồn cục bộ cho module, đặc biệt là RA-02 khi phát LoRa.
- Tụ hóa `470 uF` đến `1000 uF`: dùng làm tụ bulk trên rail nguồn chính khi test breadboard hoặc khi nguồn 3.3 V bị sụt áp.
- Tụ hóa có phân cực: chân `+` vào nguồn dương, chân `-` vào GND.
- Tụ 1000 uF không thay thế tụ gốm 100 nF; nên dùng cả hai loại.

### Khi test trên breadboard

Breadboard dễ sụt áp và nhiễu hơn PCB, nhất là khi RA-02 phát. Khi test nhanh, nên gắn tối thiểu:

- Trên rail nguồn chính `3V3-GND`: 1 tụ gốm `100 nF` và 1 tụ hóa `470 uF` hoặc `1000 uF`.
- Gần RA-02: 1 tụ gốm `100 nF` sát VCC/GND và 1 tụ hóa `47 uF` hoặc `100 uF` sát module.
- Gần MPU6050: 1 tụ gốm `100 nF` giữa VCC và GND; thêm `10 uF` nếu dây I2C dài hoặc góc/rung bị nhiễu.
- Gần cảm biến độ ẩm đất: 1 tụ gốm `100 nF` giữa VCC và GND.
- Tại chân AO/GPIO34: 1 tụ gốm `100 nF` từ GPIO34 xuống GND để lọc nhiễu ADC.
- Tại chân đo pin GPIO35: 1 tụ gốm `100 nF` từ GPIO35 xuống GND; nếu số đo pin vẫn dao động thì thay bằng `1 uF`.

Nếu LoRa phát làm ESP32 reset hoặc gateway/node mất gói nhiều, ưu tiên kiểm tra nguồn 3.3 V trước, sau đó tăng tụ bulk rail chính lên `1000 uF`.

### Trên nguồn ESP32

- Gắn 1 tụ gốm `100 nF` giữa 3V3 và GND, càng gần chân cấp nguồn ESP32 càng tốt.
- Gắn thêm 1 tụ hóa `100 uF` đến `470 uF` giữa 3V3 và GND trên rail nguồn chính.
- Nếu test breadboard hoặc nguồn yếu, có thể dùng tụ hóa `1000 uF` trên rail `3V3-GND`.
- Chọn tụ hóa chịu áp tối thiểu `6.3 V`; nên dùng `10 V` hoặc `16 V` nếu có.

### Trên module RA-02

- Gắn 1 tụ gốm `100 nF` sát chân VCC/GND của RA-02.
- Gắn 1 tụ hóa `47 uF` đến `100 uF` sát module RA-02.
- Nếu LoRa reset ngẫu nhiên khi phát, giữ tụ sát RA-02 và tăng tụ bulk nguồn chính lên `1000 uF`.

### Trên MPU6050

- Gắn 1 tụ gốm `100 nF` giữa VCC và GND sát module.
- Nếu dây I2C dài, gắn thêm `10 uF` giữa VCC và GND gần MPU6050.

### Trên cảm biến độ ẩm đất

- Gắn 1 tụ gốm `100 nF` giữa VCC và GND gần cảm biến.
- Gắn 1 tụ gốm `100 nF` từ chân AO/GPIO34 xuống GND gần ESP32 để lọc nhiễu ADC.
- Nếu tín hiệu AO còn nhiễu, có thể thêm điện trở nối tiếp `1 kOhm` giữa AO và GPIO34; tụ `100 nF` vẫn đặt từ GPIO34 xuống GND.

### Trên mạch chia áp pin

- Gắn 1 tụ gốm `100 nF` từ GPIO35 xuống GND.
- Nếu giá trị pin dao động mạnh, có thể thay bằng `1 uF`. Khi dùng `1 uF`, sau khi wake-up nên đợi ít nhất 100 ms trước khi đọc ADC.

## 6. Lưu ý nguồn và dây nối

- Tất cả GND phải nối chung: ESP32, RA-02, MPU6050, cảm biến độ ẩm, mạch chia áp pin.
- Không cấp RA-02 từ nguồn 5 V.
- Dây SPI LoRa nên ngắn. Dây antenna phải đúng loại 433 MHz nếu firmware để 433 MHz.
- Cảm biến độ ẩm đất điện dung nên bọc/giữ đầu nối khô, tránh nước chạm mạch.
