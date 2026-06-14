# Luồng công việc tổng quan hệ thống DA2

## 1. Quy ước đọc luồng

- `→`: hành động tiếp theo.
- `Yes/No`: nhánh quyết định.
- `[Node]`, `[Gateway]`, `[Cloud]`: nơi thực hiện hành động.
- `[Superloop]`: các bước được điều phối tuần tự bằng `setup()` và `loop()`.
- `Retry`: quay lại một bước trước với bộ đếm giới hạn.
- `Stop cycle`: kết thúc chu kỳ hiện tại, không phải dừng toàn hệ thống.

Kiến trúc hiện tại là **superloop**, chưa có FreeRTOS task/queue/mutex ở tầng
ứng dụng. Các tên `service...()` bên dưới biểu diễn hàm hoặc state machine được
gọi tuần tự. FreeRTOS chỉ là phương án nâng cấp về sau.

## 2. Luồng tổng thể một chu kỳ

```text
[Node] RTC đánh thức ESP32
→ Khởi tạo nguồn cảm biến, ADC, I2C, MPU6050 và LoRa
→ [Superloop] gọi lần lượt khối Soil, MPU và đọc pin
→ Hợp nhất snapshot cảm biến
→ Kiểm tra miền giá trị và tạo Error_Flag
→ Gán Node_ID, Gateway_ID, Packet_ID và timestamp
→ Đóng packet dữ liệu + CRC
→ Phát LoRa tới Gateway
→ Chờ ACK trong T_ACK
→ [Gateway] nhận packet và đo RSSI
→ Kiểm tra header + CRC + ID + duplicate
→ Lưu raw data
→ Tính u, tau, sigma_n, sigma_effective, tau_f, FS, DI, epsilon_star
→ Phân loại trạng thái
→ Tính Adaptive Duty Cycle
→ Gửi ACK gồm trạng thái và Sleep_Duration về Node
→ [Gateway] publish telemetry lên ThingsBoard
→ [Node] xác minh ACK
→ Lưu Sleep_Duration vào RTC memory
→ Cấu hình timer wake-up
→ Vào deep sleep
```

## 3. Luồng khởi động Node

```text
Node wake-up/reset
→ Đọc nguyên nhân wake-up
→ Khôi phục Packet_ID, beta_prev và Sleep_Duration từ RTC memory
→ Khởi tạo Serial debug
→ Khởi tạo ADC1
→ Khởi tạo I2C
→ Wake-up MPU6050
→ Đọc WHO_AM_I
→ WHO_AM_I hợp lệ?
  Yes → Cấu hình ACCEL_CONFIG + DLPF + SMPLRT_DIV
  No  → Đặt MPU error → retry có giới hạn
→ Khởi tạo SPI + LoRa
→ LoRa ready?
  Yes → Cho phép chu kỳ đo và truyền
  No  → Đặt RF error → chuẩn bị fallback sleep
```

## 4. Luồng đọc cảm biến độ ẩm đất

```text
[Superloop/serviceSoil] Bắt đầu chu kỳ Soil
→ Cấp nguồn cảm biến nếu có chân điều khiển
→ Chờ ổn định 500–1000 ms
→ Đọc một mẫu ADC
→ ADC_min < ADC < ADC_max?
```

Nhánh hợp lệ:

```text
Yes
→ Reset ADC error counter
→ Bỏ các mẫu đầu theo cấu hình ổn định
→ Thu đủ N = 11 mẫu hợp lệ
→ Sắp xếp mẫu
→ Chọn median
→ Tạo soil_adc_filtered
→ Dùng ADC_dry = 3400 và ADC_wet = 1200 đã hiệu chuẩn
→ Tính H_soil = (3400 - ADC_filtered) × 100 / 2200
→ Giới hạn H_soil vào 0–100%
→ Đặt soil_status = OK
→ Trả SoilData về biến trạng thái của chu kỳ Node
```

Nhánh không hợp lệ:

```text
No
→ Tăng ADC error counter
→ ADC error counter > 3?
  No  → Chờ 300–500 ms → Retry đọc ADC
  Yes → Đặt SENSOR_ERROR + soil_status = ERROR
      → Bỏ mẫu hiện tại
      → Trả SoilData lỗi về biến trạng thái của chu kỳ Node
```

## 5. Luồng đọc MPU6050

```text
[Superloop/serviceMpu] Đọc mẫu gia tốc đầu tiên
→ Khởi tạo Axf/Ayf/Azf bằng Ax_raw/Ay_raw/Az_raw
→ Lưu t_prev
→ Lấy t = monotonic clock
→ t - t_prev >= 200 ms?
  No  → Chờ/yield → lấy lại t
  Yes → Tính dt thực tế
      → Đọc Ax_raw, Ay_raw, Az_raw
      → Đọc I2C thành công?
```

Nhánh đọc lỗi:

```text
No
→ Tăng MPU error counter
→ Không cập nhật low-pass state
→ Retry có giới hạn
→ Quá giới hạn?
  Yes → Đặt MPU_ERROR → trả MPUData lỗi
  No  → Quay lại mốc lấy mẫu
```

Nhánh đọc thành công:

```text
Yes
→ Axf = alpha × Axf_prev + (1-alpha) × Ax_raw
→ Ayf = alpha × Ayf_prev + (1-alpha) × Ay_raw
→ Azf = alpha × Azf_prev + (1-alpha) × Az_raw
→ Tính pitch và roll bằng atan2
→ Đổi pitch/roll sang độ
→ Tính beta = sqrt(pitch_deg² + roll_deg²)
→ dt > 0?
  Yes → Tính beta_dot từ beta và beta_prev
  No  → Đánh dấu beta_dot chưa hợp lệ cho mẫu đầu
→ Tính Ax_vib/Ay_vib/Az_vib
→ Cộng bình phương rung vào cửa sổ RMS
→ Đủ cửa sổ 2000 ms?
  No  → Cập nhật state → quay lại lấy mẫu
  Yes → Tính A_rms trên toàn cửa sổ
      → Tạo MPUData
      → Cập nhật beta_prev và filter state
      → Trả MPUData về biến trạng thái của chu kỳ Node
```

## 6. Luồng hợp nhất và kiểm tra dữ liệu Node

```text
[Superloop/buildSensorSnapshot] Nhận SoilData
→ Nhận MPUData
→ Đọc V_bat
→ Tạo SensorSnapshot
→ Kiểm tra H_soil
→ Kiểm tra beta
→ Kiểm tra beta_dot với đúng đơn vị đã chốt
→ Kiểm tra A_rms
→ Kiểm tra V_bat
→ Có dữ liệu lỗi?
  Yes → Gộp bit vào Error_Flag
  No  → Error_Flag = 0
→ Tăng Packet_ID
→ Gắn timestamp_ms
→ Chuyển snapshot sang bước xử lý radio của cùng superloop
```

## 7. Luồng truyền Node → Gateway

```text
[Superloop/serviceNodeRadio] Nhận SensorSnapshot
→ Đóng packet DATA
→ Gắn Header + Gateway_ID + Node_ID + Packet_ID
→ Gắn dữ liệu cảm biến + timestamp + Error_Flag
→ Tính CRC
→ LoRa ready?
  No  → Đặt RF error → lưu packet pending → fallback sleep
  Yes → Phát packet
→ Chuyển radio sang receive
→ Chờ ACK trong T_ACK
→ Có packet phản hồi?
  No  → Tăng retry counter
  Yes → Kiểm tra Header + CRC + Node_ID + Packet_ID
      → ACK hợp lệ?
        Yes → Áp dụng Status + Alert_Level + Sleep_Duration
            → Đánh dấu transmission success
        No  → Tăng retry counter
→ retry >= MAX_RETRY?
  No  → Phát lại cùng Packet_ID
  Yes → Lưu packet vào retry buffer
      → Dùng local Adaptive Duty Cycle
      → Kết thúc transmission
```

## 8. Luồng nhận và xử lý tại Gateway

```text
[Superloop/serviceGatewayRadio] Kiểm tra LoRa RX
→ Nhận packet
→ Đọc RSSI
→ Sao chép packet vào RX buffer
→ Gọi bước `processGatewayPacket()` khi có packet hoàn chỉnh
→ Kiểm tra Header
→ Kiểm tra CRC
→ Kiểm tra Gateway_ID
→ Kiểm tra Node_ID
→ Kiểm tra (Node_ID, Packet_ID) đã xử lý?
```

Packet lỗi:

```text
Header/CRC/ID lỗi
→ Ghi Error_Log
→ Không parse thành telemetry bình thường
→ Không ACK nếu không đủ thông tin tin cậy
→ Kết thúc xử lý packet
```

Packet trùng:

```text
Duplicate = Yes
→ Không lưu raw data lần hai
→ Không publish telemetry lần hai
→ Gửi ACK status = DUPLICATE
→ Kết thúc xử lý packet
```

Packet mới hợp lệ:

```text
Duplicate = No
→ Parse SensorData
→ Gắn RSSI nhận tại Gateway
→ Lưu Raw_SensorData
→ Đưa snapshot vào Analysis flow
```

## 9. Luồng phân tích địa kỹ thuật

```text
[Superloop/processAnalysis] Nhận SensorData
→ Nạp bộ tham số mô hình kèm trạng thái nguồn/phiên bản
→ Xác nhận gamma, z, c', phi', H_c, H_sat, u_max đang là MEASURED hay PROVISIONAL
→ Kiểm tra Error_Flag và miền đầu vào
→ Dữ liệu đủ để phân tích?
```

Nhánh không đủ dữ liệu:

```text
No
→ analysis_valid = false
→ Không tạo số FS/epsilon giả
→ risk_status = SENSOR_ERROR hoặc ANALYSIS_INVALID
→ Chuyển sang Adaptive Duty Cycle
```

Nhánh đủ dữ liệu:

```text
Yes
→ Tính u = u_max × max(0, (H_soil-H_c)/(H_sat-H_c))
→ Tính tau = gamma × z × sin(beta) × cos(beta)
→ Tính sigma_n = gamma × z × cos²(beta)
→ Tính sigma_effective = sigma_n - u
→ Tính tau_f = c' + sigma_effective × tan(phi')
→ abs(tau) đủ lớn?
  No  → analysis_valid = false
  Yes → Tính FS = tau_f / tau
      → Tính DI
      → Tính epsilon_star = 1 / FS
      → analysis_valid = true nếu mọi kết quả hữu hạn
→ Chuyển sang phân loại cảnh báo
```

## 10. Luồng phân loại cảnh báo

```text
Nhận FS + DI + epsilon_star + analysis_valid
→ analysis_valid = false?
  Yes → WARNING/SENSOR_ERROR theo nguyên nhân
  No  → FS <= 1.0 OR DI >= 1.0 OR epsilon_star >= 1.0?
```

Nhánh nguy hiểm:

```text
Yes
→ Alert_Level = DANGER
→ Risk_Status = LANDSLIDE_RISK
→ Warning_Message = "Nguy cơ biến dạng/sạt lở cao"
→ Chuyển sang Adaptive Duty Cycle
```

Nhánh không nguy hiểm:

```text
No
→ 1.0 < FS <= 1.3 OR 0.5 <= DI < 1.0 OR 0.77 <= epsilon_star < 1.0?
  Yes → Alert_Level = WARNING
      → Risk_Status = UNSTABLE_TREND
      → Warning_Message = "Đất có xu hướng mất ổn định"
  No  → FS > 1.3 AND DI < 0.5 AND epsilon_star < 0.77?
      Yes → Alert_Level = NORMAL
          → Risk_Status = SAFE
          → Warning_Message = "Đất ổn định"
      No  → Alert_Level = WARNING
          → Risk_Status = CLASSIFICATION_GAP
→ Chuyển sang Adaptive Duty Cycle
```

## 11. Luồng Adaptive Duty Cycle

```text
[AdaptiveDutyCycle] Nhận H_soil + beta + beta_dot + A_rms
→ Nhận FS + DI + epsilon_star
→ Nhận V_bat + Error_Flag
→ Error_Flag != 0?
```

Nhánh lỗi cảm biến:

```text
Yes
→ Duty_Cycle_Mode = SENSOR_ERROR
→ Sleep_Duration = 10 phút
→ Tạo cảnh báo lỗi cảm biến
→ Đi tới bước giới hạn sleep
```

Nhánh dữ liệu hợp lệ:

```text
No
→ V_bat < 3.3 V?
  Yes → Battery_Status = CRITICAL
      → Yêu cầu khóa OTA
      → Gửi cảnh báo pin rất yếu
      → Sleep_Duration = DESIGN_TBD
  No  → V_bat < 3.5 V?
      Yes → Battery_Status = LOW
          → Yêu cầu khóa OTA
          → Gửi cảnh báo pin yếu
          → Sleep_Duration = DESIGN_TBD
      No  → DANGER?
          Yes → Duty_Cycle_Mode = DANGER
              → Sleep_Duration = 5 phút
          No  → WARNING?
              Yes → Duty_Cycle_Mode = WARNING
                  → Sleep_Duration = 20 phút
              No  → Duty_Cycle_Mode = NORMAL
                  → Sleep_Duration = 30 phút
```

Kết thúc duty cycle:

```text
Sleep_Duration đã được quyết định?
→ No  → Không được tự chọn trong code production
      → Dùng policy tạm thời có nhãn PROVISIONAL cho test
→ Yes → Giới hạn 5 phút <= Sleep_Duration <= 30 phút
→ Lưu Sleep_Duration vào RTC memory
→ Gắn Sleep_Duration vào ACK
→ Node gọi esp_sleep_enable_timer_wakeup()
→ Node gọi esp_deep_sleep_start()
```

## 12. Luồng ACK từ Gateway về Node

```text
[Superloop/processGatewayPacket] Hoàn thành validation + analysis + duty cycle
→ Tạo ACK gồm Gateway_ID + Node_ID + Packet_ID
→ Gắn Status + Alert_Level + Sleep_Duration
→ Tính CRC
→ Kiểm tra radio state đang sẵn sàng
→ Gửi ACK
→ Chuyển radio về receive
→ Chuyển radio state về receive
```

Ưu tiên ACK:

```text
ACK được gửi trước thao tác MQTT có thể block
→ Node nhận ACK đúng hạn
→ Node không retry packet không cần thiết
```

## 13. Luồng publish ThingsBoard

```text
[Superloop/serviceNetwork] Nhận processed telemetry
→ Wi-Fi connected?
  No  → Đưa telemetry vào Offline_Buffer
      → Đánh dấu Cloud_Status = OFFLINE
      → Retry khi Wi-Fi phục hồi
  Yes → MQTT connected?
      No  → Kết nối ThingsBoard
      Yes → Tiếp tục
→ Serialize JSON
→ Publish v1/devices/me/telemetry
→ Publish thành công?
  Yes → Đánh dấu packet cloud-synced
  No  → Đưa telemetry vào Offline_Buffer
→ Duy trì mqtt.loop()
```

Telemetry upload:

```text
Identity
→ Raw/filtered sensor data
→ RF + battery + error state
→ Geotechnical analysis
→ Alert state
→ Duty-cycle state
→ Original sample timestamp
```

## 14. Luồng phục hồi dữ liệu offline

```text
Wi-Fi/MQTT chuyển từ OFFLINE sang CONNECTED
→ Đọc packet cũ nhất trong Offline_Buffer
→ Publish với Packet_ID và timestamp gốc
→ Publish OK?
  No  → Giữ nguyên packet → dừng flush → retry sau
  Yes → Đánh dấu/xóa packet đã đồng bộ
      → Còn packet?
        Yes → Gửi packet tiếp theo
        No  → Chuyển Cloud_Status = ONLINE
```

## 15. Luồng command Gateway → Node

```text
[Cloud/User] Tạo command
→ Gateway nhận và lưu command
→ Kiểm tra Node_ID
→ Đóng packet CMD gồm Command + Parameter + Packet_ID + CRC
→ Chờ radio state sẵn sàng
→ Gửi command
→ Chờ ACK trong T_ACK
→ Có ACK?
  No  → Tăng retry
  Yes → Kiểm tra Node_ID + Packet_ID + CRC
      → Hợp lệ?
        Yes → COMMAND_SUCCESS
        No  → Tăng retry
→ retry >= MAX_RETRY?
  No  → Gửi lại command
  Yes → COMMAND_FAILED + Error_Log
```

Node xử lý command:

```text
[Superloop/serviceNodeRadio] Nhận CMD
→ Kiểm tra Header + CRC + Node_ID
→ Kiểm tra duplicate Packet_ID
→ Phân loại command
→ Kiểm tra parameter
→ Thực thi SET_SLEEP_DURATION / SET_THRESHOLD / REQUEST_DATA / SLEEP_NOW
→ Lưu cấu hình bền vững nếu cần
→ Tạo ACK status
→ Gửi ACK về Gateway
```

Lưu ý: node deep sleep không thể nhận command liên tục. Command phải chờ cửa sổ
node thức, hoặc thiết kế thêm cơ chế đánh thức ngoài; đây là ràng buộc hệ thống.

## 16. Luồng Superloop hiện tại và hướng nâng cấp

### 16.1 Node hiện tại

```text
setup()
→ Khởi tạo sensor + LoRa + state
→ loop()/runCycle()
→ Đọc Soil
→ Đọc MPU
→ Đọc pin
→ Hợp nhất SensorSnapshot
→ Validate + đóng packet
→ Gửi LoRa + chờ ACK/retry
→ Chọn Sleep_Duration
→ Lưu RTC state
→ Deep Sleep
```

Node xử lý một chu kỳ tuần tự rồi ngủ. Chưa có SoilTask, MpuTask,
NodeControlTask hoặc NodeRadioTask.

### 16.2 Gateway hiện tại

```text
setup()
→ Khởi tạo LoRa + Wi-Fi/MQTT state
→ loop()
→ serviceSerial()
→ serviceGatewayRadio()
→ Có packet? → processGatewayPacket()
→ Gửi ACK
→ serviceNetwork()
→ serviceOfflineBuffer()
→ Quay lại đầu loop
```

Mỗi service phải chạy ngắn và trả quyền điều khiển về `loop()`. Gateway chưa
có GatewayRadioTask, GatewayProcessingTask, GatewayNetworkTask hoặc queue RTOS.

### 16.3 FreeRTOS sau này

Chỉ chuyển sang FreeRTOS sau khi superloop đã đúng chức năng và có bằng chứng
timing cho thấy cần chạy độc lập. Khi đó có thể tách Soil, MPU, radio, analysis
và network thành task/queue. Đây là kế hoạch tương lai, không phải yêu cầu code
ở giai đoạn hiện tại.

## 17. Luồng Serial quan sát hệ thống

```text
Gateway boot
→ In LoRa init success/failure
→ In Wi-Fi connected/failed khi trạng thái thay đổi
→ In MQTT connected/failed khi trạng thái thay đổi
→ Nhận packet
→ In [RAW]
→ In [ANALYSIS]
→ In [STATE]
→ In ACK status
→ In MQTT publish OK/FAILED
```

Serial không được thay thế database và không được làm block superloop đủ lâu để
bỏ lỡ radio hoặc làm gián đoạn network service.

## 18. Điều kiện hoàn thành một chu kỳ

Một chu kỳ được coi là hoàn thành khi:

```text
Node đã tạo snapshot có trạng thái rõ ràng
→ Packet đã được gửi hoặc lưu pending
→ ACK đã nhận hoặc retry đã kết thúc
→ Sleep_Duration đã được xác định hợp lệ
→ State cần thiết đã lưu RTC/NVS
→ Node đã vào deep sleep
```

Phía gateway, packet chỉ được coi là xử lý hoàn chỉnh khi:

```text
Validation hoàn tất
→ Duplicate state được quyết định
→ Raw data được lưu theo cam kết
→ Analysis/state được tạo hoặc đánh dấu invalid
→ ACK được gửi
→ Telemetry được publish hoặc lưu offline buffer
```
