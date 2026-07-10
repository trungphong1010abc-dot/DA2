# 03. Soil Geotechnical Model - Bộ thông số đất, mô hình FS, DI và epsilon

File này là phụ lục chi tiết cho các mục 6.8 và 9.1-9.4 của
`00_DESIGN_RULES.md`. Nó gộp hai nội dung trước đây: cơ sở chọn bộ thông số đất
đỏ bazan và diễn giải mô hình địa kỹ thuật `FS`, `DI`, `epsilon_star`.

Mục tiêu của file này:

- chốt bộ thông số đang dùng trong code;
- giải thích bản chất từng đại lượng trong công thức;
- chỉ ra dữ liệu nào đo trực tiếp, dữ liệu nào là giả định;
- tính thử một số kịch bản để thấy bộ thông số tạo ra kết quả như thế nào;
- giữ rõ giới hạn: profile hiện tại phục vụ đồ án, mô phỏng và kiểm thử, không
  thay thế khảo sát/thí nghiệm địa kỹ thuật.

Nguồn diễn giải chính: `C:\Users\Public\New Section 1.pdf`, các flowchart của
đồ án, `src/common/project_config.h`, `src/common/analysis.cpp` và các nội dung
cơ sở chọn tham số đã được dùng trong bản tài liệu trước.

## 1. Phạm vi áp dụng và profile đang dùng

Loại đất đại diện được chọn:

> **Đất đỏ bazan phong hóa pha sét, lớp trượt nông**, đại diện cho mô hình đất
> phổ biến tại khu vực Tây Nguyên Việt Nam.

Tên profile trong firmware:

```text
BASALT_RED_SOIL_V1_PROVISIONAL
```

Ý nghĩa của `PROVISIONAL`: bộ số này là profile giả định để triển khai phần
mềm, mô phỏng, kiểm thử thuật toán và dashboard. Nó chưa phải kết quả thí
nghiệm của mẫu đất DA2 ngoài hiện trường.

## 2. Bộ thông số đất đang dùng trong code

Các giá trị dưới đây phải đồng bộ với `src/common/project_config.h`.

| Nhóm | Tham số | Giá trị | Đơn vị | Tên trong code | Trạng thái | Ý nghĩa nhanh |
| --- | --- | ---: | --- | --- | --- | --- |
| Hiệu chuẩn soil sensor | `ADC_wet` | 1400 | count | `SOIL_ADC_WET` | `MEASURED_SENSOR` | ADC khi cảm biến ở trạng thái ướt đã hiệu chuẩn |
| Hiệu chuẩn soil sensor | `ADC_dry` | 3400 | count | `SOIL_ADC_DRY` | `MEASURED_SENSOR` | ADC khi cảm biến ở trạng thái khô đã hiệu chuẩn |
| Nước lỗ rỗng | `H_c` | 65 | % | `MOISTURE_DANGER_START_PERCENT` | `ENGINEERING_ASSUMPTION` | Mốc độ ẩm bắt đầu làm `u_kpa` tăng khỏi 0 |
| Nước lỗ rỗng | `H_sat` | 95 | % | `MOISTURE_SATURATION_PERCENT` | `ENGINEERING_ASSUMPTION` | Mốc gần bão hòa theo chỉ số cảm biến |
| Nước lỗ rỗng | `u_max` | 10 | kPa | `PORE_PRESSURE_MAX_KPA` | `ENGINEERING_ASSUMPTION` | Áp lực nước lỗ rỗng cực đại giả định của profile |
| Cơ học đất | `gamma` | 18 | kN/m3 | `SOIL_GAMMA_KN_M3` | `ENGINEERING_ASSUMPTION` | Dung trọng đất |
| Cơ học đất | `z` | 1.0 | m | `SLIP_LAYER_DEPTH_M` | `ENGINEERING_ASSUMPTION` | Chiều dày lớp trượt giả định |
| Cơ học đất | `c'` | 5 | kPa | `SOIL_COHESION_KPA` | `ENGINEERING_ASSUMPTION` | Lực dính hiệu dụng |
| Cơ học đất | `phi'` | 28 | độ | `SOIL_FRICTION_ANGLE_DEG` | `ENGINEERING_ASSUMPTION` | Góc ma sát trong hiệu dụng |
| Động học | `beta_dot_crit` | 2.0 | độ/giờ | `BETA_DOT_CRIT_DEG_PER_HOUR` | `ENGINEERING_ASSUMPTION` | Ngưỡng chuẩn hóa tốc độ đổi góc trong DI |
| Động học | `A_crit` | 0.05 | g | `A_RMS_CRIT_G` | `ENGINEERING_ASSUMPTION` | Ngưỡng chuẩn hóa rung RMS trong DI |
| Động học | `w1` | 0.70 | Không đơn vị | `DI_WEIGHT_BETA_DOT` | `ENGINEERING_ASSUMPTION` | Trọng số cho tốc độ đổi góc |
| Động học | `w2` | 0.30 | Không đơn vị | `DI_WEIGHT_VIBRATION` | `ENGINEERING_ASSUMPTION` | Trọng số cho rung |
| Sanity | `beta_dot_sanity_max` | 360 | độ/giờ | `BETA_DOT_SANITY_MAX_DEG_PER_HOUR` | `ENGINEERING_ASSUMPTION` | Giới hạn phát hiện mẫu góc nhảy bất thường |
| Sanity | `A_rms_sanity_max` | 1.0 | g | `A_RMS_MAX_G` | `ENGINEERING_ASSUMPTION` | Giới hạn sanity cho rung RMS |

Phiên bản hằng số dạng firmware:

```text
SOIL_ADC_WET = 1400
SOIL_ADC_DRY = 3400

MOISTURE_DANGER_START_PERCENT = 65.0
MOISTURE_SATURATION_PERCENT = 95.0
PORE_PRESSURE_MAX_KPA = 10.0

SOIL_GAMMA_KN_M3 = 18.0
SLIP_LAYER_DEPTH_M = 1.0
SOIL_COHESION_KPA = 5.0
SOIL_FRICTION_ANGLE_DEG = 28.0

BETA_DOT_CRIT_DEG_PER_HOUR = 2.0
A_RMS_CRIT_G = 0.05
DI_WEIGHT_BETA_DOT = 0.70
DI_WEIGHT_VIBRATION = 0.30
```

## 3. Vì sao chọn bộ số này?

Không tìm thấy nguồn công khai đáng tin cậy cung cấp đồng thời toàn bộ
`gamma`, `z`, `c'`, `phi'`, `H_c`, `H_sat`, `u_max`, `beta_dot_crit` và
`A_crit` cho đúng mẫu đất của đề tài. Vì vậy chỉ `ADC_wet` và `ADC_dry` được
xem là đã hiệu chuẩn bằng phần cứng đang dùng; các tham số còn lại là giả định
kỹ thuật để firmware có một profile nhất quán.

| Tham số | Cơ sở chọn |
| --- | --- |
| `ADC_wet=1400`, `ADC_dry=3400` | Giá trị hiệu chuẩn người dùng cung cấp |
| `H_c=65%` | Chỉ bắt đầu tăng áp lực nước khi đất đã khá ẩm, tránh làm FS giảm quá sớm ở vùng khô-trung bình |
| `H_sat=95%` | Cho phép mô hình đạt vùng gần `u_max` trước khi ADC chạm đúng wet-point 100% |
| `u_max=10 kPa` | Bảo thủ nhưng không quá lớn so với tải phủ `gamma*z = 18 kPa` trong miền góc thường dùng |
| `gamma=18 kN/m3` | Giá trị đại diện cho đất khoáng ẩm/đất pha sét trong tính toán sơ bộ |
| `z=1.0 m` | Kịch bản lớp trượt nông, dễ diễn giải vì `gamma*z = 18 kPa` |
| `c'=5 kPa` | Chọn thấp để tránh đánh giá quá cao lực dính khi chưa có thí nghiệm |
| `phi'=28°` | Mức trung bình-bảo thủ cho đất pha sét/phong hóa |
| `beta_dot_crit=2°/h` | Bám ví dụ trong `New Section 1.pdf`: 2°/h là vùng nguy hiểm định tính |
| `A_crit=0.05 g` | Cao hơn rung nền kỳ vọng nhưng vẫn nhạy với rung bất thường |
| `w1=0.70`, `w2=0.30` | Ưu tiên biến dạng góc hơn rung môi trường |

## 4. Luồng tính tổng quát

Hệ thống không đo trực tiếp sạt lở. ESP32 node chỉ đo các tín hiệu đầu vào:

```text
soil_adc_filtered  -> h_soil
pitch_deg, roll_deg -> beta_deg
beta_deg theo thời gian -> beta_dot_deg_per_hour
Ax/Ay/Az sau lọc -> a_rms_g
```

Sau đó gateway hoặc khối phân tích dùng profile đất để suy ra các đại lượng cơ
học:

```text
h_soil
-> u_kpa
-> sigma_effective_kpa
-> tau_f_kpa
-> fs
-> epsilon_star

beta_dot_deg_per_hour + a_rms_g
-> di
```

Có thể hiểu nhanh:

| Chỉ số | Câu hỏi nó trả lời | Bản chất |
| --- | --- | --- |
| `FS` | Đất còn đủ sức chống trượt không? | Chỉ số cơ học tĩnh |
| `DI` | Đất có đang nghiêng/rung bất thường không? | Chỉ số động học |
| `epsilon_star` | Mức nguy cơ quy ước tăng hay giảm? | Chỉ số hiển thị từ `1 / FS` |

`FS`, `DI` và `epsilon_star` không phải số đo trực tiếp. Chúng là kết quả mô
hình. Vì profile hiện tại là `BASALT_RED_SOIL_V1_PROVISIONAL`, mọi kết luận chỉ
nên dùng cho mô phỏng, kiểm thử và dashboard đồ án, không dùng để tuyên bố an
toàn thực địa.

## 5. Vì sao loại đất làm thay đổi kết quả đo?

PDF nhấn mạnh cùng một cảm biến độ ẩm điện dung có thể cho phản ứng khác nhau
với từng loại đất. Lý do là cảm biến điện dung không đo trực tiếp "lượng nước
tuyệt đối", mà đo sự thay đổi đặc tính điện môi quanh đầu dò. Đặc tính này bị
ảnh hưởng bởi cấu trúc hạt, độ rỗng, độ chặt, khả năng giữ nước, khoáng vật và
cách nước phân bố trong đất.

| Nhóm đất | Bản chất vật lý | Ảnh hưởng đến cảm biến |
| --- | --- | --- |
| Tenosol | Đất nghèo dinh dưỡng, kết cấu rời, giữ nước kém | Nước thoát nhanh, vùng quanh đầu dò có thể khô nhanh hơn trạng thái tổng thể, dễ đọc thấp |
| Ferrosol | Đất đỏ giàu sắt, cấu trúc ổn định hơn, giữ ẩm tương đối tốt | Phản ứng trung bình hơn nhưng vẫn phụ thuộc cấu trúc hạt, độ chặt và thành phần sắt |
| Vertosol | Đất sét nặng, giữ nước cao, co ngót/nứt khi khô | Nước giữ nhiều nhưng phân bố không đều, khe nứt làm tín hiệu dao động lớn |

Hai hiện tượng cần nhớ:

- **Negative bias**: cảm biến đọc thấp hơn trạng thái ẩm thực tế. Ví dụ đất giữ
  nước không đều hoặc nước thoát nhanh khỏi vùng quanh đầu dò.
- **Variance cao**: các lần đo dao động lớn do đất không đồng nhất, độ sâu cắm
  khác nhau, độ chặt khác nhau hoặc nhiễu môi trường.

Kết luận cho firmware:

- `h_soil` chỉ là **độ ẩm tương đối theo cảm biến**, không phải độ ẩm thể tích.
- Không dùng chung một bộ `ADC_dry`, `ADC_wet`, `H_c`, `H_sat` cho mọi loại đất.
- Khi đổi loại đất, vị trí cắm, độ sâu cắm hoặc cảm biến, phải hiệu chuẩn lại.

## 6. Chuỗi đại lượng và vai trò

| Đại lượng | Bản chất | Nguồn | Đơn vị | Có đo trực tiếp không? |
| --- | --- | --- | --- | --- |
| `soil_adc_filtered` | Điện áp analog của cảm biến đất sau median filter | ESP32 ADC1 GPIO34 | count | Có |
| `h_soil` | Độ ẩm tương đối theo hai điểm dry/wet | Tính từ ADC | % | Không, là derived |
| `u_kpa` | Áp lực nước lỗ rỗng giả định | Tính từ `h_soil` | kPa | Không |
| `beta_deg` | Góc nghiêng tổng | MPU6050 pitch/roll | độ | Suy ra từ cảm biến |
| `beta_dot_deg_per_hour` | Tốc độ thay đổi góc nghiêng | Chênh lệch `beta` theo thời gian | độ/giờ | Không, là derived |
| `a_rms_g` | Mức rung RMS trong cửa sổ mẫu | MPU6050 sau xử lý | g | Không, là derived |
| `gamma` | Dung trọng đất | Profile/thí nghiệm/giả định | kN/m3 | Không đo bởi node |
| `z` | Chiều dày lớp trượt | Hình học/khảo sát/giả định | m | Không đo bởi node |
| `c'` | Lực dính hiệu dụng | Thí nghiệm/giả định | kPa | Không đo bởi node |
| `phi'` | Góc ma sát trong hiệu dụng | Thí nghiệm/giả định | độ | Không đo bởi node |
| `tau_kpa` | Ứng suất gây trượt | Mô hình mái dốc | kPa | Không |
| `sigma_n_kpa` | Ứng suất pháp tuyến tổng | Mô hình mái dốc | kPa | Không |
| `sigma_effective_kpa` | Ứng suất hữu hiệu | `sigma_n - u` | kPa | Không |
| `tau_f_kpa` | Sức kháng cắt | Mohr-Coulomb | kPa | Không |
| `fs` | Hệ số an toàn | `tau_f / tau` | Không đơn vị | Không |
| `di` | Chỉ số động học | `beta_dot` và `a_rms` | Không đơn vị | Không |
| `epsilon_star` | Độ giãn tương đối quy ước | `1 / FS` | Không đơn vị | Không |

## 7. Độ ẩm tương đối `h_soil`

### Bản chất

`h_soil` là độ ẩm tương đối được chuẩn hóa từ cảm biến độ ẩm điện dung:

```text
H_soil(%) = (ADC_dry - ADC_filtered) * 100 / (ADC_dry - ADC_wet)
```

Với cấu hình hiện tại:

```text
ADC_dry = 3400
ADC_wet = 1400
H_soil = (3400 - ADC_filtered) * 100 / 2000
```

Giải thích:

- Khi đất khô, ADC thường cao hơn, gần `ADC_dry`.
- Khi đất ướt, ADC thường thấp hơn, gần `ADC_wet`.
- Công thức đảo chiều để đất khô gần `0%`, đất ướt gần `100%`.

### Lưu ý

`h_soil=95%` không có nghĩa độ bão hòa vật lý `Sr=95%`. Nó chỉ có nghĩa tín hiệu
cảm biến đang nằm rất gần điểm wet đã hiệu chuẩn. Muốn biết độ bão hòa thật cần
phương pháp thí nghiệm đất riêng.

## 8. Áp lực nước lỗ rỗng `u_kpa`

### Bản chất vật lý

Trong đất có các khe rỗng giữa hạt. Khi đất ướt, nước trong khe rỗng tạo ra áp
lực. Áp lực này "gánh" một phần lực ép mà lẽ ra truyền qua tiếp xúc hạt-hạt.
Vì vậy nước nhiều làm ứng suất hữu hiệu giảm, ma sát giảm và đất yếu hơn.

Nói đơn giản:

```text
đất càng ẩm -> nước trong khe rỗng càng nhiều
-> áp lực nước lỗ rỗng tăng
-> lực ép thật giữa các hạt giảm
-> đất dễ trượt hơn
```

### Công thức

Firmware dùng:

```text
u = u_max * clamp((H_soil - H_c) / (H_sat - H_c), 0, 1)
```

Trong đó:

| Tham số | Bản chất | Giá trị hiện tại |
| --- | --- | ---: |
| `H_soil` | Độ ẩm tương đối hiện tại từ cảm biến | dữ liệu đo |
| `H_c` | Ngưỡng bắt đầu coi nước có ảnh hưởng đáng kể | `65%` |
| `H_sat` | Ngưỡng gần bão hòa theo chỉ số cảm biến | `95%` |
| `u_max` | Áp lực nước lỗ rỗng lớn nhất giả định trong profile | `10 kPa` |

### Vì sao phải `clamp(..., 0, 1)`?

Nếu đất còn khô hơn `H_c`, phần `(H_soil - H_c)` bị âm. Áp lực nước lỗ rỗng âm
là không hợp lý với mô hình đơn giản này, nên cận dưới của clamp giữ `u=0` khi
đất chưa đủ ẩm.

Nếu `H_soil` vượt `H_sat`, hệ thống coi đất đã vào vùng gần bão hòa theo chỉ số
cảm biến. Khi đó `u` không tăng tiếp mà giữ tại `u_max`, đúng nghĩa `u_max` là
áp lực nước lỗ rỗng cực đại giả định của profile.

Ví dụ:

```text
H_soil = 50%
H_c = 65%

(50 - 65) / (95 - 65) = -0.5
clamp(-0.5, 0, 1) = 0
u = 0 kPa
```

Ví dụ khi đất vượt mốc gần bão hòa:

```text
H_soil = 100%
(100 - 65) / (95 - 65) = 1.17
clamp(1.17, 0, 1) = 1
u = 10 * 1 = 10 kPa
```

Nhờ vậy code, design rule và PDF thống nhất: `u_kpa` luôn nằm trong khoảng
`0..u_max`.

### Vì sao chọn chặn trên tại `u_max = 10 kPa`?

Chặn trên tại `u_max` vì trong mô hình của đồ án, `u_max` được định nghĩa là
**áp lực nước lỗ rỗng cực đại giả định của profile**. Khi `H_soil >= H_sat`,
cảm biến độ ẩm điện dung chỉ cho biết đất đã ở vùng rất ướt/gần bão hòa theo
chỉ số cảm biến. Nó không đo trực tiếp áp lực nước lỗ rỗng, nên không có đủ dữ
liệu để suy ra `u` tiếp tục tăng tuyến tính vượt `u_max`.

Nguồn của quyết định này:

| Nội dung | Nguồn | Ghi chú |
| --- | --- | --- |
| Dạng mô hình `u = u_max * clamp(...)` | `C:\Users\Public\New Section 1.pdf` | PDF diễn giải `u_max` là áp lực nước lỗ rỗng lớn nhất giả định khi đất gần bão hòa |
| Giá trị `u_max = 10 kPa` | Profile `BASALT_RED_SOIL_V1_PROVISIONAL` trong tài liệu này và `src/common/project_config.h` | Đây là `ENGINEERING_ASSUMPTION`, không phải số đo piezometer |
| Cách xác định chính xác | Thí nghiệm/đo đồng thời `H_soil` và `u_reference` | Cần cảm biến áp lực nước lỗ rỗng hoặc piezometer để fit lại `H_c`, `H_sat`, `u_max` |

Lý do `10 kPa` hợp lý cho profile thử nghiệm hiện tại:

```text
gamma = 18 kN/m3
z = 1 m
gamma * z = 18 kPa
```

`gamma*z` là tải phủ đặc trưng của lớp đất trong mô hình. Với góc dốc thường
dùng, `sigma_n` nằm cùng bậc khoảng vài kPa đến dưới `18 kPa`. Chọn
`u_max=10 kPa` đủ để làm giảm mạnh ứng suất hữu hiệu khi đất gần bão hòa, nhưng
vẫn không phải một số quá lớn so với tải phủ của profile.

Ví dụ tại `beta=30°`:

```text
sigma_n = 18 * cos^2(30°)
        = 13.5 kPa

sigma_effective_at_umax = 13.5 - 10
                        = 3.5 kPa
```

Như vậy khi đất gần bão hòa, nước làm sức kháng cắt giảm rõ rệt, nhưng mô hình
vẫn còn miền tính toán dễ diễn giải. Nếu sau này đo thực tế thấy áp lực nước có
thể lớn hơn, phải tạo profile mới hoặc tăng `u_max`, không để công thức tự vượt
giá trị đã gọi là `max`.

## 9. Dung trọng đất `gamma`

### Bản chất

`gamma` là trọng lượng của đất trên một đơn vị thể tích. Đất càng nặng thì khối
đất trên mái dốc càng tạo ra ứng suất lớn hơn.

```text
gamma = rho * g
```

Với profile hiện tại:

```text
gamma = 18 kN/m3
```

Nếu đổi về khối lượng thể tích xấp xỉ:

```text
rho = 18000 / 9.81 ≈ 1835 kg/m3
```

### Nguồn lấy

Node không đo `gamma`. Giá trị đúng phải lấy từ thí nghiệm density/unit weight
trên mẫu đất hoặc khảo sát địa kỹ thuật. Trong đồ án hiện tại, `18 kN/m3` là
giả định kỹ thuật cho đất khoáng ẩm/đất pha sét, không phải kết quả đo mẫu DA2.

## 10. Chiều dày lớp trượt `z`

### Bản chất

`z` là chiều dày lớp đất phía trên mặt trượt giả định. Nó mô tả khối đất trượt
nông hay sâu. Lớp càng dày thì trọng lượng khối đất càng lớn, kéo theo `tau` và
`sigma_n` đều tăng.

Profile hiện tại:

```text
z = 1.0 m
```

### Nguồn lấy

MPU6050 không đo được `z`. Giá trị này phải lấy từ hình học mô hình, khảo sát
hố đào, khoan, mặt phân lớp hoặc giả định bài toán. Nếu mô hình vật lý chỉ dày
`0.2 m` thì phải dùng `z=0.2 m`, không giữ `1.0 m` chỉ vì profile mặc định.

## 11. Góc dốc `beta_deg`

### Bản chất

`beta_deg` là góc nghiêng tổng dùng trong mô hình mái dốc. Trong firmware, nó
được suy từ pitch và roll:

```text
beta_deg = sqrt(pitch_deg^2 + roll_deg^2)
```

`beta` càng lớn thì thành phần trọng lượng kéo đất trượt xuống dốc càng lớn.
Đồng thời thành phần ép vuông góc vào mặt trượt thường giảm, làm đất kém ổn
định hơn.

### Lưu ý lắp đặt

Nếu cảm biến không được gá cứng vào khối đất hoặc chưa hiệu chuẩn tư thế 0 độ,
`beta_deg` sẽ không còn đại diện tốt cho góc dốc/mức nghiêng của khối đất. Khi
đó FS và DI có thể sai dù công thức code đúng.

## 12. Ứng suất trên mái dốc: `tau_kpa` và `sigma_n_kpa`

### Bản chất

Một khối đất nằm trên mặt nghiêng chịu trọng lượng bản thân. Trọng lượng này có
thể tách thành hai phần:

- phần song song mặt trượt: kéo đất trượt xuống dốc;
- phần vuông góc mặt trượt: ép đất vào mặt trượt.

### Công thức

```text
tau     = gamma * z * sin(beta) * cos(beta)
sigma_n = gamma * z * cos^2(beta)
```

Trong đó:

| Đại lượng | Bản chất | Vai trò |
| --- | --- | --- |
| `tau_kpa` | Ứng suất gây trượt | Mẫu số của FS |
| `sigma_n_kpa` | Ứng suất pháp tuyến tổng | Cơ sở tính ứng suất hữu hiệu |

`gamma` dùng `kN/m3`, `z` dùng `m`, nên `gamma * z` có đơn vị `kN/m2`, tương
đương `kPa`.

### Ví dụ nhanh

Giả sử:

```text
gamma = 18 kN/m3
z = 1 m
beta = 30°
```

Khi đó:

```text
tau = 18 * sin(30°) * cos(30°)
    ≈ 7.79 kPa

sigma_n = 18 * cos^2(30°)
        = 13.50 kPa
```

Ý nghĩa: khối đất đang chịu khoảng `7.79 kPa` kéo trượt và `13.50 kPa` ép vuông
góc vào mặt trượt trước khi xét đến nước.

## 13. Ứng suất hữu hiệu `sigma_effective_kpa`

### Bản chất

Ứng suất hữu hiệu là phần lực ép thật sự truyền qua khung hạt đất. Đây mới là
phần tạo ra ma sát giữa các hạt.

```text
sigma_effective = sigma_n - u
```

Nói dễ hiểu:

```text
tổng lực ép xuống mặt trượt
- phần lực bị nước trong khe rỗng gánh
= lực ép thật giữa các hạt đất
```

### Vì sao đại lượng này quan trọng?

Ma sát giữa hạt đất phụ thuộc vào lực ép thật giữa các hạt. Khi mưa làm `u_kpa`
tăng, `sigma_effective_kpa` giảm. Dù hình học mái dốc không đổi, đất vẫn có thể
yếu đi vì phần ma sát bị giảm.

### Lưu ý code

Không tự ép `sigma_effective_kpa` âm thành 0 nếu design rule chưa yêu cầu. Giá
trị âm hoặc rất nhỏ có thể cho thấy mô hình đang ở vùng cực kỳ bất lợi hoặc bộ
tham số `u_max`, `gamma`, `z`, `beta` không còn phù hợp.

## 14. Lực dính hiệu dụng `c'`

### Bản chất

`c'` biểu diễn phần sức kháng cắt có sẵn do cấu trúc/kết dính của đất. Đất pha
sét hoặc đất có liên kết hạt có thể có `c'` lớn hơn đất cát rời.

Profile hiện tại:

```text
c' = 5 kPa
```

### Nguồn lấy

Node không đo `c'`. Giá trị đúng phải lấy từ thí nghiệm direct shear hoặc
triaxial trên mẫu đất. Trong đồ án, `5 kPa` là giả định bảo thủ để tránh làm FS
quá lạc quan.

## 15. Góc ma sát trong hiệu dụng `phi'`

### Bản chất

`phi'` thể hiện mức độ ma sát/cài khóa giữa các hạt đất. `phi'` càng lớn thì
đất càng có khả năng chống trượt nhờ ma sát.

Profile hiện tại:

```text
phi' = 28°
```

Trong công thức, code phải đổi `phi'` sang radian trước khi dùng `tan()`.

### Nguồn lấy

`phi'` thường lấy từ đường phá hoại Mohr-Coulomb:

```text
tau_failure = c' + sigma_effective * tan(phi')
```

Khi fit nhiều điểm thí nghiệm:

```text
c'   = giao điểm với trục tau
phi' = atan(độ dốc đường fit)
```

## 16. Sức kháng cắt `tau_f_kpa`

### Bản chất

Sức kháng cắt là khả năng chống lại chuyển động trượt. Nếu `tau_kpa` là lực kéo
đất trượt xuống dốc thì `tau_f_kpa` là lực giữ đất lại.

### Công thức Mohr-Coulomb

```text
tau_f = c' + sigma_effective * tan(phi')
```

Hai phần trong công thức:

| Thành phần | Ý nghĩa |
| --- | --- |
| `c'` | sức kháng do lực dính |
| `sigma_effective * tan(phi')` | sức kháng do ma sát giữa hạt đất |

Khi `u_kpa` tăng:

```text
u tăng
-> sigma_effective giảm
-> sigma_effective * tan(phi') giảm
-> tau_f giảm
-> FS giảm
```

Đây là lý do đất sau mưa lớn thường dễ mất ổn định hơn.

## 17. Hệ số an toàn `fs`

### Bản chất

`FS` là tỉ số giữa sức chống trượt và lực gây trượt:

```text
FS = tau_f / tau
```

Ý nghĩa:

| Miền FS | Bản chất | Trạng thái thiết kế |
| --- | --- | --- |
| `FS > 1.3` | lực giữ lớn hơn lực kéo đủ xa | `NORMAL` |
| `1.0 < FS <= 1.3` | lực giữ chỉ nhỉnh hơn lực kéo | `WARNING` |
| `FS <= 1.0` | lực giữ không thắng lực kéo | `DANGER` |

### Vì sao FS là chỉ số chính?

FS gom toàn bộ các yếu tố cơ học trước đó:

```text
độ ẩm -> u
góc dốc + gamma + z -> tau và sigma_n
u -> sigma_effective
c' + phi' + sigma_effective -> tau_f
tau_f / tau -> FS
```

Nếu đất khô hơn, `u` giảm, `sigma_effective` tăng, `tau_f` tăng và FS tăng.
Nếu đất ẩm hơn hoặc dốc hơn, FS thường giảm.

### Ví dụ

Giả sử:

```text
H_soil = 85%
beta = 30°
gamma = 18 kN/m3
z = 1 m
c' = 5 kPa
phi' = 28°
```

Tính:

```text
u = 10 * (85 - 65) / (95 - 65)
  = 6.67 kPa

tau = 18 * sin(30°) * cos(30°)
    = 7.79 kPa

sigma_n = 18 * cos^2(30°)
        = 13.50 kPa

sigma_effective = 13.50 - 6.67
                = 6.83 kPa

tau_f = 5 + 6.83 * tan(28°)
      = 8.63 kPa

FS = 8.63 / 7.79
   = 1.11
```

Kết quả `FS=1.11` nằm trong vùng `WARNING`.

### Lưu ý code

Nếu `tau` quá gần 0, FS không có ý nghĩa vì mẫu số gần bằng 0. Code hiện kiểm
tra:

```text
abs(tau) > 0.001
```

Nếu không đạt, `analysis_valid=false`, không thay FS bằng số giả.

## 18. Tốc độ đổi góc `beta_dot_deg_per_hour`

### Bản chất

`beta_dot` cho biết góc nghiêng thay đổi nhanh hay chậm theo thời gian. Đây là
dấu hiệu động học: dù FS chưa quá thấp, nếu khối đất đang nghiêng nhanh thì vẫn
có thể có chuyển động bất thường.

```text
beta_dot = abs(beta_current - beta_previous) / delta_time_hours
```

PDF đưa ví dụ định tính:

| `beta_dot` | Diễn giải |
| ---: | --- |
| `0.01°/h` | gần như ổn định |
| `0.5°/h` | bắt đầu đáng chú ý |
| `2°/h` | nguy hiểm |

Profile hiện tại chọn:

```text
beta_dot_crit = 2.0°/h
```

`beta_dot_crit` không phải giới hạn vật lý của MPU6050. Nó là ngưỡng chuẩn hóa
cho DI và cần được kiểm chứng bằng dữ liệu thử nghiệm.

## 19. Rung RMS `a_rms_g`

### Bản chất

MPU6050 đo gia tốc gồm cả thành phần chậm như trọng lực/tư thế và thành phần
rung nhanh. Để lấy rung, firmware tách phần low-pass:

```text
Ax_vib = Ax_raw - Ax_filtered
Ay_vib = Ay_raw - Ay_filtered
Az_vib = Az_raw - Az_filtered
```

Sau đó tính RMS:

```text
a_rms_g = sqrt(sum(Ax_vib^2 + Ay_vib^2 + Az_vib^2) / sample_count)
```

### Vì sao chia cho `sample_count`?

`sample_count` là số mẫu MPU6050 hợp lệ trong cửa sổ đo. Chia cho
`sample_count` để lấy trung bình bình phương trên mỗi mẫu. Nếu không chia, cửa
sổ nào lấy nhiều mẫu hơn sẽ tự động có tổng lớn hơn dù mức rung thật không đổi.

RMS là:

```text
Root Mean Square = căn bậc hai của trung bình bình phương
```

Vì vậy phép chia là phần "Mean" trong RMS.

### Ngưỡng hiện tại

```text
A_crit = 0.05 g
```

Nếu `a_rms_g = 0.05 g`, thành phần rung trong DI đạt mức chuẩn hóa bằng 1.
Ngưỡng này không lấy từ datasheet MPU6050; nó là giả định thiết kế và cần đo
nền rung thực tế để hiệu chỉnh.

## 20. Chỉ số động học `di`

### Bản chất

`DI` kết hợp hai dấu hiệu động học:

- đất/cụm cảm biến đang nghiêng nhanh;
- đất/cụm cảm biến có rung bất thường.

```text
DI = w1 * abs(beta_dot / beta_dot_crit)
   + w2 * (A_rms / A_crit)
```

Vì `beta_dot` có đơn vị độ/giờ và `A_rms` có đơn vị `g`, phải chia cho ngưỡng
tương ứng để đưa chúng về cùng thang tương đối.

### Ý nghĩa từng phần

| Thành phần | Ý nghĩa |
| --- | --- |
| `abs(beta_dot / beta_dot_crit)` | tốc độ nghiêng đã đạt bao nhiêu phần của ngưỡng nguy hiểm |
| `A_rms / A_crit` | rung RMS đã đạt bao nhiêu phần của ngưỡng nguy hiểm |
| `w1` | mức ưu tiên cho biến dạng góc |
| `w2` | mức ưu tiên cho rung |

Profile hiện tại:

```text
w1 = 0.70
w2 = 0.30
```

Nghĩa là hệ thống ưu tiên tốc độ nghiêng hơn rung động, vì góc nghiêng phản ánh
biến dạng hình học trực tiếp hơn, còn rung dễ bị ảnh hưởng bởi va chạm, môi
trường hoặc thao tác lắp đặt.

Điều kiện trọng số:

```text
w1 >= 0
w2 >= 0
w1 + w2 = 1
```

### Ví dụ

```text
beta_dot = 1.0°/h
beta_dot_crit = 2.0°/h
A_rms = 0.025 g
A_crit = 0.05 g
w1 = 0.70
w2 = 0.30

DI = 0.70 * (1.0 / 2.0) + 0.30 * (0.025 / 0.05)
   = 0.70 * 0.5 + 0.30 * 0.5
   = 0.50
```

Kết quả bắt đầu vùng `WARNING`.

## 21. Độ giãn tương đối quy ước `epsilon_star`

### Bản chất

`epsilon_star` được định nghĩa:

```text
epsilon_star = 1 / FS
```

Lý do: `FS` càng nhỏ thì nguy cơ càng cao, nhưng dashboard thường dễ nhìn hơn
khi chỉ số nguy cơ tăng lên cùng chiều với nguy hiểm. Lấy nghịch đảo biến FS
thành một đại lượng tăng khi đất mất ổn định hơn.

| FS | `epsilon_star` | Diễn giải |
| ---: | ---: | --- |
| `2.0` | `0.50` | ổn định hơn |
| `1.3` | `0.77` | biên warning |
| `1.0` | `1.00` | biên danger |
| `0.8` | `1.25` | nguy hiểm |

### Không phải strain thật

`epsilon_star` không phải biến dạng vật lý đo bằng strain gauge. Nó là chỉ số
quy ước suy ra từ FS. Vì vậy không được ghi là "đất giãn 0.77" theo nghĩa cơ học
thực nghiệm. Cách gọi đúng là "độ giãn tương đối quy ước" hoặc "chỉ số biến
dạng quy ước".

## 22. Tính thử bằng bộ thông số trong code

Phần này dùng đúng bộ thông số `BASALT_RED_SOIL_V1_PROVISIONAL` để kiểm tra
logic mô hình.

### 22.1 Kịch bản đất tương đối khô

Giả sử:

```text
H_soil = 50%
beta = 20°
```

Vì `H_soil < H_c`:

```text
u = 0 kPa
```

Tính ứng suất:

```text
tau = 18 * sin(20°) * cos(20°)
    ≈ 5.79 kPa

sigma_n = 18 * cos^2(20°)
        ≈ 15.89 kPa

sigma_effective = 15.89 - 0
                = 15.89 kPa
```

Tính sức kháng và FS:

```text
tau_f = 5 + 15.89 * tan(28°)
      ≈ 13.45 kPa

FS = 13.45 / 5.79
   ≈ 2.32

epsilon_star = 1 / 2.32
             ≈ 0.43
```

Kết luận: `FS > 1.3` và `epsilon_star < 0.77`, trạng thái cơ học ổn định theo
ngưỡng thiết kế nếu DI cũng thấp.

### 22.2 Kịch bản đất ẩm cao

Giả sử:

```text
H_soil = 85%
beta = 30°
```

Tính áp lực nước:

```text
u = 10 * (85 - 65) / (95 - 65)
  = 6.67 kPa
```

Tính ứng suất:

```text
tau = 18 * sin(30°) * cos(30°)
    = 7.79 kPa

sigma_n = 18 * cos^2(30°)
        = 13.50 kPa

sigma_effective = 13.50 - 6.67
                = 6.83 kPa
```

Tính sức kháng và FS:

```text
tau_f = 5 + 6.83 * tan(28°)
      = 8.63 kPa

FS = 8.63 / 7.79
   = 1.11

epsilon_star = 1 / 1.11
             = 0.90
```

Kết luận: `1.0 < FS <= 1.3` và `0.77 <= epsilon_star < 1.0`, trạng thái
`WARNING`.

### 22.3 Kịch bản gần bão hòa và góc dốc lớn

Giả sử:

```text
H_soil = 95%
beta = 35°
```

Tính:

```text
u = 10 kPa
tau = 18 * sin(35°) * cos(35°)
    ≈ 8.46 kPa

sigma_n = 18 * cos^2(35°)
        ≈ 12.08 kPa

sigma_effective = 12.08 - 10
                ≈ 2.08 kPa

tau_f = 5 + 2.08 * tan(28°)
      ≈ 6.11 kPa

FS = 6.11 / 8.46
   ≈ 0.72

epsilon_star = 1 / 0.72
             ≈ 1.39
```

Kết luận: `FS <= 1.0` và `epsilon_star >= 1.0`, trạng thái `DANGER`.

### 22.4 Ví dụ DI

Trường hợp bắt đầu cảnh báo:

```text
beta_dot = 1.0°/h
A_rms = 0.025 g

DI = 0.70 * (1.0 / 2.0) + 0.30 * (0.025 / 0.05)
   = 0.70 * 0.5 + 0.30 * 0.5
   = 0.50
```

Trường hợp nguy hiểm động học:

```text
beta_dot = 2.0°/h
A_rms = 0.05 g

DI = 0.70 * 1 + 0.30 * 1
   = 1.00
```

Kết luận: `DI=0.50` bắt đầu vùng `WARNING`; `DI=1.00` đạt vùng `DANGER`.

## 23. Phân loại cảnh báo

Code hiện tại phân loại theo thứ tự ưu tiên:

```text
1. Sensor/config/data error
2. Analysis invalid
3. DANGER nếu FS <= 1.0 hoặc DI >= 1.0 hoặc epsilon_star >= 1.0
4. WARNING nếu FS <= 1.3 hoặc DI >= 0.5 hoặc epsilon_star >= 0.77
5. NORMAL nếu không vướng các điều kiện trên
```

Ngưỡng chính:

| Chỉ số | Normal | Warning | Danger |
| --- | --- | --- | --- |
| `FS` | `> 1.3` | `1.0 < FS <= 1.3` | `<= 1.0` |
| `DI` | `< 0.5` | `0.5 <= DI < 1.0` | `>= 1.0` |
| `epsilon_star` | `< 0.77` | `0.77 <= epsilon_star < 1.0` | `>= 1.0` |

Nếu dữ liệu cảm biến lỗi hoặc phân tích không hợp lệ, hệ thống không được báo
`NORMAL`, kể cả các giá trị khác trông có vẻ an toàn.

## 24. Mapping với code hiện tại

Các giá trị dưới đây phải đồng bộ với `src/common/project_config.h`:

| Tham số | Giá trị | Tên code |
| --- | ---: | --- |
| `ADC_dry` | `3400 count` | `SOIL_ADC_DRY` |
| `ADC_wet` | `1400 count` | `SOIL_ADC_WET` |
| `H_c` | `65%` | `MOISTURE_DANGER_START_PERCENT` |
| `H_sat` | `95%` | `MOISTURE_SATURATION_PERCENT` |
| `u_max` | `10 kPa` | `PORE_PRESSURE_MAX_KPA` |
| `gamma` | `18 kN/m3` | `SOIL_GAMMA_KN_M3` |
| `z` | `1.0 m` | `SLIP_LAYER_DEPTH_M` |
| `c'` | `5 kPa` | `SOIL_COHESION_KPA` |
| `phi'` | `28°` | `SOIL_FRICTION_ANGLE_DEG` |
| `beta_dot_crit` | `2.0°/h` | `BETA_DOT_CRIT_DEG_PER_HOUR` |
| `A_crit` | `0.05 g` | `A_RMS_CRIT_G` |
| `w1` | `0.70` | `DI_WEIGHT_BETA_DOT` |
| `w2` | `0.30` | `DI_WEIGHT_VIBRATION` |

Profile:

```text
BASALT_RED_SOIL_V1_PROVISIONAL
```

Thứ tự tính trong `Analysis::evaluate()`:

```text
1. betaRad = degToRad(betaDeg)
2. phiRad = degToRad(SOIL_FRICTION_ANGLE_DEG)
3. gammaZ = SOIL_GAMMA_KN_M3 * SLIP_LAYER_DEPTH_M
4. porePressureKpa = calcPorePressure(h_soil)
5. tauDriveKpa = gammaZ * sin(beta) * cos(beta)
6. sigmaNormalKpa = gammaZ * cos(beta)^2
7. sigmaEffectiveKpa = sigmaNormalKpa - porePressureKpa
8. shearStrengthKpa = c' + sigmaEffectiveKpa * tan(phi')
9. factorOfSafety = shearStrengthKpa / tauDriveKpa
10. strainIndex = 1 / factorOfSafety
11. dynamicIndex = calcDynamicIndex(data)
12. alertLevel/riskStatus/dutyCycleMode
```

## 25. Những gì cần đo lại khi chuyển từ đồ án sang thực nghiệm

| Nhóm | Cần đo/xác nhận | Vì sao |
| --- | --- | --- |
| Cảm biến đất | `ADC_dry`, `ADC_wet`, độ sâu cắm, độ chặt đất | Cảm biến điện dung phụ thuộc loại đất và cách lắp |
| Nước trong đất | Quan hệ `H_soil -> u_reference` | Để fit `H_c`, `H_sat`, `u_max` |
| Hình học | `beta`, `z`, mặt trượt giả định | Ảnh hưởng trực tiếp đến `tau` và `sigma_n` |
| Cơ học đất | `gamma`, `c'`, `phi'` | Quyết định `tau_f` và FS |
| MPU | noise góc, drift, rung nền | Để chọn `beta_dot_crit`, `A_crit`, sanity limit |
| Cảnh báo | dữ liệu có nhãn normal/warning/danger | Để kiểm chứng ngưỡng FS, DI, epsilon |

## 26. Tài liệu phương pháp và nguồn tham khảo

- ASTM D7263-21, phương pháp xác định density và unit weight của mẫu đất:
  <https://store.astm.org/d7263-21.html>
- ASTM D3080/D3080M-23, direct shear consolidated drained:
  <https://store.astm.org/d3080_d3080m-23.html>
- ASTM D4767-11(2020), triaxial compression cho đất dính:
  <https://store.astm.org/d4767-11r20.html>
- TDK InvenSense MPU-6050, đặc tính thiết bị và các thang đo gia tốc:
  <https://invensense.tdk.com/en-us/products/motion-tracking/6-axis/mpu-6050/>
- `C:\Users\Public\New Section 1.pdf`, nguồn nội bộ giải thích công thức và ví
  dụ định tính cho `beta_dot`, `DI`, `FS` và `epsilon_star`.

Các tiêu chuẩn trên cung cấp phương pháp xác định, không cung cấp trực tiếp bộ
số của mẫu đất DA2.

## 27. Nguyên tắc viết báo cáo

- Ghi rõ `ADC_dry=3400` và `ADC_wet=1400` là hiệu chuẩn cảm biến hiện tại.
- Ghi rõ các tham số `gamma`, `z`, `c'`, `phi'`, `H_c`, `H_sat`, `u_max`,
  `beta_dot_crit`, `A_crit`, `w1`, `w2` là `ENGINEERING_ASSUMPTION`.
- Không viết `epsilon_star` như strain thật.
- Không viết `u_kpa` như áp lực nước đo trực tiếp.
- Không dùng profile `BASALT_RED_SOIL_V1_PROVISIONAL` để khẳng định an toàn
  ngoài hiện trường.
- Khi có dữ liệu thí nghiệm, tạo profile mới có version, nguồn và ngày đo, không
  sửa âm thầm profile V1.
