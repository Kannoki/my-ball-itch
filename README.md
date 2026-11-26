# wdf-line-tracker

Firmware robot dò line dùng **PlatformIO** (Arduino framework) hỗ trợ **ESP32 38-pin** và **ESP32-S3**, điều khiển 4 động cơ qua **L298N**, 3 servo độc lập bằng **LEDC phần cứng** và giao tiếp không dây qua **Bluetooth Low Energy (Adafruit Bluefruit LE Connect)**.

---

## ⚙️ Các Build Environment (Combo Phần Cứng)

| Environment | Board | Cảm biến | Tài liệu đấu dây |
| :--- | :--- | :--- | :--- |
| **`esp32-ir`** *(mặc định)* | ESP32 38-pin (`esp32dev`) | IR 8 mắt (`HY-S301` / `TCRT5000`) | [docs/wiring-esp32-ir.md](docs/wiring-esp32-ir.md) |
| **`s3-ir`** | ESP32-S3 DevKitC-1 | IR 8 mắt (`HY-S301` / `TCRT5000`) | [docs/wiring-s3-ir.md](docs/wiring-s3-ir.md) |
| **`esp32-photores`** | ESP32 38-pin (`esp32dev`) | Quang trở (LDR) 8 mắt | [docs/wiring-esp32-photores.md](docs/wiring-esp32-photores.md) |
| **`esp32-tcrt5000-2d41`** | ESP32 38-pin (`esp32dev`) | TCRT5000 `2D4.1` (5 mắt, 7 chân) | [docs/wiring-esp32-tcrt5000-2d41.md](docs/wiring-esp32-tcrt5000-2d41.md) |

Xem hướng dẫn nguồn điện (Pin 2S 7.4V + mạch hạ áp LM2596 5.0V) và tổng quan hệ thống tại [docs/wiring.md](docs/wiring.md).

### Thử nghiệm đủ 8 mắt trên `fix/line_sensor_optimize`

`main` giữ nguyên snapshot recovery trước simplify. Nhánh thử nghiệm dùng **toàn bộ S1–S8** cho `s3-ir` và `esp32-ir`; quang trở và TCRT5000 5 mắt giữ thuật toán recovery. Không thay GPIO, mặc định **HIGH = black** của IR 8 mắt hoặc cơ chế Standby khi khởi động.

Trên xe `s3-ir` hiện tại, thử đặt vạch dưới mắt ngoài cùng bên trái cho bit S8/GPIO 18 bật. Vì vậy cấu hình dùng `LINE_SENSOR_REVERSED = 1`, chuẩn hóa thứ tự đọc **S8 → S1** theo hướng **trái → phải khi xe tiến**. Cả chuỗi `GPIO`, mẫu line và bộ điều khiển dùng thứ tự đã chuẩn hóa này: vạch ở đầu trái phải hiện `#.......`, lệch trái mang dấu âm. Các cấu hình khác giữ thứ tự S1 → S8. Serial khi khởi động và chẩn đoán chi tiết ghi thứ tự chân thực tế.

Bộ điều khiển nằm ở [src/line_tracking.cpp](src/line_tracking.cpp), đọc mỗi **5 ms** (200 Hz danh định), lọc đa số **2/3 mẫu**. Một cụm liên tục 1–4 mắt được xem là line đơn; đây là giả thiết cho line ~20 mm và bước mắt 6 mm, cần kiểm tra theo góc/độ cao thực tế.

- **FOLLOW:** dùng tâm cụm line và PD để chỉnh hai cặp motor. Khi chưa chắc chắn, tốc độ nền là `LINE_SLOW_PWM = 165` trước power. Cho cặp phía ngoài tăng tới `LINE_STEER_PWM = 185` khi chỉnh lệch để tạo chênh lệch PWM; cặp phía trong vẫn tiến ít nhất PWM 165. Trước đây ép cả sàn và trần về 165 làm mất chỉnh hướng nhẹ, rồi cắt một cặp về 0 khi lệch lớn. Chỉ TURN/RECOVER mới ngắt cặp phía trong. Chỉ tăng tốc độ nền tới tốc độ cơ bản sau **200 ms** giữ tâm ổn định. Giới hạn mỗi cặp khi bám line là `max(tốc độ nền, LINE_STEER_PWM)`, tối đa `MOTOR_MAX_SPEED_PWM`; các số này là lệnh PWM, không phải vận tốc đo được.
- **OBSERVE:** mẫu rộng từ 5 mắt, nhiều cụm hoặc chạm mép với độ lệch lớn chỉ là **nghi góc/giao lộ**. Giảm tốc ngay rồi bò chậm, ghi nhớ các nhánh trái/phải đã thấy qua nhiều mẫu. Sau tối thiểu **80 ms**, chỉ chấp nhận lối thẳng khi line đơn ở giữa ổn định **20 ms**. Tối đa **240 ms** quan sát; nếu không có lối thẳng thì chọn nhánh **trái → phải** đã thấy. Tức ưu tiên tổng thể **thẳng → trái → phải**. Toàn active kéo dài chuyển phục hồi, không tự coi là cross để chạy thẳng vô hạn.
  Nếu hai mắt giữa đều mất line, chỉ một bên còn thấy line trên cả mẫu thô và lọc, xe dừng tiến ngay và xác nhận **20 ms** rồi rẽ về bên đó, không chờ đủ 80/240 ms. Nếu mất toàn bộ line lúc quan sát, xe cũng ngừng tiến trong lúc quyết định hướng theo nhánh đã ghi nhớ.
- **TURN_LEFT / TURN_RIGHT:** ghi nhớ hướng, ngắt PWM cặp motor phía trong và cho cặp phía ngoài tiến PWM **165** trước power. Rẽ trái là `(0, +165)`, rẽ phải là `(+165, 0)` theo đơn vị PWM của bộ điều khiển. Không cho cặp trong chạy lùi. Phải rời line cũ, rẽ tối thiểu **40 ms**, rồi bắt line đơn nằm giữa ổn định **30 ms** trên cả mẫu thô và mẫu lọc. Trong lúc xác nhận tâm thì ngắt PWM cả hai bên. Quá **1200 ms** thì vào phục hồi.
- **VERIFY:** tiến chậm và tiếp tục PD. Phải bám giữa ổn định **100 ms** mới trở lại FOLLOW; sau đó vẫn cần ổn định trước khi tăng tốc. Mất line hoặc quá **300 ms** chưa xác nhận được thì phục hồi. Nếu gặp dấu hiệu góc mới thì quan sát lại ngay, không có cooldown dài cản zigzag.
- **RECOVER:** dừng **40 ms**, quét về phía vừa rẽ/vừa thấy line **180 ms**, quét phía ngược **360 ms**, rồi quay lại **180 ms**. Cũng dùng một cặp tiến, cặp còn lại ngắt PWM, không đảo chiều motor. Bắt được line giữa thì qua VERIFY; nếu xác nhận lại thất bại cũng tính vào số lần phục hồi. Tối đa **2 lần**, tổng thời gian phục hồi kể cả xác nhận không quá **2500 ms**.
- **STOPPED:** hết giới hạn thì giữ dừng, phải tắt/bật auto bằng nút `1` hoặc đảo logic `I` để reset. Nếu chưa từng thấy line hợp lệ từ lúc bật auto, mất line sẽ dừng ngay thay vì quét mù. Một chuỗi OBSERVE/TURN/VERIFY cũng có giới hạn tổng **2000 ms** để không lặp vô hạn.

Các giới hạn là giá trị **thử nghiệm**, không phải kết quả đã hiệu chỉnh trên xe. Sensor đi trước trục bánh khoảng 4 cm theo cấu hình: thời gian bò chậm phải đo để bánh tới vị trí rẽ phù hợp. Không có encoder nên thời gian không bảo đảm khoảng cách, góc quét hoặc điểm quay lại chính xác; nếu điều kiện không rõ thì dừng có giới hạn. Không đọc được ý nghĩa mũi tên, không biết một nhánh hợp lệ có phải đường đích hay không; thứ tự chọn nhánh là quy tắc hành trình mặc định.

Nhấc xe, nền ít phản xạ và vùng đen có thể cho cùng tín hiệu digital. Flow này không phân biệt tuyệt đối các tình huống đó; cần kiểm tra ngưỡng và độ cao cảm biến trước khi chạy.

**Power auto là 65%** (`MOTOR_OUTPUT_PERCENT`, giảm từ 88% để hạn chế vượt góc), chỉ truyền cho lệnh của bộ điều khiển IR 8 mắt. Các lệnh motor thường và điều khiển tay mặc định **100%**. Auto dùng `runLR(left, right, power, true)` để bỏ ramp nhưng giữ đề-pa ngắn **40 ms** ở PWM **245** khi khởi hành. Mỗi kênh chỉ được đề-pa lần đầu hoặc sau ít nhất **500 ms ngắt PWM** (`MOTOR_AUTO_RESTART_MS`), không kích lại sau mỗi lần dừng ngắn để chỉnh hướng hay xác nhận line. PWM chạy thực tế không thấp hơn `XM` (mặc định **165** mỗi bên): `output = minPWM + max(0, requestedPWM - minPWM) * power / 100`. Ví dụ yêu cầu 185, power 65% → đầu ra 178; yêu cầu 165 → 165, yêu cầu dừng → 0. Manual vẫn giữ đề-pa/ramp cũ. Không thể giảm power xuống dưới ngưỡng kéo xe rồi coi đó là chạy chậm. Serial thêm `[AutoMotor]` với lệnh và PWM thực tế mỗi 500 ms để kiểm tra đầu ra; motor không có encoder nên số này không chứng minh bánh đã quay. Các combo dùng thuật toán recovery hiện chạy motor mặc định 100%.

Hai motor cùng bên dùng chung một cầu H của L298N; firmware không điều khiển từng bánh trước/sau độc lập. Trong auto, ngắt PWM cặp trong là thả trôi (`EN = LOW`), theo [datasheet L298, bảng 5](https://www.st.com/resource/en/datasheet/l298.pdf), không phải giữ phanh điện. Cách rẽ này tránh ép một bên từ tiến sang lùi khi còn bám sàn; bán kính rẽ và khoảng quét khác kiểu xoay tại chỗ, cần chạy lại trên xe để hiệu chỉnh. `power` chỉ đổi duty PWM, không phải giới hạn dòng; firmware hiện không đo dòng hay nhiệt độ nên không xác nhận hoặc bảo vệ quá tải điện.

Telemetry vẫn hiển thị đủ 8 bit raw và thêm `Track:FOLLOW`, `OBSERVE`, `TURN_LEFT`, `TURN_RIGHT`, `VERIFY`, `RECOVER` hoặc `STOPPED`. Serial còn ghi `[Line] trạng thái cũ -> mới` ngay khi chuyển trạng thái để thấy được lần rẽ ngắn giữa hai bản tin 500 ms. `FULL_ACTIVE` chỉ là tám mắt active, không xác nhận cross. Trường `E` mô tả mẫu tức thời để chẩn đoán, còn `Track` là trạng thái điều khiển thực tế sau lọc. `V` trên hai cấu hình IR chỉ báo chế độ thử nghiệm bám tâm; không chuyển sang dò mép. `I` vẫn đảo HIGH/LOW.

---

## 📌 Tính Năng Chính

- **Điều khiển 4 động cơ L298N theo kiểu `LbMotion.runLR` (Leanbot-style):**
  - **Power riêng cho auto IR 8 mắt: 65%**, điều khiển tay mặc định 100%. `runLR(left, right, powerPercent)` nhận power theo từng lệnh; các lời gọi hai tham số giữ mặc định 100%.
  - API chính: **`runLR(vL, vR)`** với **vận tốc chuẩn hoá `-1000..+1000`** trong *frame robot* (dương = tiến, âm = lùi, cho **cả hai bên**).
  - `runLR(1000, 1000)` = đi thẳng · `runLR(0, 0)` = dừng · `runLR(-500, 500)` = xoay tại chỗ sang trái.
  - Lớp trên cùng: `runForward` / `runBackward` / `runTurnLeft` / `runTurnRight` / `runArcLeft` / `runArcRight` / `stopMotion`.
  - **Cực thực tế vật lý** (`INVERT_LEFT_MOTORS` / `INVERT_RIGHT_MOTORS` / `SWAP_LEFT_RIGHT_MOTORS`) được áp dụng **ở tầng ghi GPIO**, *không* áp lên tốc độ đã lệnh — nên việc đảo một bên **không làm sai hướng xoay** (`runTurnLeft` luôn xoay sang trái).
  - **Vùng chết (dead-band)** tách khỏi **duty tối thiểu**: yêu cầu nhỏ hơn dead-band → **dừng thật** (duty = 0) thay vì kéo hai động cơ.
  - **Ramp duty** trong `150 ms` + nhịp đề-pa cấu hình `245/255` trong `120 ms` khi khởi hành (PWM thực tế phụ thuộc power của lệnh), tránh giật khi đổi chiều.
- **Tự chẩn đoán & cấu hình runtime qua BLE (không cần nạp lại firmware):**
  - `X` — báo cáo chẩn đoán động cơ (duty, mức GPIO `IN1..IN4`, cực, dead-band) + cảnh báo **còn jumper 5V trên ENA/ENB**.
  - `X1` — **tự kiểm tra**: trái tiến → phải tiến → trái lùi → phải lùi (để xác nhận 2 bên có quay cùng chiều).
  - `XI` / `XR` / `XS` — đảo chiều quay bên trái / bên phải / hoán đổi kênh vật lý 2 bên.
  - `XM <l> <r>` — đặt duty tối thiểu mà vẫn chạy được.
  - `XD <l> <r>` — đặt ngưỡng dead-band.
- **3 Servo điều khiển độc lập qua LEDC phần cứng (`50 Hz`, 14-bit):**
  - Tách riêng timer & kênh LEDC (`ch 2, 4, 6`, tránh xung đột timer `0` của động cơ).
  - **Servo 1 (`TD-8120MG`):** dải xung `500–2500 µs`, góc quay `0°–270°`.
  - **Servo 2 & Servo 3:** dải xung `500–2500 µs`, góc quay `0°–180°`.
- **Dò line linh hoạt (`5` hoặc `8` mắt):**
  - Hỗ trợ **Bám viền Đen–Trắng** (`TRANG TRAI - DEN PHAI`, `DEN TRAI - TRANG PHAI`, `AUTO`) và **Bám giữa vạch** (`BAM GIUA VACH` — mặc định cho IR 8 mắt và `esp32-tcrt5000-2d41`).
  - IR 8 mắt dùng lọc 2/3 mẫu, quan sát nhánh, xác nhận sau rẽ và phục hồi có giới hạn. Các combo khác giữ thuật toán recovery.
- **An toàn & điều khiển qua BLE (Adafruit Bluefruit LE Connect):**
  - Khởi động luôn ở chế độ **Standby** (dừng toàn bộ động cơ), chỉ chạy tự động khi bấm **Nút `1`**.
  - Truyền trạng thái cảm biến theo thời gian thực không chặn vòng lặp PID (`non-blocking TX queue` mỗi `300 ms`).

---

## 🔌 API Điều Khiển Động Cơ (cho lập trình viên)

Toàn bộ điều khiển động cơ nằm trong [`include/motor_driver.h`](include/motor_driver.h).

| Hàm | Ý nghĩa |
| :--- | :--- |
| `runLR(vL, vR)` | **API chính.** Vận tốc chuẩn hoá `-1000..+1000`, *frame robot* (dương = tiến cho cả hai bên). |
| `runForward(speed)` | Tiến thẳng (cả hai bên). |
| `runBackward(speed)` | Lùi. |
| `runTurnLeft(speed)` | **Xoay tại chỗ sang trái** — bên phải tiến, bên trái lùi. |
| `runTurnRight(speed)` | **Xoay tại chỗ sang phải** — bên trái tiến, bên phải lùi. |
| `runArcLeft(speed)` / `runArcRight(speed)` | Rẽ cong (bên trong chạy chậm, **không đảo chiều**). |
| `stopMotion()` | Dừng cả hai bên (duty = 0, thả chân hướng). |
| `getMotorRuntimeConfig()` / `applyMotorRuntimeConfig(cfg)` | Đọc/ghi cấu hình runtime (cực quay, dead-band, duty tối thiểu). |
| `toggleInvertLeft()` / `toggleInvertRight()` / `toggleSwapLeftRight()` | Đảo cực quay tại runtime. |
| `getMotorDiagnostics()` | Báo cáo chẩn đoán (duty, mức GPIO, cảnh báo jumper). |
| `runMotorSelfTest(stepMs)` | Tự kiểm tra: trái tiến → phải tiến → trái lùi → phải lùi. |

**Quy ước quan trọng:**
- Mọi lệnh đều dùng **frame robot** — `runTurnLeft` **luôn** xoay sang trái, bất kể cách đấu dây hay cờ `INVERT_*`/`SWAP_*`.
- Cực thực tế vật lý được áp dụng ở **tầng ghi GPIO** (`motor_driver.cpp`), không phải trên tốc độ đã lệnh.
- Đơn vị vận tốc chuẩn hoá: `1000` = hết tốc. Quy đổi sang PWM: `duty = speed * 255 / 1000`.
- Hàm cũ (`setMotorSpeeds`, `moveForward`, `spinLeft`, …) vẫn còn nhưng **dùng đơn vị PWM 0–255**; nên dùng API mới cho code mới.

---

## 📂 Cấu Trúc Dự Án

```text
wdf-line-tracker/
├── docs/
│   ├── wiring.md                      # Tổng quan nguồn điện, L298N, Servo, BLE
│   ├── wiring-esp32-ir.md             # Combo ESP32 38-pin + IR 8 mắt (mặc định)
│   ├── wiring-s3-ir.md                # Combo ESP32-S3 + IR 8 mắt
│   ├── wiring-esp32-photores.md       # Combo ESP32 38-pin + Quang trở 8 mắt
│   ├── wiring-esp32-tcrt5000-2d41.md  # Combo ESP32 38-pin + TCRT5000 2D4.1 (5 mắt)
│   └── wiring-signals.svg             # Sơ đồ tín hiệu ESP32-S3 tham khảo
├── include/
│   ├── config.h                       # Cấu hình chân GPIO theo board, PID, PWM, BLE
│   ├── ble_controller.h               # BLE Nordic UART Service (Bluefruit)
│   ├── line_sensor.h                  # Đọc cảm biến 5/8 mắt & tính vị trí line
│   ├── motor_driver.h                 # Điều khiển 4 động cơ L298N
│   ├── servo_pwm.h                    # Helper LEDC 50Hz 14-bit dùng chung cho servo
│   ├── servo1.h / servo2.h / servo3.h # Giao diện điều khiển 3 servo độc lập
├── src/
│   ├── main.cpp                       # Vòng lặp chính & gọi bộ điều khiển
│   ├── line_tracking.cpp              # Thử nghiệm đủ 8 mắt, nhớ hướng rẽ, timeout
│   ├── ble_controller.cpp             # Xử lý gói Control Pad & UART non-blocking
│   ├── line_sensor.cpp                # Lọc, giải mã, bám viền / bám giữa vạch
│   ├── motor_driver.cpp               # Xuất PWM & điều hướng L298N
│   ├── servo_pwm.cpp                  # Cấu hình LEDC phần cứng cho servo
│   └── servo1.cpp / servo2.cpp / servo3.cpp
└── platformio.ini                     # Cấu hình 4 build environments (mặc định esp32-ir)
```

---

## 🚀 Biên Dịch & Nạp Code

Yêu cầu cài **PlatformIO** (`pip install platformio` hoặc extension PlatformIO IDE trên VS Code).

```bash
# Biên dịch & nạp bản mặc định (esp32-ir: ESP32 38-pin + IR 8 mắt)
pio run -t upload

# Hoặc chọn env cụ thể theo phần cứng:
pio run -e s3-ir -t upload
pio run -e esp32-photores -t upload
pio run -e esp32-tcrt5000-2d41 -t upload

# Mở Serial Monitor
pio device monitor -b 115200
```

---

## 🎮 Điều Khiển Qua App Adafruit Bluefruit LE Connect

1. Mở Bluetooth & Định vị trên điện thoại → kết nối thiết bị theo tên `BLE_DEVICE_NAME` trong [include/config.h](include/config.h) (không cần mã PIN).
2. **Tab Controller → Control Pad:**
   - **Nút `1`**: Bắt đầu / Tạm dừng dò line.
   - **Nút `2` / `3` / `4`**: Đảo góc Servo 1 (`0° ↔ 270°`), Servo 2 (`0° ↔ 180°`), Servo 3 (`0° ↔ 180°`).
   - **D-Pad (`↑ ↓ ← →`)**: Lái xe thủ công khi đang ở chế độ Standby.
3. **Tab UART / Terminal (gửi lệnh & xem cảm biến trực tiếp):**
   - Telemetry mỗi `300 ms`: đủ 8 mức GPIO và trạng thái `Track` của bộ điều khiển. Các bit active là mức logic theo cấu hình, không xác nhận màu vật lý.
   - Lệnh nhanh:
     - `?` hoặc `H`: Xem chi tiết từng chân GPIO, mức logic, chế độ hiện tại.
     - `1`: Bắt đầu / Tạm dừng dò line.
     - `V`: Với IR 8 mắt, giữ chế độ thử nghiệm bám tâm cụm; các combo khác vẫn đổi chế độ như recovery.
     - `I`: Đảo mức logic vạch đen (`LOW ↔ HIGH`).
     - `P`: Bật / Tắt `INPUT_PULLUP` cho các chân cảm biến.
     - `E`: Bật / Tắt đèn phát IR (khi dùng mạch có chân `IR_EN`).
     - `T`: Bật / Tắt luồng telemetry tự động.
     - `+` / `-`: Tăng / Giảm tốc độ cơ bản (`MOTOR_BASE_SPEED`, bước `40`).
     - `2`, `3`, `4` hoặc `S1 <góc/µs>`, `S2 <góc>`, `S3 <góc>`: Điều khiển trực tiếp từng servo.
     - `F`, `B`, `L`, `R`, `S`: Tiến, lùi, quay trái, quay phải, dừng (khi ở Standby).
     - `X`: Báo cáo chẩn đoán động cơ (duty, mức GPIO, cực, dead-band, cảnh báo jumper ENA/ENB).
     - `X1`: Tự kiểm tra động cơ — trái tiến → phải tiến → trái lùi → phải lùi.
     - `XI` / `XR` / `XS`: Đảo chiều quay bên trái / bên phải / hoán đổi kênh vật lý 2 bên.
     - `XM <l> <r>`: Đặt duty tối thiểu mà vẫn chạy được (VD `XM 170 175`).
     - `XD <l> <r>`: Đặt ngưỡng dead-band (VD `XD 120 120`).

> **Lưu ý về `XI` / `XR` / `XS`:** đây là **cấu hình runtime trong RAM**, mất khi khởi động lại.
> Khi tìm được giá trị đúng, hãy chép ngược lại vào
> [`include/config.h`](include/config.h) (`INVERT_LEFT_MOTORS` / `INVERT_RIGHT_MOTORS` / `SWAP_LEFT_RIGHT_MOTORS`).

### 🛠️ Troubleshooting: Động cơ không chạy

Xem mục **"XE KHẮC PHỤC SỰ CỐ MOTOR KHÔNG CHẠY"** trong [docs/wiring.md](docs/wiring.md).
