# 2.5 Cơ sở lựa chọn bộ tham số cho đất đỏ bazan

## 2.5.1 Mục đích và phạm vi áp dụng

Tài liệu này chốt một bộ tham số cụ thể để sau này triển khai mô hình `FS`,
`DI` và `epsilon_star` trong firmware.

Loại đất đại diện được chọn:

> **Đất đỏ bazan phong hóa pha sét, lớp trượt nông**, đại diện cho mô hình đất
> phổ biến tại khu vực Tây Nguyên Việt Nam.

Bộ số dưới đây phục vụ đồ án, mô phỏng, kiểm thử thuật toán và dashboard. Các
giá trị được lựa chọn theo hướng bảo thủ để tạo một profile nhất quán cho phần
mềm, nhưng không thay thế khảo sát địa kỹ thuật hoặc thí nghiệm mẫu đất tại
hiện trường.

## 2.5.2 Nguyên tắc và nguồn lựa chọn

Các tham số được chia thành ba nhóm:

| Nhóm | Ý nghĩa |
| --- | --- |
| `MEASURED` | Đã đo hoặc hiệu chuẩn bằng phần cứng đang sử dụng |
| `LITERATURE_METHOD` | Cách xác định dựa trên phương pháp/tiêu chuẩn chuyên ngành |
| `ENGINEERING_ASSUMPTION` | Số cụ thể được chọn bảo thủ để chạy mô hình khi chưa có thí nghiệm |

Không tìm thấy nguồn công khai đáng tin cậy cung cấp đồng thời toàn bộ
`gamma`, `z`, `c'`, `phi'`, `H_c`, `H_sat`, `u_max`, `beta_dot_crit` và
`A_crit` cho đúng mẫu đất của đề tài. Vì vậy các số không đo được phải được ghi
rõ là giả định kỹ thuật, không được trình bày như kết quả thực nghiệm.

## 2.5.3 Bộ tham số sử dụng trong mô hình

### 2.5.3.1 Bảng tổng hợp tham số

**Bảng 2.1. Bộ tham số giả định cho profile đất đỏ bazan phong hóa pha sét**

| Tham số | Giá trị chọn | Đơn vị | Trạng thái | Lý do chọn ngắn gọn |
| --- | ---: | --- | --- | --- |
| `ADC_wet` | 1200 | count | `MEASURED` | Kết quả hiệu chuẩn người dùng cung cấp |
| `ADC_dry` | 3400 | count | `MEASURED` | Kết quả hiệu chuẩn người dùng cung cấp |
| `H_c` | 65 | % | `ENGINEERING_ASSUMPTION` | Chỉ bắt đầu tăng `u` khi đất đã khá ẩm |
| `H_sat` | 95 | % | `ENGINEERING_ASSUMPTION` | Đại diện vùng chỉ số cảm biến gần wet-point |
| `u_max` | 10 | kPa | `ENGINEERING_ASSUMPTION` | Bảo thủ nhưng không vượt tải pháp tuyến trong phần lớn miền góc thiết kế |
| `gamma` | 18 | kN/m³ | `ENGINEERING_ASSUMPTION` | Giá trị đại diện cho đất khoáng ẩm/đất pha sét |
| `z` | 1.0 | m | `ENGINEERING_ASSUMPTION` | Kịch bản lớp trượt nông của mô hình mái dốc vô hạn |
| `c'` | 5 | kPa | `ENGINEERING_ASSUMPTION` | Chọn thấp để tránh đánh giá quá cao lực dính |
| `phi'` | 28 | độ | `ENGINEERING_ASSUMPTION` | Giá trị trung bình-bảo thủ cho đất pha sét phong hóa |
| `beta_dot_crit` | 2.0 | độ/giờ | `ENGINEERING_ASSUMPTION` | PDF nội bộ xem 2°/h là mức nguy hiểm minh họa |
| `A_crit` | 0.05 | g | `ENGINEERING_ASSUMPTION` | Cao hơn rung nền kỳ vọng nhưng đủ nhạy với rung bất thường |
| `w1` | 0.70 | Không đơn vị | `ENGINEERING_ASSUMPTION` | Ưu tiên biến dạng góc vì phản ánh dịch chuyển trực tiếp hơn |
| `w2` | 0.30 | Không đơn vị | `ENGINEERING_ASSUMPTION` | Giảm ảnh hưởng rung môi trường lên cảnh báo |
| `beta_dot_sanity_max` | 360 | độ/giờ | `ENGINEERING_ASSUMPTION` | Tương đương 0.1°/s, dùng phát hiện mẫu nhảy bất thường |
| `A_rms_sanity_max` | 1.0 | g | `ENGINEERING_ASSUMPTION` | Một nửa thang ±2 g đang cấu hình |

### 2.5.3.2 Bộ hằng số đề xuất cho phần mềm

```text
SOIL_ADC_WET = 1200
SOIL_ADC_DRY = 3400

MOISTURE_CRITICAL_START_PERCENT = 65.0
MOISTURE_NEAR_SATURATION_PERCENT = 95.0
PORE_PRESSURE_MAX_KPA = 10.0

SOIL_UNIT_WEIGHT_KN_M3 = 18.0
SLIP_LAYER_DEPTH_M = 1.0
SOIL_EFFECTIVE_COHESION_KPA = 5.0
SOIL_EFFECTIVE_FRICTION_ANGLE_DEG = 28.0

BETA_DOT_CRIT_DEG_PER_HOUR = 2.0
A_RMS_CRIT_G = 0.05
DI_WEIGHT_BETA_DOT = 0.70
DI_WEIGHT_VIBRATION = 0.30

BETA_DOT_SANITY_MAX_DEG_PER_HOUR = 360.0
A_RMS_SANITY_MAX_G = 1.0
```

## 2.5.4 Phân tích các tham số tạo hệ số an toàn FS

### 2.5.4.1 Ngưỡng bắt đầu ảnh hưởng của nước `H_c = 65%`

#### Khái niệm

`H_c` là chỉ số ẩm bắt đầu làm mô hình áp lực nước lỗ rỗng tăng khỏi 0.

#### Nguồn xác định

Về nguyên tắc, phải đo đồng thời:

```text
H_soil từ cảm biến điện dung
u_reference từ piezometer/cảm biến áp lực nước lỗ rỗng
```

Sau đó chọn `H_c` tại điểm `u_reference` bắt đầu vượt vùng nhiễu nền.

#### Cơ sở lựa chọn

- Đây là giả định chuyển tiếp giữa vùng ẩm thông thường và vùng đất khá ướt.
- Nó giữ `u=0` trong phần lớn vùng khô-trung bình, tránh làm FS giảm quá sớm.
- Từ 65% trở lên, mô hình bắt đầu phản ánh tác động bất lợi của nước.
- Giá trị này không có nghĩa mọi đất đỏ bazan bắt đầu sinh áp lực nước tại 65%.

#### Trạng thái tham số

```text
ENGINEERING_ASSUMPTION
```

### 2.5.4.2 Ngưỡng gần bão hòa `H_sat = 95%`

#### Khái niệm

`H_sat` là chỉ số ẩm cảm biến được quy ước tương ứng trạng thái gần bão hòa.

#### Cơ sở lựa chọn

- Điểm wet đã được hiệu chuẩn tại ADC 1200 và ánh xạ thành 100%.
- Chọn 95% thay vì 100% để mô hình đạt gần `u_max` trước khi ADC chạm đúng
  wet-point, giảm phụ thuộc vào nhiễu tại biên hiệu chuẩn.
- Đây là chỉ số cảm biến tương đối, không phải độ bão hòa vật lý `Sr=95%`.

#### Trạng thái tham số

```text
ENGINEERING_ASSUMPTION
```

### 2.5.4.3 Áp lực nước lỗ rỗng cực đại giả định `u_max = 10 kPa`

#### Khái niệm

`u_max` là áp lực nước lỗ rỗng giả định khi `H_soil` đạt `H_sat`.

#### Cơ sở lựa chọn

Với bộ tham số:

```text
gamma = 18 kN/m³
z = 1 m
```

tải phủ đặc trưng là:

```text
gamma × z = 18 kPa
```

Trong miền góc khoảng 0–45°, ứng suất pháp tuyến tổng thường nằm khoảng
9–18 kPa. Chọn `u_max=10 kPa` đủ lớn để giảm mạnh ứng suất hữu hiệu khi đất
gần bão hòa nhưng không làm `sigma_effective` âm trong toàn bộ miền vận hành
thông thường.

Ví dụ tại `beta=30°`:

```text
sigma_n = 18 × cos²(30°) = 13.5 kPa
sigma_effective_at_umax = 13.5 - 10 = 3.5 kPa
```

#### Phương pháp xác định chính xác

Phải fit bằng dữ liệu `H_soil` và `u_reference` đo đồng thời. Không thể suy ra
`u_max` chỉ từ ADC.

#### Trạng thái tham số

```text
ENGINEERING_ASSUMPTION
```

### 2.5.4.4 Dung trọng đất `gamma = 18 kN/m³`

#### Khái niệm

`gamma` là dung trọng đất, biểu diễn trọng lượng trên một đơn vị thể tích.

#### Nguồn xác định

- Đo density/unit weight trên mẫu đất đại diện.
- Có thể dùng phương pháp ASTM D7263.
- Tính gần đúng `gamma = rho × g`.

#### Cơ sở lựa chọn

- Đây là giá trị đại diện thường dùng cho đất khoáng ẩm hoặc đất pha sét trong
  tính toán sơ bộ.
- Giá trị không quá thấp để đánh giá thiếu tải trọng bản thân.
- Giá trị không quá cao như đất rất chặt hoặc vật liệu đá.
- Không có dữ liệu chứng minh đây là dung trọng đúng của mẫu đất đang dùng.

#### Khối lượng thể tích tương đương

```text
rho = gamma / g
    = 18 000 / 9.81
    ≈ 1835 kg/m³
```

#### Trạng thái tham số

```text
ENGINEERING_ASSUMPTION dựa trên khoảng kỹ thuật phổ biến
```

### 2.5.4.5 Chiều dày lớp trượt `z = 1.0 m`

#### Khái niệm

`z` là chiều dày lớp đất phía trên mặt trượt giả định trong mô hình mái dốc vô
hạn.

#### Cơ sở lựa chọn

- Hệ thống hướng đến sạt lở nông, không phải mặt trượt sâu hàng chục mét.
- `1 m` tạo kịch bản dễ diễn giải vì `gamma × z = 18 kPa`.
- Đây là giả định hình học, không phải thuộc tính cố hữu của đất đỏ bazan.

Nếu mô hình vật lý chỉ dày 0.2 m, phải dùng `z=0.2 m`, không được giữ 1 m.

#### Nguồn xác định

- Đo trực tiếp hình học mô hình.
- Khảo sát hố đào, khoan hoặc lớp phân cách nếu dùng ngoài hiện trường.

#### Trạng thái tham số

```text
ENGINEERING_ASSUMPTION
```

### 2.5.4.6 Lực dính hiệu dụng `c' = 5 kPa`

#### Khái niệm

`c'` là lực dính hiệu dụng trong mô hình Mohr-Coulomb.

#### Nguồn xác định

- Direct shear consolidated drained, tham khảo ASTM D3080/D3080M.
- Hoặc triaxial phù hợp với loại đất và điều kiện thoát nước.
- Fit nhiều điểm phá hoại:

```text
tau_failure = c' + sigma_effective × tan(phi')
```

#### Cơ sở lựa chọn

- Đất đỏ bazan phong hóa pha sét có thể biểu hiện lực dính, nhưng lực dính thay
  đổi mạnh theo độ ẩm, cấu trúc và mức phong hóa.
- Chọn 5 kPa theo hướng bảo thủ để không đánh giá quá cao khả năng chống trượt.
- Nếu chọn `c'` lớn khi chưa đo, FS có thể bị nâng giả tạo.

#### Trạng thái tham số

```text
ENGINEERING_ASSUMPTION bảo thủ
```

### 2.5.4.7 Góc ma sát trong hiệu dụng `phi' = 28°`

#### Khái niệm

`phi'` là góc ma sát trong hiệu dụng.

#### Nguồn xác định

Từ độ dốc của đường phá hoại Mohr-Coulomb:

```text
slope = tan(phi')
phi' = atan(slope)
```

#### Cơ sở lựa chọn

- Đây là mức trung bình-bảo thủ cho đất pha sét/đất phong hóa, thấp hơn nhóm
  cát chặt có ma sát lớn.
- Chọn thấp vừa phải giúp tránh FS quá lạc quan khi chưa có direct shear.
- Không có nguồn công khai nào chứng minh 28° là đúng cho mẫu đất DA2.

#### Trạng thái tham số

```text
ENGINEERING_ASSUMPTION
```

### 2.5.4.8 Góc dốc đầu vào `beta`

`beta` không phải hằng số lấy từ bên ngoài. Nó là giá trị đo/suy ra từ MPU6050
hoặc góc hình học mái dốc.

Quy tắc sử dụng:

- Dùng `beta_deg = sqrt(pitch_deg² + roll_deg²)` nếu cảm biến được gá đúng.
- Hiệu chuẩn tư thế 0° sau khi gắn cảm biến.
- Miền sanity hiện tại: `0–60°`.
- Không clamp góc lớn về 60°; đánh dấu `OUT_OF_MODEL_RANGE`.

## 2.5.5 Kiểm tra tính hợp lý của bộ tham số FS

### 2.5.5.1 Trường hợp đất tương đối khô

Giả sử:

```text
H_soil = 50%
beta = 20°
```

Do `H_soil < H_c`:

```text
u = 0 kPa
tau = 18 × sin(20°) × cos(20°) ≈ 5.79 kPa
sigma_n = 18 × cos²(20°) ≈ 15.89 kPa
tau_f = 5 + 15.89 × tan(28°) ≈ 13.45 kPa
FS ≈ 13.45 / 5.79 ≈ 2.32
```

Kết quả: trạng thái cơ học ổn định theo ngưỡng FS.

### 2.5.5.2 Trường hợp độ ẩm cao

Giả sử:

```text
H_soil = 85%
beta = 30°
```

Tính:

```text
u = 10 × (85 - 65) / (95 - 65)
  ≈ 6.67 kPa

tau = 18 × sin(30°) × cos(30°)
    ≈ 7.79 kPa

sigma_n = 18 × cos²(30°)
        = 13.50 kPa

sigma_effective = 13.50 - 6.67
                ≈ 6.83 kPa

tau_f = 5 + 6.83 × tan(28°)
      ≈ 8.63 kPa

FS = 8.63 / 7.79
   ≈ 1.11
```

Kết quả: nằm trong vùng WARNING, gần DANGER. Điều này phù hợp với mục tiêu mô
hình: đất ẩm cao và dốc hơn phải làm FS giảm đáng kể.

### 2.5.5.3 Trường hợp gần bão hòa và góc dốc lớn

Giả sử:

```text
H_soil = 95%
beta = 35°
```

Tính gần đúng:

```text
u = 10 kPa
tau ≈ 8.46 kPa
sigma_n ≈ 12.08 kPa
sigma_effective ≈ 2.08 kPa
tau_f ≈ 6.11 kPa
FS ≈ 0.72
```

Kết quả: DANGER. Bộ tham số tạo được xu hướng hợp lý giữa ba kịch bản khô,
ẩm cao và gần bão hòa.

## 2.5.6 Phân tích các tham số tạo chỉ số động học DI

### 2.5.6.1 Tốc độ thay đổi góc `beta_dot`

`beta_dot` là dữ liệu dẫn xuất từ hai lần đo góc:

```text
beta_dot = abs(beta_current - beta_previous) / delta_time_hours
```

Nguồn của nó là MPU6050 và timestamp, không phải một hằng số lấy trên mạng.

Quy tắc:

- Dùng độ/giờ trong toàn hệ thống.
- Chu kỳ đầu chưa có `beta_previous` phải đánh dấu chưa hợp lệ hoặc dùng state
  khởi tạo rõ ràng.
- Sanity maximum chọn `360°/h`; vượt mức này xem là mẫu nhảy bất thường.

### 2.5.6.2 Ngưỡng tốc độ thay đổi góc `beta_dot_crit = 2°/h`

#### Khái niệm

Ngưỡng chuẩn hóa tốc độ thay đổi góc trong DI. Khi `beta_dot=2°/h`, thành phần
nghiêng chuẩn hóa bằng 1.

#### Cơ sở lựa chọn

- `New Section 1.pdf` dùng ví dụ 0.01°/h gần ổn định, 0.5°/h bắt đầu đáng chú ý
  và 2°/h nguy hiểm.
- Chọn 2°/h bám trực tiếp ví dụ định tính đó.
- Đây vẫn là giả định thiết kế, không phải ngưỡng sạt lở phổ quát.

#### Trạng thái tham số

```text
ENGINEERING_ASSUMPTION dựa trên tài liệu nội bộ
```

### 2.5.6.3 Chỉ số rung động `A_rms`

`A_rms` được tính từ gia tốc MPU6050 sau khi tách thành phần low-pass:

```text
Ax_vib = Ax_raw - Ax_filtered
Ay_vib = Ay_raw - Ay_filtered
Az_vib = Az_raw - Az_filtered

A_rms = sqrt(sum(Ax_vib² + Ay_vib² + Az_vib²) / sample_count)
```

Đây là dữ liệu đo/dẫn xuất, không phải hằng số bên ngoài.

Miền sanity:

```text
0 <= A_rms <= 1.0 g
```

### 2.5.6.4 Ngưỡng rung động `A_crit = 0.05 g`

#### Cơ sở lựa chọn

- Firmware MPU6050 dùng thang ±2 g.
- `0.05 g` bằng 2.5% full-scale, cao hơn rung nhiễu nhỏ kỳ vọng nhưng vẫn đủ
  nhạy để làm thành phần DI tăng khi có rung rõ rệt.
- Giá trị này thấp hơn giả định cũ 0.08 g nên thận trọng hơn với rung bất thường.
- Datasheet MPU6050 không quy định ngưỡng sạt lở; 0.05 g là quyết định kỹ thuật.

#### Trạng thái tham số

```text
ENGINEERING_ASSUMPTION
```

### 2.5.6.5 Trọng số `w1 = 0.70`, `w2 = 0.30`

#### Cơ sở lựa chọn

- Góc thay đổi theo thời gian phản ánh biến dạng hình học trực tiếp của khối đất.
- Rung dễ chịu ảnh hưởng từ bước chân, va chạm, giao thông và rung thiết bị.
- Do đó ưu tiên beta-dot 70%, rung 30% để giảm báo động giả do môi trường.
- Hai trọng số không âm và có tổng bằng 1.

#### Trạng thái tham số

```text
ENGINEERING_ASSUMPTION
```

### 2.5.6.6 Ví dụ kiểm tra chỉ số DI

#### Trường hợp dao động nhỏ

```text
beta_dot = 0.2°/h
A_rms = 0.005 g

DI = 0.70 × (0.2 / 2.0) + 0.30 × (0.005 / 0.05)
   = 0.10
```

Kết quả: NORMAL theo DI.

#### Trường hợp cảnh báo

```text
beta_dot = 1.0°/h
A_rms = 0.025 g

DI = 0.70 × 0.5 + 0.30 × 0.5
   = 0.50
```

Kết quả: bắt đầu WARNING.

#### Trường hợp nguy hiểm

```text
beta_dot = 2.0°/h
A_rms = 0.05 g

DI = 0.70 × 1 + 0.30 × 1
   = 1.00
```

Kết quả: DANGER.

## 2.5.7 Độ giãn tương đối quy ước `epsilon_star`

```text
epsilon_star = 1 / FS
```

Không có tham số riêng để lựa chọn. Nó phụ thuộc hoàn toàn vào FS.

Ví dụ:

| FS | epsilon_star | Diễn giải |
| ---: | ---: | --- |
| 2.0 | 0.50 | Ổn định theo chỉ số quy ước |
| 1.3 | 0.77 | Biên WARNING |
| 1.0 | 1.00 | Biên DANGER |
| 0.8 | 1.25 | Nguy hiểm |

`epsilon_star` không phải strain thật.

## 2.5.8 Quy tắc sử dụng profile trong phần mềm

Khi triển khai code:

1. Đưa toàn bộ hằng số vào một struct/config có phiên bản.
2. Gắn metadata `parameter_profile = BASALT_RED_SOIL_V1_PROVISIONAL`.
3. Publish các hằng số quan trọng lên ThingsBoard attributes, không chỉ compile
   cứng mà không truy vết.
4. Telemetry phải có `model_status = PROVISIONAL`.
5. Không dùng profile này để tuyên bố an toàn thực địa.
6. Khi có kết quả thí nghiệm, tạo profile V2; không sửa âm thầm giá trị V1.

Tên profile đề xuất:

```text
BASALT_RED_SOIL_V1_PROVISIONAL
```

## 2.5.9 Bảng giá trị profile cuối cùng

```text
ADC_wet                  = 1200 count
ADC_dry                  = 3400 count
H_c                      = 65 %
H_sat                    = 95 %
u_max                    = 10 kPa
gamma                    = 18 kN/m³
z                        = 1.0 m
c_effective              = 5 kPa
phi_effective            = 28°
beta_dot_crit            = 2.0°/h
A_crit                   = 0.05 g
w1                       = 0.70
w2                       = 0.30
beta_dot_sanity_max      = 360°/h
A_rms_sanity_max         = 1.0 g
```

## 2.5.10 Nguồn tham khảo

- ASTM D7263-21, phương pháp xác định density và unit weight của mẫu đất:
  <https://store.astm.org/d7263-21.html>
- ASTM D3080/D3080M-23, direct shear consolidated drained:
  <https://store.astm.org/d3080_d3080m-23.html>
- ASTM D4767-11(2020), triaxial compression cho đất dính:
  <https://store.astm.org/d4767-11r20.html>
- TDK InvenSense MPU-6050, đặc tính thiết bị và các thang đo gia tốc:
  <https://invensense.tdk.com/en-us/products/motion-tracking/6-axis/mpu-6050/>
- `C:\Users\Public\New Section 1.pdf`, nguồn nội bộ giải thích công thức và ví
  dụ định tính cho beta-dot/DI.

Các tiêu chuẩn trên cung cấp **phương pháp xác định**, không cung cấp trực tiếp
bộ số của mẫu đất DA2. Bộ số trong tài liệu này là profile giả định cụ thể được
chọn để triển khai phần mềm trước khi có thí nghiệm.

## 2.5.11 Kết luận mục

Qua quá trình phân tích, profile `BASALT_RED_SOIL_V1_PROVISIONAL` đã được xây
dựng với một bộ tham số thống nhất cho mô hình FS và DI. Profile cho phép triển
khai và kiểm thử phần mềm theo cùng một cơ sở tính toán. Tuy nhiên, các tham số
địa kỹ thuật vẫn mang tính giả định và phải được thay thế bằng kết quả khảo sát
hoặc thí nghiệm khi hệ thống được áp dụng cho một vị trí cụ thể.
