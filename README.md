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

### IR 8 mắt: đọc đủ 8, dò line bằng 4 mắt

Với `s3-ir` và `esp32-ir`, firmware vẫn đọc và hiển thị đủ **S1–S8**, nhưng chỉ dùng **S2, S4, S5, S7** để tính vị trí line (`LINE_TRACKING_MASK = 0x5A`). **S1, S3, S6, S8** được bỏ qua khi điều chỉnh hướng; cả **8 mắt** cùng tham gia nhận cross. Số thứ tự ở đây bắt đầu từ **1**. Cấu hình quang trở và TCRT5000 5 mắt vẫn dùng toàn bộ mắt.

Line hiện tại rộng khoảng **20 mm**. Chế độ mặc định là **bám giữa vạch**: S4 và S5 cùng thấy line thì xe đi thẳng; mẫu thực tế `-#-..-#-` (S2/S7 active đối xứng, S4/S5 inactive) cũng là **cân tâm, sai số 0**. Khi line lệch về trái/phải thì đổi tốc độ hai bên theo sai số. Thuật toán giữ khoảng cách thật của bốn mắt được chọn: `−15, −3, +3, +15 mm` so với tâm thanh; không coi chúng là bốn mắt cách đều.

Telemetry ví dụ: `R:00011000 [...##...] U:-.-##-.- E:+0.0`. IR 8 mắt mặc định **HIGH = line**, dựa trên mẫu nền trắng đã đo `GPIO: 00000000`; mẫu trắng này báo `NO_LINE`, không phải cross. `R` là mức GPIO tức thời của đủ 8 mắt; `[...]` là tám bit đã giải mã theo `LINE_ACTIVE_STATE`; `U` chỉ ra các mắt tham gia tính vị trí (`-` = bỏ qua, `#` = active, `.` = inactive). Bộ điều khiển dùng lọc đa số 2/3 mẫu, còn telemetry hiển thị mẫu tức thời để chẩn đoán.

**Giới hạn nhận màu:** GPIO chỉ cho biết mức nhị phân, không đo màu đen/trắng hay độ phản xạ liên tục. Với IR 8 mắt:

- `CROSS`: **đủ 8 mắt cùng báo black**, không chỉ bốn mắt được chọn. Bộ điều khiển yêu cầu hai mẫu toàn black liên tiếp, rồi đi thẳng trong **140 ms** (`CROSS_LINE_PASS_DELAY_MS`) để qua giao lộ. Sau đó bám lại line; nếu vẫn toàn black thì dừng và tiếp tục quan sát, không lặp lại nhịp đi thẳng. Nếu mất line sau giao lộ thì tìm lại tối đa 750 ms.
- `NO_LINE`: không mắt nào trong S2/S4/S5/S7 báo line; áp dụng cơ chế tìm lại line.
- `AMBIGUOUS`: các mắt active tạo nhiều cụm rời chưa xác định được line, **ngoại trừ `-#-..-#-` đã xác nhận là mẫu cân tâm hợp lệ**.
- Chỉ bốn mắt được chọn cùng active, nhưng chưa đủ tám mắt, cũng là `AMBIGUOUS` và làm xe dừng sau bộ lọc. Mẫu thiếu phản xạ khi nhấc xe có thể giống toàn black; quy tắc cross này không phải bộ phát hiện nhấc xe.

Trước khi chạy, đo `R` trên nền trắng trơn và trên vạch đen ở cùng độ cao lắp thực tế; dùng `I` nếu mức active bị ngược. Nếu hai bề mặt cho cùng mức, cần kiểm tra phát IR và chỉnh ngưỡng module (nếu có biến trở); phần mềm không khôi phục được độ phản xạ đã mất khi tín hiệu bị chuyển thành HIGH/LOW. Nền có màu/hoa văn có thể phản xạ IR khác với cảm nhận bằng mắt. Tham khảo [Pololu: ảnh hưởng khoảng cách và nhiễu lên cảm biến phản xạ](https://www.pololu.com/docs/0J13/all) và [Vishay: đặc tính TCRT5000](https://www.vishay.com/docs/83760/tcrt5000.pdf).

---

## 📌 Tính Năng Chính

- **Điều khiển 4 động cơ L298N theo kiểu `LbMotion.runLR` (Leanbot-style):**
  - **PWM đầu ra còn 50%** (`MOTOR_OUTPUT_PERCENT`), áp dụng sau các mức tối thiểu và đề-pa cho mọi lệnh di chuyển. Giá trị lệnh và hệ số PID giữ nguyên; báo cáo `duty` là PWM thực tế đã giảm. Đây là tỷ lệ PWM, không phải phép đo vận tốc bánh xe.
  - API chính: **`runLR(vL, vR)`** với **vận tốc chuẩn hoá `-1000..+1000`** trong *frame robot* (dương = tiến, âm = lùi, cho **cả hai bên**).
  - `runLR(1000, 1000)` = đi thẳng · `runLR(0, 0)` = dừng · `runLR(-500, 500)` = xoay tại chỗ sang trái.
  - Lớp trên cùng: `runForward` / `runBackward` / `runTurnLeft` / `runTurnRight` / `runArcLeft` / `runArcRight` / `stopMotion`.
  - **Cực thực tế vật lý** (`INVERT_LEFT_MOTORS` / `INVERT_RIGHT_MOTORS` / `SWAP_LEFT_RIGHT_MOTORS`) được áp dụng **ở tầng ghi GPIO**, *không* áp lên tốc độ đã lệnh — nên việc đảo một bên **không làm sai hướng xoay** (`runTurnLeft` luôn xoay sang trái).
  - **Vùng chết (dead-band)** tách khỏi **duty tối thiểu**: yêu cầu nhỏ hơn dead-band → **dừng thật** (duty = 0) thay vì kéo hai động cơ.
  - **Ramp duty** trong `150 ms` + nhịp đề-pa cấu hình `245/255` trong `120 ms` khi khởi hành (PWM thực tế `122/255` ở đầu ra 50%), tránh giật khi đổi chiều.
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
  - Bộ lọc đa số 2/3 mẫu, tự xoay tìm lại line theo hướng lệch cuối (tối đa `750 ms` rồi dừng an toàn).
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
│   ├── main.cpp                       # Vòng lặp chính & thuật toán bám line
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
   - Telemetry mỗi `300 ms`: mức GPIO đủ 8 mắt, các bit giải mã và `U` đánh dấu mắt dùng tính vị trí. IR 8 mắt báo `E:<sai số>`, `CROSS`, `NO_LINE`, `AMBIGUOUS` hoặc `NO_EDGE`; `#`/`.` là active/inactive theo cấu hình logic, không phải xác nhận màu vật lý.
   - Lệnh nhanh:
     - `?` hoặc `H`: Xem chi tiết từng chân GPIO, mức logic, chế độ hiện tại.
     - `1`: Bắt đầu / Tạm dừng dò line.
     - `V`: Đổi chế độ bám (`TRANG TRAI - DEN PHAI` / `DEN TRAI - TRANG PHAI` / `AUTO` / `BAM GIUA VACH`).
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
