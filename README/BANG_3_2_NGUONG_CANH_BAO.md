# Bảng 3.2. Ngưỡng các đại lượng và quy tắc tổng hợp mức cảnh báo

## Mục đích

Bảng này quy định các ngưỡng dùng để kiểm tra dữ liệu, đánh giá trạng thái
pin, tính chỉ số nguy cơ và tổng hợp mức cảnh báo của hệ thống giám sát đất.

Các giá trị trong bảng hiện là **ngưỡng thiết kế ban đầu** lấy từ flowchart,
`Code.pdf` và cấu hình mô hình. Chúng chưa thay thế kết quả hiệu chuẩn cảm biến
hoặc thí nghiệm trên loại đất thực tế.

Riêng cảm biến độ ẩm đã có hai điểm hiệu chuẩn do người dùng cung cấp:
`ADC_wet = 1200`, `ADC_dry = 3400`. Nguồn và cách lựa chọn các tham số mô hình
được giải thích trong mục 9 của `README/DESIGN_RULES.md`.

## Bảng 3.2

| STT | Đại lượng | Ký hiệu/key | Đơn vị | Bình thường | Cảnh báo | Nguy hiểm/lỗi | Vai trò trong hệ thống | Trạng thái ngưỡng |
| ---: | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | Giá trị ADC độ ẩm sau lọc | `soil_adc_filtered` | count | `ADC_min < ADC < ADC_max` | Không áp dụng trực tiếp | Ngoài miền hợp lệ quá 3 lần liên tiếp → lỗi cảm biến | Kiểm tra tính hợp lệ trước khi tính độ ẩm | Phải hiệu chuẩn theo cảm biến |
| 2 | Độ ẩm đất tương đối | `h_soil` | % | `0 ≤ H_soil < 65` | `65 ≤ H_soil < 95` | `H_soil ≥ 95` biểu thị gần bão hòa | Đầu vào mô hình áp lực nước lỗ rỗng; không dùng một mình để kết luận sạt lở | 65% và 95% là giả thiết mô hình, cần thí nghiệm |
| 3 | Góc nghiêng tổng | `beta_deg` | độ | `0 ≤ β ≤ 60` theo miền kiểm tra dữ liệu | Chưa chốt ngưỡng cảnh báo độc lập | `β < 0` hoặc `β > 60` → dữ liệu ngoài miền thiết kế | Đầu vào mô hình mái dốc; kết hợp với độ ẩm và chỉ số động học | Miền 0–60° cần xác nhận theo mô hình gá cảm biến |
| 4 | Tốc độ thay đổi góc | `beta_dot_deg_per_hour` | độ/giờ | Nhỏ hơn ngưỡng thực nghiệm | Tiệm cận ngưỡng `β_dot_crit` | `|β_dot| ≥ β_dot_crit` làm thành phần động học đạt mức nguy hiểm tương đối | Thành phần thứ nhất của DI | Giá trị cấu hình ban đầu `β_dot_crit = 3 độ/giờ`; cần kiểm chứng |
| 5 | Rung động RMS | `a_rms_g` | g | Nhỏ hơn ngưỡng thực nghiệm | Tiệm cận ngưỡng `A_crit` | `A_rms ≥ A_crit` làm thành phần rung đạt mức nguy hiểm tương đối | Thành phần thứ hai của DI | Giá trị cấu hình ban đầu `A_crit = 0.08 g`; cần kiểm chứng |
| 6 | Điện áp pin | `v_bat` | V | `V_bat ≥ 3.5` | `3.3 ≤ V_bat < 3.5` → pin yếu | `V_bat < 3.3` → pin rất yếu; ngoài `3.2–4.2 V` có thể coi là dữ liệu bất thường | Ưu tiên cảnh báo nguồn và điều khiển duty cycle | Phụ thuộc loại pin, mạch chia áp và tải thực tế |
| 7 | Áp lực nước lỗ rỗng | `u_kpa` | kPa | Không có ngưỡng độc lập | Tăng theo `H_soil` sau ngưỡng `H_c` | Không kết luận nguy hiểm trực tiếp | Làm giảm ứng suất hữu hiệu và sức kháng cắt | `u_max`, `H_c`, `H_sat` phải hiệu chỉnh thực nghiệm |
| 8 | Ứng suất gây trượt | `tau_kpa` | kPa | Không có ngưỡng độc lập | Không áp dụng | `|τ|` quá gần 0 → không đủ điều kiện tính FS | Mẫu số của hệ số an toàn FS | Tính từ mô hình, không đo trực tiếp |
| 9 | Ứng suất pháp tuyến tổng | `sigma_n_kpa` | kPa | Không có ngưỡng độc lập | Không áp dụng | Kết quả không hữu hạn → phân tích không hợp lệ | Thành phần mô hình mái dốc | Tính từ `γ`, `z`, `β` |
| 10 | Ứng suất hữu hiệu | `sigma_effective_kpa` | kPa | Không có ngưỡng độc lập | Giá trị giảm thể hiện ảnh hưởng bất lợi của nước | Không tự ép giá trị âm về 0; phải giữ để đánh giá mô hình | Đầu vào công thức Mohr–Coulomb | Cần đối chiếu mô hình địa kỹ thuật thực tế |
| 11 | Sức kháng cắt | `tau_f_kpa` | kPa | Không có ngưỡng độc lập | Không áp dụng | Kết quả không hữu hạn → phân tích không hợp lệ | Tử số của FS | Phụ thuộc `c'`, `φ'` và `σ'_n` |
| 12 | Hệ số an toàn | `fs` | Không đơn vị | `FS > 1.3` | `1.0 < FS ≤ 1.3` | `FS ≤ 1.0` | Chỉ số ổn định cơ học chính | Ngưỡng thiết kế theo flowchart |
| 13 | Chỉ số động học | `di` | Không đơn vị | `DI < 0.5` | `0.5 ≤ DI < 1.0` | `DI ≥ 1.0` | Phát hiện chuyển động/rung bất thường | Ngưỡng thiết kế; trọng số và ngưỡng thành phần cần thử nghiệm |
| 14 | Độ giãn tương đối quy ước | `epsilon_star` | Không đơn vị | `ε* < 0.77` | `0.77 ≤ ε* < 1.0` | `ε* ≥ 1.0` | Chỉ số quy ước `ε* = 1/FS` | Phụ thuộc trực tiếp vào tính hợp lệ của FS |
| 15 | Cờ lỗi | `error_flag` | Bitmask | `Error_Flag = 0` | Không áp dụng | `Error_Flag ≠ 0` → dữ liệu lỗi, ưu tiên nhánh sensor/system error | Ngăn phân loại NORMAL khi dữ liệu không đáng tin cậy | Bắt buộc kiểm tra trước các chỉ số nguy cơ |

## Quy tắc tổng hợp mức cảnh báo

Hệ thống không xác định cảnh báo chỉ từ một cảm biến đơn lẻ. Mức cảnh báo được
tổng hợp từ `FS`, `DI`, `epsilon_star`, trạng thái dữ liệu và trạng thái pin.

### Thứ tự ưu tiên

```text
Kiểm tra Error_Flag và analysis_valid
→ Kiểm tra trạng thái pin
→ Kiểm tra điều kiện DANGER
→ Kiểm tra điều kiện WARNING
→ Kiểm tra đồng thời điều kiện NORMAL
→ Nếu không khớp đầy đủ, dùng trạng thái WARNING/CLASSIFICATION_GAP
```

### 1. Dữ liệu lỗi hoặc phân tích không hợp lệ

| Điều kiện | `alert_level` | `risk_status` | Xử lý |
| --- | --- | --- | --- |
| `Error_Flag ≠ 0` | `WARNING` hoặc mức lỗi chuyên biệt | `SENSOR_ERROR`/`RF_ERROR`/`PACKET_ERROR` | Không báo NORMAL; ghi rõ nguồn lỗi; chu kỳ ngủ lỗi cảm biến là 10 phút |
| `analysis_valid = false` | `WARNING` | `ANALYSIS_INVALID` | Không thay FS hoặc ε* bằng số giả; telemetry số không xác định dùng `null` |

### 2. Pin yếu

| Điều kiện | Trạng thái pin | Mức xử lý | Ghi chú |
| --- | --- | --- | --- |
| `V_bat < 3.3 V` | `BATTERY_CRITICAL` | Cảnh báo pin rất yếu, yêu cầu khóa OTA | Sleep duration chính thức chưa được flowchart chốt |
| `3.3 V ≤ V_bat < 3.5 V` | `BATTERY_LOW` | Cảnh báo pin yếu, yêu cầu khóa OTA | Sleep duration chính thức chưa được flowchart chốt |
| `V_bat ≥ 3.5 V` | `BATTERY_NORMAL` | Tiếp tục đánh giá nguy cơ đất | Cần xác nhận theo loại pin thực tế |

### 3. Mức DANGER

Mức `DANGER` được kích hoạt khi **ít nhất một** điều kiện đúng:

```text
FS <= 1.0
OR DI >= 1.0
OR epsilon_star >= 1.0
```

Kết quả:

| Trường | Giá trị |
| --- | --- |
| `alert_level` | `DANGER` |
| `risk_status` | `LANDSLIDE_RISK` |
| `warning_message` | `Nguy cơ biến dạng/sạt lở cao` |
| `duty_cycle_mode` | `DANGER` |
| `sleep_duration` | 5 phút |

### 4. Mức WARNING

Chỉ xét khi không thỏa mãn `DANGER`. Mức `WARNING` được kích hoạt khi **ít
nhất một** điều kiện đúng:

```text
1.0 < FS <= 1.3
OR 0.5 <= DI < 1.0
OR 0.77 <= epsilon_star < 1.0
```

Kết quả:

| Trường | Giá trị |
| --- | --- |
| `alert_level` | `WARNING` |
| `risk_status` | `UNSTABLE_TREND` |
| `warning_message` | `Đất có xu hướng mất ổn định` |
| `duty_cycle_mode` | `WARNING` |
| `sleep_duration` | 20 phút |

### 5. Mức NORMAL

Mức `NORMAL` chỉ được xác lập khi dữ liệu hợp lệ và **đồng thời tất cả** điều
kiện sau đều đúng:

```text
FS > 1.3
AND DI < 0.5
AND epsilon_star < 0.77
AND Error_Flag = 0
AND analysis_valid = true
```

Kết quả:

| Trường | Giá trị |
| --- | --- |
| `alert_level` | `NORMAL` |
| `risk_status` | `SAFE` |
| `warning_message` | `Đất ổn định` |
| `duty_cycle_mode` | `NORMAL` |
| `sleep_duration` | 30 phút |

## Công thức các chỉ số dùng trong bảng

### Áp lực nước lỗ rỗng

```text
u = u_max × max(0, (H_soil - H_c) / (H_sat - H_c))
```

### Ứng suất mái dốc

```text
tau     = gamma × z × sin(beta) × cos(beta)
sigma_n = gamma × z × cos²(beta)
sigma_effective = sigma_n - u
```

### Sức kháng cắt và hệ số an toàn

```text
tau_f = c' + sigma_effective × tan(phi')
FS    = tau_f / tau
```

### Chỉ số động học

```text
DI = w1 × abs(beta_dot / beta_dot_crit)
   + w2 × (A_rms / A_crit)
```

### Độ giãn tương đối quy ước

```text
epsilon_star = 1 / FS
```

## Ngưỡng cấu hình ban đầu

Các giá trị dưới đây phục vụ mô phỏng và kiểm thử ban đầu, chưa phải kết quả
hiệu chuẩn cuối cùng:

| Tham số | Giá trị ban đầu | Đơn vị | Ghi chú |
| --- | ---: | --- | --- |
| `ADC_min` | 100 | count | Miền kiểm tra phần cứng hiện tại |
| `ADC_max` | 4090 | count | Miền kiểm tra phần cứng hiện tại |
| `ADC_wet` | 1200 | count | Đã hiệu chuẩn theo thông tin người dùng cung cấp |
| `ADC_dry` | 3400 | count | Đã hiệu chuẩn theo thông tin người dùng cung cấp |
| `H_c` | 65 | % | Ngưỡng bắt đầu tăng áp lực nước lỗ rỗng |
| `H_sat` | 95 | % | Ngưỡng gần bão hòa |
| `u_max` | 12 | kPa | Hệ số mô hình cần fit thực nghiệm |
| `beta_dot_crit` | 3 | độ/giờ | Phải thống nhất đơn vị với dữ liệu MPU |
| `A_crit` | 0.08 | g | Cần đo nền rung tại vị trí lắp |
| `w1` | 0.55 | Không đơn vị | Trọng số tốc độ đổi góc |
| `w2` | 0.45 | Không đơn vị | Trọng số rung động |
| `FS_warning` | 1.3 | Không đơn vị | Biên NORMAL/WARNING |
| `FS_danger` | 1.0 | Không đơn vị | Biên WARNING/DANGER |
| `DI_warning` | 0.5 | Không đơn vị | Biên NORMAL/WARNING |
| `DI_danger` | 1.0 | Không đơn vị | Biên WARNING/DANGER |
| `epsilon_warning` | 0.77 | Không đơn vị | Xấp xỉ nghịch đảo của FS = 1.3 |
| `epsilon_danger` | 1.0 | Không đơn vị | Tương ứng FS = 1.0 |
| `V_bat_low` | 3.5 | V | Phụ thuộc loại pin và tải |
| `V_bat_critical` | 3.3 | V | Phụ thuộc loại pin và tải |

Các giá trị `gamma=18 kN/m³`, `z=1 m`, `c'=5 kPa`, `phi'=28°`, `H_c=65%`,
`H_sat=95%`, `u_max=12 kPa`, `beta_dot_crit=3 độ/giờ`, `A_crit=0.08 g` và
trọng số `0.55/0.45` hiện là giả định mô hình. Chúng không phải kết quả đo của
mẫu đất DA2.

| Nhóm tham số | Nguồn phải dùng để chốt | Trạng thái hiện tại |
| --- | --- | --- |
| `ADC_wet`, `ADC_dry` | Hiệu chuẩn chính cảm biến đang sử dụng | Đã đo: 1200 và 3400 |
| `gamma` | Thí nghiệm density/unit weight trên mẫu đất | Giả định 18 kN/m³ |
| `z` | Khảo sát địa tầng hoặc đo hình học mô hình | Giả định 1 m |
| `c'`, `phi'` | Direct shear/triaxial trên đúng mẫu đất | Giả định 5 kPa và 28° |
| `H_c`, `H_sat`, `u_max` | Fit từ `H_soil` và áp lực nước lỗ rỗng đo đồng thời | Chưa fit |
| `beta_dot_crit`, `A_crit` | Thử nghiệm ổn định/nguy hiểm có gắn nhãn | Chưa xác nhận |
| `w1`, `w2` | Chuyên gia hoặc tối ưu trên dữ liệu có nhãn | Giả định 0.55/0.45 |

## Yêu cầu hiệu chuẩn trước khi chốt bảng

Trước khi dùng Bảng 3.2 làm ngưỡng chính thức trong báo cáo và firmware, cần:

1. Ghi đầy đủ metadata cho kết quả đã đo `ADC_dry=3400`, `ADC_wet=1200` và
   kiểm tra lặp lại khi đổi cảm biến, nguồn cấp hoặc loại đất.
2. Thu thập quan hệ giữa `H_soil` và áp lực nước lỗ rỗng để fit `H_c`, `H_sat`
   và `u_max`.
3. Đo nhiễu nền của MPU6050 khi đất ổn định để xác định `A_crit`.
4. Đo độ trôi góc theo thời gian để xác định `beta_dot_crit`.
5. Xác nhận `gamma`, `z`, `c'` và `phi'` bằng tài liệu hoặc thí nghiệm đất.
6. Kiểm tra ngưỡng pin dưới tải phát LoRa, không chỉ đo pin khi không tải.
7. Chạy thử các kịch bản NORMAL, WARNING, DANGER và sensor error.
8. Ghi lại ngày hiệu chuẩn, thiết bị đo, loại đất, số mẫu và sai số.

## Ghi chú sử dụng trong báo cáo

Khi đưa bảng vào báo cáo, cần ghi rõ:

> Các ngưỡng trong Bảng 3.2 là ngưỡng thiết kế ban đầu. Các tham số liên quan
> đến cảm biến, rung động, tốc độ thay đổi góc, áp lực nước lỗ rỗng và đặc tính
> cơ học đất cần được hiệu chỉnh bằng dữ liệu thực nghiệm trước khi sử dụng cho
> đánh giá ngoài hiện trường.
