# Quy tắc thiết kế hệ thống DA2

## 1. Mục đích và phạm vi

Tài liệu này là đặc tả thiết kế bắt buộc cho hệ thống IoT giám sát độ ẩm,
độ nghiêng, rung động và nguy cơ mất ổn định của đất. Mọi thay đổi firmware,
giao thức LoRa, xử lý dữ liệu, ThingsBoard và kiến trúc thực thi phải được đối chiếu với
tài liệu này trước khi triển khai.

Nguồn yêu cầu:

- Flowchart cảm biến độ ẩm đất điện dung.
- Flowchart MPU6050.
- Flowchart Node → Gateway.
- Flowchart Gateway → Node.
- Flowchart Gateway → ThingsBoard IoT Application.
- Flowchart Adaptive Duty Cycle.
- Tài liệu `C:\Users\Public\Code.pdf`.
- Tài liệu `C:\Users\Public\New Section 1.pdf`.

Tài liệu này mô tả thiết kế mục tiêu. Nó không khẳng định firmware hiện tại đã
hoàn thành toàn bộ chức năng được nêu.

Kiến trúc thực thi giai đoạn hiện tại là **superloop tuần tự** dựa trên
`setup()` và `loop()`. Chưa thiết kế hoặc triển khai các FreeRTOS task, queue,
mutex hay cơ chế chạy song song ở tầng ứng dụng. FreeRTOS chỉ là hướng nâng cấp
sau khi superloop đã ổn định và được kiểm thử đầy đủ.

## 2. Nguyên tắc ưu tiên

Khi có xung đột, áp dụng thứ tự ưu tiên sau:

1. Dữ liệu đo thực tế và trạng thái lỗi không được làm sai lệch.
2. Công thức, đơn vị và ngưỡng đã được chốt trong flowchart/Code.pdf.
3. Tính toàn vẹn packet, khả năng truy vết và chống xử lý trùng.
4. An toàn nguồn, pin và chu kỳ ngủ.
5. Khả năng gửi dữ liệu và phục hồi khi mất kết nối.
6. Tối ưu hiệu năng, bộ nhớ và năng lượng.

Không được sửa dữ liệu chỉ để dashboard trông hợp lý hoặc để tránh `NaN`,
`Inf`, số âm hay giá trị ngoài dự kiến.

## 3. Quy tắc dữ liệu chung

### 3.1 Phân loại dữ liệu

Mỗi biến phải thuộc đúng một nhóm:

- **Raw data**: dữ liệu đọc trực tiếp hoặc giá trị ADC thô.
- **Filtered data**: dữ liệu sau bộ lọc, phải giữ được raw data để truy vết.
- **Derived data**: dữ liệu tính từ raw/filtered data.
- **Analysis data**: kết quả mô hình địa kỹ thuật và chỉ số cảnh báo.
- **State data**: trạng thái cảm biến, node, mạng, pin, cảnh báo và duty cycle.
- **Control data**: command, ACK, retry, sleep duration và cấu hình từ xa.

Không được ghi đè raw data bằng filtered data hoặc analysis data.

### 3.2 Tên biến chuẩn

Tên truyền qua LoRa, Serial, local buffer và ThingsBoard phải ánh xạ được về
các tên chuẩn sau:

| Nhóm | Tên chuẩn | Ý nghĩa | Đơn vị |
| --- | --- | --- | --- |
| Định danh | `gateway_id` | Mã gateway | Không đơn vị |
| Định danh | `node_id` | Mã node | Không đơn vị |
| Định danh | `packet_id` | Số thứ tự packet | Không đơn vị |
| Thời gian | `timestamp_ms` | Thời điểm tạo mẫu tại node | ms |
| Soil | `soil_adc_raw` | Mẫu ADC thô nếu được lưu | count |
| Soil | `soil_adc_filtered` | ADC sau median filter | count |
| Soil | `h_soil` | Độ ẩm đất tương đối | % |
| MPU | `ax_raw`, `ay_raw`, `az_raw` | Gia tốc thô đã đổi thang | g |
| MPU | `ax_filtered`, `ay_filtered`, `az_filtered` | Gia tốc low-pass | g |
| MPU | `pitch_deg` | Góc pitch | độ |
| MPU | `roll_deg` | Góc roll | độ |
| MPU | `beta_deg` | Độ nghiêng tổng | độ |
| MPU | `beta_dot_deg_per_hour` | Tốc độ đổi góc | độ/giờ |
| MPU | `a_rms_g` | Chỉ số rung RMS | g |
| Nguồn | `v_bat` | Điện áp pin | V |
| RF | `lora_rssi` | Cường độ tín hiệu nhận | dBm |
| Lỗi | `error_flag` | Bitmask lỗi | Không đơn vị |
| Phân tích | `u_kpa` | Áp lực nước lỗ rỗng | kPa |
| Phân tích | `tau_kpa` | Ứng suất gây trượt | kPa |
| Phân tích | `sigma_n_kpa` | Ứng suất pháp tuyến tổng | kPa |
| Phân tích | `sigma_effective_kpa` | Ứng suất hữu hiệu | kPa |
| Phân tích | `tau_f_kpa` | Sức kháng cắt | kPa |
| Phân tích | `fs` | Hệ số an toàn | Không đơn vị |
| Phân tích | `di` | Chỉ số động học | Không đơn vị |
| Phân tích | `epsilon_star` | Độ giãn tương đối quy ước | Không đơn vị |
| Trạng thái | `alert_level` | `NORMAL/WARNING/DANGER` | Chuỗi |
| Trạng thái | `risk_status` | Trạng thái rủi ro chi tiết | Chuỗi |
| Trạng thái | `warning_message` | Nội dung cảnh báo | Chuỗi |
| Duty cycle | `sleep_duration_sec` | Thời gian ngủ tiếp theo | giây |
| Duty cycle | `duty_cycle_mode` | Lý do chọn chu kỳ | Chuỗi |

Tên alias chỉ được dùng để tương thích ngược. Tên chuẩn phải luôn tồn tại.

### 3.3 Giá trị không hợp lệ

- Không thay giá trị không tính được bằng `0`, `99`, `9.99` hoặc giá trị giả.
- Dùng cờ `valid`, `sensor_status`, `analysis_valid` và `error_flag`.
- JSON dùng `null` cho số không xác định nếu nền tảng tiếp nhận hỗ trợ.
- Serial phải in rõ `invalid` hoặc `nan`; không được che lỗi.
- Không tự ép `beta`, `sigma_effective`, `FS`, `DI` hoặc `epsilon_star` về một
  miền khác nếu flowchart không yêu cầu.
- Chỉ được clamp tại bước được flowchart chỉ định, ví dụ `H_soil` về 0–100% và
  `Sleep_Duration` về 5–30 phút.

### 3.4 Timestamp

- `timestamp_ms` của packet là thời gian node tạo dữ liệu, không phải thời gian
  gateway nhận packet.
- Nếu cần thời gian thực UTC, bổ sung trường riêng như `received_at_utc` hoặc
  đồng bộ thời gian cho node; không đổi nghĩa `timestamp_ms`.

## 4. Thiết kế cảm biến độ ẩm đất

### 4.1 Khởi tạo

- ESP32 dùng ADC1, độ phân giải 12 bit, attenuation 11 dB.
- Chỉ dùng chân ADC1 phù hợp để tránh xung đột Wi-Fi.
- Cấu hình chân trước khi bắt đầu chu kỳ lấy mẫu.
- Chờ cảm biến ổn định trong khoảng 500–1000 ms sau khi cấp nguồn.

### 4.2 Kiểm tra ADC và retry

Luồng bắt buộc:

`Đọc ADC một mẫu → kiểm tra ADC_min < ADC < ADC_max`

- Mẫu hợp lệ: reset bộ đếm lỗi ADC và tiếp tục thu thập.
- Mẫu không hợp lệ: tăng bộ đếm lỗi ADC.
- Nếu số lỗi chưa vượt ngưỡng: chờ 300–500 ms rồi đọc lại.
- Nếu lỗi ADC lớn hơn 3 lần: đặt lỗi cảm biến, đánh dấu soil status lỗi và bỏ
  mẫu hiện tại.
- Không đưa mẫu ngoài miền hợp lệ vào median filter.

### 4.3 Thu thập và lọc

- Thu đủ số mẫu lẻ `N`; flowchart đề xuất 10–11 mẫu và nên chốt `N = 11`.
- Có thể bỏ ba mẫu đầu sau khi cấp nguồn nếu xác nhận chúng chỉ phục vụ ổn định.
- Áp dụng median filter để tạo `soil_adc_filtered`.
- Không dùng trung bình thay median nếu chưa cập nhật lại đặc tả.

### 4.4 Hiệu chuẩn độ ẩm

Công thức chuẩn:

```text
H_soil(%) = (ADC_dry - ADC_filtered) × 100 / (ADC_dry - ADC_wet)
```

Quy tắc:

- Giá trị hiệu chuẩn hiện tại đã được người dùng cung cấp:

```text
ADC_dry = 3400
ADC_wet = 1200
```

- Hai giá trị trên chỉ áp dụng cho đúng cảm biến và điều kiện hiệu chuẩn tương
  ứng; thay cảm biến/điện áp/loại đất phải đánh giá lại.
- Sau tính toán mới giới hạn `H_soil` vào 0–100%.
- Lưu cả `soil_adc_filtered` và `h_soil`.
- Khi mẫu lỗi, không tái sử dụng âm thầm giá trị cũ như dữ liệu mới.

### 4.5 Kết quả SoilData

Hàm/khối xử lý Soil phải trả tối thiểu:

```text
soil_adc_filtered, h_soil, soil_status, error_flag, timestamp_ms
```

## 5. Thiết kế MPU6050

### 5.1 Khởi tạo và xác minh

- Khởi tạo ESP32, I2C và MPU6050.
- Wake-up MPU6050.
- Cấu hình `ACCEL_CONFIG`, DLPF và `SMPLRT_DIV`.
- Đọc `WHO_AM_I`; chỉ tiếp tục khi địa chỉ thiết bị hợp lệ theo phần cứng.
- Nếu xác minh thất bại, đặt lỗi MPU và thực hiện retry có giới hạn.

### 5.2 Chu kỳ lấy mẫu

- Lấy thời gian bằng bộ đếm monotonic.
- Chỉ đọc mẫu mới khi khoảng thời gian từ mẫu trước đạt ít nhất 200 ms.
- `dt` phải được tính từ timestamp thực tế của hai mẫu, không giả định cố định.
- Nếu đọc I2C lỗi, mẫu đó không được dùng để cập nhật bộ lọc.

### 5.3 Low-pass filter

Với `alpha = 0.85`:

```text
Axf = alpha × Axf_prev + (1 - alpha) × Ax_raw
Ayf = alpha × Ayf_prev + (1 - alpha) × Ay_raw
Azf = alpha × Azf_prev + (1 - alpha) × Az_raw
```

Bộ lọc lần đầu được khởi tạo bằng mẫu đọc đầu tiên, không khởi tạo bằng 0.

### 5.4 Tính góc

```text
pitch = atan2(-Axf, sqrt(Ayf² + Azf²))
roll  = atan2( Ayf, sqrt(Axf² + Azf²))

pitch_deg = pitch × 180 / pi
roll_deg  = roll  × 180 / pi
beta_deg  = sqrt(pitch_deg² + roll_deg²)
```

Không tự giới hạn `beta_deg` để làm ổn định mô hình phân tích.

### 5.5 Tốc độ đổi góc

```text
beta_dot_deg_per_hour = abs(beta_deg - beta_prev_deg) / dt_seconds × 3600
```

- Khi `dt <= 0`, kết quả phải là không hợp lệ hoặc 0 theo đúng trạng thái khởi
  tạo đã công bố; không được chia cho một thời gian giả.
- Cần phân biệt tốc độ trong cửa sổ lấy mẫu và tốc độ giữa hai chu kỳ ngủ.
- Trường telemetry phải ghi rõ loại tốc độ đang sử dụng nếu cả hai cùng tồn tại.

### 5.6 Chỉ số rung

```text
Ax_vib = Ax_raw - Axf
Ay_vib = Ay_raw - Ayf
Az_vib = Az_raw - Azf
```

`a_rms_g` phải là RMS trên toàn cửa sổ mẫu:

```text
a_rms_g = sqrt(sum(Ax_vib² + Ay_vib² + Az_vib²) / sample_count)
```

Không gọi độ lớn rung của một mẫu duy nhất là RMS.

### 5.7 Kết quả MPUData

Hàm/khối xử lý MPU phải trả tối thiểu:

```text
beta_deg, beta_dot_deg_per_hour, a_rms_g,
pitch_deg, roll_deg, timestamp_ms, mpu_status, error_flag
```

## 6. Kiểm tra dữ liệu node

Trước khi đóng packet, node phải kiểm tra tối thiểu:

```text
0% <= h_soil <= 100%
0° <= beta_deg <= 60°
abs(beta_dot_deg_per_hour) <= 5°/s quy đổi đúng đơn vị thiết kế
0 g <= a_rms_g <= 1 g
3.2 V <= v_bat <= 4.2 V
```

Lưu ý bắt buộc: flowchart đang ghi ngưỡng `beta_dot` theo `°/s`, trong khi
telemetry chuẩn dùng `°/h`. Trước khi code validation, phải chốt một đơn vị và
quy đổi ngưỡng rõ ràng. Không so sánh trực tiếp hai đơn vị khác nhau.

Dữ liệu lỗi vẫn có thể được đóng packet để gateway biết trạng thái, nhưng phải
có `error_flag != 0`; không được giả thành packet bình thường.

## 7. Giao thức Node → Gateway

### 7.1 Packet dữ liệu

Packet phải có tối thiểu:

```text
Header, Gateway_ID, Node_ID, Packet_ID, timestamp,
H_soil, beta, beta_dot, A_rms, pitch_deg, roll_deg,
V_bat, Error_Flag, CRC
```

Gateway bổ sung RSSI tại thời điểm nhận; node không tự khai RSSI chiều nhận.

### 7.2 Toàn vẹn và chống trùng

- Kiểm tra header và CRC trước khi parse trường dữ liệu.
- Kiểm tra `gateway_id` và `node_id` trước khi xử lý.
- Dùng cặp `(node_id, packet_id)` để phát hiện duplicate.
- Packet trùng không được lưu hoặc publish như dữ liệu mới.
- Packet trùng hợp lệ vẫn có thể nhận ACK `DUPLICATE` để node dừng retry.
- Không cập nhật `last_packet_id` trước khi packet qua kiểm tra cần thiết.

### 7.3 ACK và retry

- Node gửi packet rồi chờ ACK trong `T_ACK`.
- ACK phải chứa tối thiểu `node_id`, `packet_id`, `status`, `CRC`.
- Thiết kế hiện tại còn cần `sleep_duration` và `alert_level` trong ACK.
- Node chỉ chấp nhận ACK đúng `node_id`, `packet_id` và CRC.
- Timeout hoặc ACK lỗi làm tăng retry counter.
- Khi đạt `MAX_RETRY`, lưu packet vào buffer gửi lại sau và chuyển sang sleep.
- Packet buffer phải có giới hạn, chính sách ghi đè và chỉ báo mất dữ liệu.

## 8. Giao thức Gateway → Node

Các command mục tiêu:

```text
SET_SLEEP_DURATION
SET_THRESHOLD
REQUEST_DATA
SLEEP_NOW
```

Packet command phải gồm:

```text
Header, Gateway_ID, Node_ID, Command, Parameter, Packet_ID, CRC
```

Quy tắc:

- Gateway kiểm tra `node_id` trước khi gửi.
- Node kiểm tra header, CRC, `node_id` và duplicate `packet_id`.
- Command thay cấu hình phải kiểm tra miền giá trị trước khi lưu flash/NVS.
- Node gửi ACK command gồm `node_id`, `packet_id`, `status`, `CRC`.
- Gateway retry có giới hạn và công bố trạng thái `COMMAND_SUCCESS` hoặc
  `COMMAND_FAILED`.
- Command và telemetry dùng chung radio phải được điều phối bằng một radio
  state machine trong superloop; không TX command khi radio đang chờ ACK hoặc
  xử lý packet dữ liệu.

## 9. Mô hình phân tích địa kỹ thuật

### 9.1 Áp lực nước lỗ rỗng

```text
u = u_max × max(0, (H_soil - H_c) / (H_sat - H_c))
```

- `H_soil`, `H_c`, `H_sat` phải cùng cách biểu diễn: cùng là % hoặc cùng 0–1.
- Không tự chặn phía trên nếu công thức thiết kế chỉ yêu cầu `max(0, ...)`.
- Nếu muốn giới hạn `u <= u_max`, phải cập nhật đặc tả và nêu lý do vật lý.
- `H_c=65%`, `H_sat=95%`, `u_max=12 kPa` hiện là giả định thiết kế, chưa được
  fit bằng phép đo áp lực nước lỗ rỗng.

Nguồn và cách lựa chọn:

- `H_c` phải lấy từ điểm mà áp lực nước lỗ rỗng đo tham chiếu bắt đầu rời khỏi
  vùng nhiễu gần 0 khi tăng độ ẩm.
- `H_sat` phải được xác định tại trạng thái mẫu gần bão hòa bằng phương pháp
  tham chiếu. Chỉ số `H_soil=95%` không đồng nghĩa độ bão hòa vật lý 95%.
- `u_max` phải lấy từ piezometer/cảm biến áp lực nước lỗ rỗng đo đồng thời với
  `H_soil`; không thể suy ra `u_max` từ `ADC_wet` và `ADC_dry`.
- Quy trình fit: giữ cố định loại đất, độ chặt và hình học → tăng nước theo từng
  mức → chờ ổn định → ghi `ADC_filtered`, `H_soil`, `u_reference` → lặp nhiều
  chu kỳ → fit `H_c`, `H_sat`, `u_max` → đánh giá RMSE/MAE.
- Nếu chưa có `u_reference`, kết quả `u` và FS phải mang nhãn
  `MODEL_PROVISIONAL` và chỉ được mô tả là mô phỏng.

### 9.2 Mô hình mái dốc vô hạn

```text
tau     = gamma × z × sin(beta) × cos(beta)
sigma_n = gamma × z × cos²(beta)
sigma_effective = sigma_n - u
tau_f   = c' + sigma_effective × tan(phi')
FS      = tau_f / tau
```

Quy tắc:

- `beta` đổi sang radian khi gọi hàm lượng giác.
- `gamma` dùng kN/m³ và `z` dùng m để kết quả ứng suất là kPa.
- Không tự ép `sigma_effective` âm thành 0 nếu đặc tả chưa yêu cầu.
- Nếu `tau` bằng hoặc quá gần 0, `FS` không hợp lệ; không thay bằng 99.
- `gamma=18 kN/m³`, `z=1 m`, `c'=5 kPa`, `phi'=28°` hiện là bộ tham số mô
  phỏng ban đầu, không phải kết quả thí nghiệm của mẫu đất.

Nguồn và cách lựa chọn:

- `gamma`: đo density/unit weight trên mẫu đất đại diện rồi tính
  `gamma = rho × g`. Có thể tham khảo ASTM D7263. Giá trị 18 kN/m³ hiện là giả
  định, không phải kết quả mẫu đất DA2.
- `z`: lấy từ khảo sát địa tầng, hố đào, khoan, mặt phân lớp hoặc đo hình học
  mô hình. MPU6050 không đo được chiều sâu lớp trượt. Nếu chưa chắc chắn, phải
  tính sensitivity với nhiều giá trị `z`.
- `c'` và `phi'`: lấy từ direct shear drained hoặc triaxial phù hợp trên đúng
  mẫu đất. ASTM D3080/D3080M dùng direct shear consolidated drained. Fit:

```text
tau_failure = c' + sigma_effective × tan(phi')
c' = giao điểm trục tau
phi' = atan(độ dốc đường fit)
```

- ASTM D4767 có thể tham khảo cho triaxial đất dính; lựa chọn điều kiện và diễn
  giải tham số phải do người có chuyên môn địa kỹ thuật quyết định.
- `beta`: lấy từ góc hình học mái dốc hoặc MPU6050 đã hiệu chuẩn trục và gá cứng.
  Phải phân biệt góc dốc nền với phần thay đổi góc của khối cảm biến.

### 9.3 Chỉ số động học

```text
DI = w1 × abs(beta_dot / beta_dot_crit)
   + w2 × (A_rms / A_crit)
```

- Công bố `w1`, `w2`, `beta_dot_crit`, `A_crit` trong cấu hình.
- `beta_dot` và `beta_dot_crit` phải cùng đơn vị.
- Nếu `A_rms` không âm theo định nghĩa, không cần lấy trị tuyệt đối.
- `beta_dot_crit=3 độ/giờ`, `A_crit=0.08 g`, `w1=0.55`, `w2=0.45` là giả
  định thiết kế hiện tại; phải được kiểm chứng bằng dữ liệu có nhãn.

Nguồn và cách lựa chọn:

- `beta_dot_crit`: đo noise/drift khi đứng yên và đo các thử nghiệm ổn định,
  bắt đầu dịch chuyển, nguy hiểm; chọn ngưỡng theo false alarm và missed
  detection. Giá trị 3 độ/giờ hiện chưa có dữ liệu chứng minh.
- `A_crit`: đo `A_rms` nền sau khi gắn MPU6050, các nguồn rung môi trường bình
  thường và các thử nghiệm dịch chuyển có nhãn. Datasheet MPU6050 không cung
  cấp ngưỡng rung gây sạt lở. Giá trị 0.08 g hiện là giả định.
- `w1`, `w2` phải không âm và có tổng bằng 1. Có thể chọn bằng chuyên gia khi
  chưa có dữ liệu, nhưng tốt hơn là tối ưu trên tập dữ liệu có nhãn và kiểm tra
  sensitivity. Cặp 0.55/0.45 chỉ biểu thị ưu tiên nhẹ cho tốc độ nghiêng.

### 9.4 Độ giãn tương đối quy ước

```text
epsilon_star = 1 / FS
```

Nếu `FS` không hợp lệ hoặc bằng 0, `epsilon_star` cũng không hợp lệ.

`epsilon_star` không phải strain vật lý đo bằng strain gauge. Nếu các tham số
tạo FS còn là giả định thì `epsilon_star` cũng chỉ là chỉ số mô phỏng.

### 9.5 Trạng thái nguồn tham số

| Loại nguồn | Ý nghĩa |
| --- | --- |
| `MEASURED_SENSOR` | Đã đo/hiệu chuẩn trên cảm biến đang dùng |
| `LAB_TEST` | Kết quả thí nghiệm mẫu đất |
| `SITE_SURVEY` | Đo hình học hoặc khảo sát hiện trường/mô hình |
| `DATASHEET` | Lấy từ datasheet thiết bị |
| `LITERATURE` | Khoảng tham khảo từ tiêu chuẩn/tài liệu |
| `EMPIRICAL_FIT` | Fit từ dữ liệu thực nghiệm |
| `DESIGN_ASSUMPTION` | Giả định tạm để mô phỏng |

Mỗi tham số phải có giá trị, đơn vị, loại nguồn, phương pháp, mẫu đất, ngày đo,
thiết bị, số lần lặp và sai số. Hiện chỉ `ADC_wet=1200` và `ADC_dry=3400` được
xác nhận là `MEASURED_SENSOR` theo thông tin người dùng cung cấp.

Tài liệu phương pháp tham khảo:

- ASTM D7263-21, xác định density và unit weight của mẫu đất:
  <https://store.astm.org/d7263-21.html>
- ASTM D3080/D3080M-23, direct shear consolidated drained:
  <https://store.astm.org/d3080_d3080m-23.html>
- ASTM D4767-11(2020), triaxial compression cho đất dính:
  <https://store.astm.org/d4767-11r20.html>
- TDK InvenSense MPU-6050, dùng làm nguồn đặc tính cảm biến, không phải nguồn
  ngưỡng sạt lở:
  <https://invensense.tdk.com/en-us/products/motion-tracking/6-axis/mpu-6050/>
- `Code.pdf` và `New Section 1.pdf` giải thích flow/công thức, nhưng không phải
  hồ sơ thí nghiệm xác nhận bộ tham số đất.

### 9.6 Phân loại cảnh báo

**DANGER** khi có ít nhất một điều kiện:

```text
FS <= 1.0
DI >= 1.0
epsilon_star >= 1.0
```

**WARNING** khi không DANGER và có ít nhất một điều kiện:

```text
1.0 < FS <= 1.3
0.5 <= DI < 1.0
0.77 <= epsilon_star < 1.0
```

**NORMAL** chỉ khi đồng thời:

```text
FS > 1.3
DI < 0.5
epsilon_star < 0.77
```

Sensor error, analysis invalid và battery state là các trạng thái ưu tiên riêng;
không được báo `NORMAL` khi dữ liệu không đủ để phân loại.

## 10. Adaptive Duty Cycle

Thứ tự quyết định bắt buộc:

1. Kiểm tra `error_flag`.
2. Kiểm tra pin rất yếu `< 3.3 V`.
3. Kiểm tra pin yếu `< 3.5 V`.
4. Tính/đọc `FS`, `DI`, `epsilon_star`.
5. Phân loại DANGER/WARNING/NORMAL.
6. Giới hạn `sleep_duration` trong 5–30 phút.
7. Lưu sleep duration vào RTC memory.
8. Cấu hình timer wake-up và vào deep sleep.

Bảng thời gian đã được flowchart chốt:

| Trạng thái | Sleep duration |
| --- | ---: |
| Sensor error | 10 phút |
| DANGER | 5 phút |
| WARNING | 20 phút |
| NORMAL | 30 phút |

Flowchart chưa chốt sleep duration cụ thể cho `BATTERY_CRITICAL` và
`BATTERY_LOW`. Không được tự chọn giá trị trong implementation cuối cùng.
Phải tạo quyết định thiết kế riêng trước khi code. Giá trị tạm thời nếu dùng để
thử nghiệm phải được đánh dấu rõ là provisional.

Khi pin `< 3.5 V`, trạng thái thiết kế yêu cầu khóa OTA. Chỉ được báo
`ota_locked=true` khi cơ chế OTA thực sự kiểm tra cờ này; nếu chưa có OTA, ghi
`ota_supported=false` thay vì tạo cảm giác đã khóa chức năng.

## 11. Gateway, ThingsBoard và lưu dữ liệu

### 11.1 Trình tự xử lý

Gateway phải thực hiện:

```text
Nhận packet → kiểm tra CRC/header → kiểm tra ID → chống trùng
→ lưu raw data → phân tích → tạo trạng thái → gửi ACK
→ publish telemetry hoặc lưu offline buffer
```

ACK nên được ưu tiên thời gian hơn MQTT để node không retry không cần thiết.

### 11.2 Telemetry bắt buộc

ThingsBoard phải nhận cả dữ liệu gốc, dữ liệu phân tích và trạng thái:

```text
gateway_id, node_id, packet_id, timestamp_ms,
soil_adc_filtered, h_soil,
beta_deg, beta_dot_deg_per_hour, a_rms_g, pitch_deg, roll_deg,
v_bat, lora_rssi, error_flag, error_text,
u_kpa, tau_kpa, sigma_n_kpa, sigma_effective_kpa, tau_f_kpa,
fs, di, epsilon_star, analysis_valid,
alert_level, risk_status, warning_message,
duty_cycle_mode, sleep_duration_sec, battery_status
```

### 11.3 Offline buffer

- MQTT offline không được đồng nghĩa với mất mẫu.
- Telemetry chưa publish phải vào local buffer bền vững nếu yêu cầu hệ thống là
  không mất dữ liệu qua reset/mất điện.
- Khi mạng trở lại, gửi theo thứ tự thời gian và giữ nguyên `packet_id`.
- Phải phân biệt thời gian đo và thời gian upload lại.
- Không ACK `data persisted` nếu dữ liệu chưa được lưu an toàn theo cam kết.

### 11.4 Serial log

Mỗi packet mới phải có ba nhóm log:

```text
[RAW]       dữ liệu node và RSSI
[ANALYSIS]  u, tau, sigma_n, sigma_effective, tau_f, FS, DI, epsilon_star
[STATE]     alert, risk, battery, duty cycle, sleep duration, error
```

Log mạng chỉ cần trạng thái chuyển tiếp có ý nghĩa: Wi-Fi connected/failed,
MQTT connected/failed, publish OK/FAILED. Không in danh sách Wi-Fi định kỳ.

## 12. Quy tắc kiến trúc Superloop hiện tại

### 12.1 Mô hình thực thi

- Firmware hiện tại dùng `setup()` để khởi tạo và `loop()` để điều phối.
- Mọi chức năng ứng dụng chạy tuần tự; không có task ứng dụng chạy đồng thời.
- Mỗi vòng `loop()` phải thực hiện các hàm dịch vụ ngắn, có trạng thái và quay
  lại nhanh để các chức năng khác tiếp tục được phục vụ.
- Các khối chức năng được tách thành hàm/module để sau này có thể chuyển sang
  FreeRTOS mà không đổi công thức, packet hoặc contract dữ liệu.

Phân chia trách nhiệm logic trong superloop:

- `serviceSoil()`: đọc, retry, median filter và tạo SoilData.
- `serviceMpu()`: lấy mẫu định kỳ, low-pass, tính góc và rung.
- `buildSensorSnapshot()`: hợp nhất dữ liệu, validation và tạo packet.
- `serviceNodeRadio()`: TX, chờ ACK, retry và command RX.
- `serviceGatewayRadio()`: RX/TX LoRa và timestamp nhận.
- `processGatewayPacket()`: parse, validation, duplicate và analysis.
- `serviceNetwork()`: Wi-Fi, MQTT, publish và offline retry.
- `serviceSerial()`: log và command debug.

Tên hàm cụ thể có thể khác, nhưng trách nhiệm không được trộn lẫn tùy tiện.

### 12.2 State machine và ownership

- LoRa chỉ được điều khiển tại một điểm trong superloop hoặc qua một API radio
  duy nhất.
- Radio state phải phân biệt tối thiểu: `IDLE`, `RX`, `TX_DATA`, `WAIT_ACK`,
  `TX_ACK`, `TX_COMMAND`.
- MQTT chỉ được gọi trong khối network service.
- Dùng struct snapshot hoặc buffer có ownership rõ ràng giữa các bước xử lý.
- Buffer đầy phải tạo metric/log; không được drop packet im lặng.
- Không cần mutex trong superloop vì không có truy cập đồng thời. Nếu code bắt
  đầu có callback/ISR sửa chung state, phải thiết kế vùng bảo vệ riêng.

### 12.3 Timing không chặn

- Không dùng vòng chờ dài làm dừng toàn bộ `loop()`.
- Dùng `millis()` và state machine để quản lý sampling, timeout ACK, retry,
  Wi-Fi reconnect và MQTT reconnect.
- Chỉ cho phép delay ngắn tại bước phần cứng bắt buộc; mọi delay phải được ghi
  rõ tác động tới khả năng nhận LoRa và phục vụ mạng.
- Mỗi service function phải có timeout hữu hạn.
- ACK được ưu tiên trước tác vụ MQTT có thể chậm.
- Deep sleep chỉ bắt đầu sau khi log, ACK cần thiết và dữ liệu quan trọng đã
  được flush hoặc lưu.

### 12.4 Kế hoạch chuyển sang FreeRTOS sau này

FreeRTOS **chưa thuộc phạm vi triển khai hiện tại**. Chỉ bắt đầu migration khi:

1. Superloop đã chạy đúng toàn bộ flow và có test biên.
2. Timing thực tế chứng minh superloop không đáp ứng sampling/radio/network.
3. Contract dữ liệu giữa Soil, MPU, radio, analysis và network đã ổn định.
4. Có kế hoạch stack size, priority, queue depth và ownership peripheral.

Khi migration, có thể tách thành Soil task, MPU task, radio task, processing
task và network task. Việc chuyển đổi phải giữ nguyên công thức, telemetry key,
packet format, ACK/retry và Adaptive Duty Cycle. Không được thêm task chỉ để
đổi kiến trúc khi chưa có yêu cầu timing cụ thể.

## 13. Cấu hình và bảo mật

- SSID, mật khẩu Wi-Fi và ThingsBoard token không được commit vào tài liệu hoặc
  source dùng chung.
- Tách secrets khỏi cấu hình mô hình và cấu hình chân GPIO.
- Token đã lộ phải được rotate.
- Mọi ngưỡng phải có đơn vị trong tên hoặc comment.
- Tham số hiệu chuẩn phải có nguồn, ngày đo và loại đất/cảm biến.

## 14. Tiêu chí kiểm thử trước khi chấp nhận

### 14.1 Soil

- ADC hợp lệ, ADC lỗi liên tiếp và phục hồi sau retry.
- Median filter với nhiễu đột biến.
- `ADC_dry == ADC_wet` không gây chia 0.
- Clamp độ ẩm đúng 0–100%.

### 14.2 MPU

- WHO_AM_I sai, I2C timeout và mẫu thiếu.
- Mặt phẳng ngang cho beta gần 0.
- Góc nghiêng biết trước cho pitch/roll/beta.
- `dt <= 0`, chu kỳ đầu và sau deep sleep.
- Rung tĩnh không tạo RMS lớn bất thường.

### 14.3 LoRa

- CRC sai, header sai, node ID sai và packet trùng.
- Mất ACK, ACK sai packet ID và retry đạt giới hạn.
- Gateway khởi động trong lúc node retry.
- Command và telemetry tranh chấp radio.

### 14.4 Analysis và duty cycle

- Kiểm tra biên FS: 1.0 và 1.3.
- Kiểm tra biên DI: 0.5 và 1.0.
- Kiểm tra biên epsilon: 0.77 và 1.0.
- Sensor error luôn đi nhánh 10 phút.
- Sleep duration luôn trong 5–30 phút.
- Giá trị analysis invalid không bị thay bằng số giả.

### 14.5 ThingsBoard

- Mỗi packet ID chỉ tạo một mẫu logic.
- Telemetry có đủ raw, analysis và state keys.
- Mất Wi-Fi rồi phục hồi không làm mất hoặc đổi timestamp dữ liệu.
- Dashboard phân biệt sensor error, cloud offline và landslide danger.

## 15. Các quyết định còn mở

Các mục sau phải được chốt bằng tài liệu trước khi triển khai hoàn chỉnh:

1. Sleep duration chính thức cho pin `< 3.3 V` và `< 3.5 V`.
2. Đơn vị validation chính thức của `beta_dot` (`°/s` hay `°/h`).
3. Cơ chế OTA thực tế và ý nghĩa kỹ thuật của `ota_locked`.
4. Loại local storage: RTC RAM, RAM ring buffer, NVS, LittleFS hay thẻ nhớ.
5. Dung lượng và chính sách tràn offline buffer.
6. Đồng bộ UTC cho node và timestamp khi deep sleep.
7. Lưu database nằm ở gateway, ThingsBoard hay backend riêng.
8. Chính sách command khi node đang deep sleep.
9. Cách hỗ trợ nhiều hơn 100 node và thời hạn lưu duplicate history.
10. Ngưỡng ADC, pin và địa kỹ thuật sau hiệu chuẩn thực nghiệm.
