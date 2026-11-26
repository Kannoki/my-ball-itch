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

### Động cơ TT 1:48 và L298N

Hai chân ENA/ENB dùng PWM 8 bit ở **10 kHz** (`MOTOR_PWM_FREQ_HZ`), servo dùng **50 Hz**. Tháo jumper ENA/ENB để ESP32 điều tốc. Nhìn theo hướng tiến, khi xoay phải tại chỗ bánh trái chạy tiến và bánh phải chạy lùi. Xoay trái dùng chiều ngược lại. Dùng `X1` ở Standby để kiểm tra từng bên. Nếu nhầm bên, dùng `XS`; nếu một bên quay ngược, dùng `XI` (trái) hoặc `XR` (phải).

Các bước xoay ở trạm dùng tốc độ riêng `MOTOR_PIVOT_SPEED` (725/1000). Đo góc trên mặt sân thực tế ở tốc độ này rồi chỉnh `CROSS_RIGHT_45_MS` và `CROSS_LEFT_45_MS` trong `include/config.h`. Giá trị ban đầu là 300 ms cho mỗi 45°.

### Dò line và xử lý giao lộ

Ba cấu hình 8 mắt dùng **S1–S4 bên trái** và **S5–S8 bên phải**. Cấu hình TCRT5000 5 mắt dùng S1/S2 bên trái, S3 ở giữa và S4/S5 bên phải. Thấy S3 thì đi thẳng.

Các luật nằm trong `getLineAction()` ở [src/line_sensor.cpp](src/line_sensor.cpp):

- Không thấy mắt nào đọc đen → **mất line** (`STOP`).
- Khi bám vạch, xe dùng tâm của các mắt đang thấy line để điều chỉnh độc lập hai bánh. Bộ điều khiển PD dùng lại hệ số P/D của bản PID cũ, lọc sai lệch và giảm tốc trong cua.
- Khi gặp vạch rộng, xe đi thẳng. Trên bản 8 mắt, S1–S5 cùng thấy line cũng nhận diện được nhánh trạm ngắn về bên trái xe.
- Xe lấy mẫu mỗi 10 ms và xác nhận hai mẫu cùng loại. Nếu mất line ngay sau khi rẽ hoặc qua vạch ngang, xe lùi theo lệnh bánh đã lưu rồi thử đi thẳng lại. Bản 5 mắt đi thẳng tối đa 140 ms khi toàn bộ mắt cùng thấy line.

**Power chỉ dành cho auto:** `MOTOR_OUTPUT_PERCENT` hiện là **88%** và chỉ được truyền từ hàm dò line. Điều khiển tay, servo và các lệnh motor khác giữ hoạt động bình thường; `runLR(left, right)` mặc định power **100%**. Power là tỷ lệ PWM đầu ra, áp dụng cả đề-pa, không phải phép đo vận tốc bánh xe.

Ví dụ telemetry `R:00011000 [...##...] U:...##... FORWARD`. `R` là mức GPIO, `[...]` là mẫu đã giải mã theo `LINE_ACTIVE_STATE`, và `U` là mẫu dùng để dò line (`#` là đen, `.` là trắng). Telemetry hiển thị phân loại của mẫu hiện tại trước khi xác nhận hai lần đọc liên tiếp.

IR 8 mắt mặc định **HIGH = đen**, dựa trên mẫu nền trắng đã đo `GPIO: 00000000`. Dùng `I` để đảo nếu phần cứng ngược logic. Nếu trắng và đen đều cho cùng mức GPIO, cần kiểm tra độ cao, phát IR và ngưỡng module: phần mềm không thể phân biệt các bề mặt có cùng tín hiệu số. Xem [Pololu: cảm biến phản xạ](https://www.pololu.com/docs/0J13/all).

#### Chuỗi servo trên xe 8 mắt

Khi bật dò line, servo 2 và 3 về **180°**. Xe bám line thẳng liên tục **200 ms** để xác nhận đã rời vạch xuất phát. Chỉ sau khi thấy line lệch trái và phải đủ 40 ms mỗi bên ở đoạn cua chữ S, xe mới nhận trạm servo. Trước thời điểm này, mẫu cả 8 mắt đen chỉ cho xe đi thẳng; mất line ngay trên vạch xuất phát sẽ dừng chờ line.

Mỗi lượt bật dò line xử lý tối đa **3 trạm**, tương ứng ba vạch ngang bên phải trên sân. Ở mỗi trạm, xe đang đi thẳng gặp vạch ngang phủ S1–S5, rồi bắt lại line thẳng liên tục **200 ms**, sẽ thực hiện lần lượt:

1. Xoay phải tại chỗ **90°**, rồi lùi thẳng **2 giây** theo cài đặt mặc định.
2. Đưa servo 2 về **0°**, chờ **600 ms** theo mặc định, rồi đưa servo 3 về **45°**.
3. Xoay phải thêm **45°**, rồi lần lượt đưa servo 3 và servo 2 về **180°**.
4. Xoay trái **45°**, tiến bằng thời gian đã lùi, xoay trái **90°**, rồi tiếp tục dò line.

`CROSS_REVERSE_DEFAULT_MS` ở đầu [include/config.h](include/config.h) đặt thời gian lùi mặc định. Gửi `REV` để xem thời gian hiện tại hoặc `REV 2500` để đặt 2,5 giây cho lần chạy tiếp theo. Dùng tab BLE UART hoặc Serial Monitor **115200 baud** có ký tự xuống dòng. Cài đặt qua UART có hiệu lực đến khi khởi động lại.

Sau khi hoàn thành một trạm, xe phải bám line thẳng liên tục **200 ms** trước khi được nhận trạm kế tiếp. Việc đọc lại vạch trong lúc thao tác hoặc trước khi đủ khoảng xác nhận này sẽ được bỏ qua. Sau trạm thứ ba, xe dừng ở vạch đích khi hai mẫu liên tiếp mất line ngay sau vạch ngang. Bật lại chế độ dò line bắt đầu một lượt mới với bộ đếm về 0.

Cách nhận trạm giả định xe xuất phát tại chữ T và ba trạm là các vạch toàn bề ngang tiếp theo sau khi rời start. Cảm biến line xác định trạm theo mẫu đọc, nên một giao lộ khác tạo cùng chuỗi tín hiệu cũng có thể được tính là trạm. Khoảng line thẳng giữa các vạch cần đủ thời gian xác nhận 200 ms.

Firmware ước lượng góc quay và vị trí quay về bằng thời gian chạy motor. Hằng số `CROSS_LEFT_45_MS`, `CROSS_RIGHT_45_MS` và `CROSS_SERVO_SETTLE_MS` dùng để hiệu chỉnh thời gian quay và chờ servo trên xe thật.

---

## 📌 Tính Năng Chính

- **Điều khiển 4 động cơ L298N theo kiểu `LbMotion.runLR` (Leanbot-style):**
  - **Power auto 88%** (`MOTOR_OUTPUT_PERCENT`); điều khiển tay và lệnh motor thường dùng 100%. Báo cáo `duty` là PWM thực tế sau khi áp dụng power của lệnh đang chạy.
  - API chính: **`runLR(vL, vR)`** với **vận tốc chuẩn hoá `-1000..+1000`** trong *frame robot* (dương = tiến, âm = lùi, cho **cả hai bên**).
  - `runLR(1000, 1000)` = đi thẳng · `runLR(0, 0)` = dừng · `runLR(-500, 500)` = xoay tại chỗ sang trái.
  - Lớp trên cùng: `runForward` / `runBackward` / `runTurnLeft` / `runTurnRight` / `runArcLeft` / `runArcRight` / `stopMotion`.
  - **Cực thực tế vật lý** (`INVERT_LEFT_MOTORS` / `INVERT_RIGHT_MOTORS` / `SWAP_LEFT_RIGHT_MOTORS`) được áp dụng **ở tầng ghi GPIO**, *không* áp lên tốc độ đã lệnh — nên việc đảo một bên **không làm sai hướng xoay** (`runTurnLeft` luôn xoay sang trái).
  - **Vùng chết (dead-band)** tách khỏi **duty tối thiểu**: yêu cầu nhỏ hơn dead-band → **dừng thật** (duty = 0) thay vì kéo hai động cơ.
  - **Ramp duty** trong `150 ms` + nhịp đề-pa cấu hình `245/255` trong `120 ms`; power auto được áp dụng sau các mức PWM này.
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
  - Điều chỉnh tốc độ từng bánh theo tâm vạch. Trên sân 8 mắt, khi gặp góc vuông bên phải (S4–S8 hoặc S3–S8), xe giữ cua qua khoảng mất line tối đa 600 ms.
  - Xác nhận hai kết luận liên tiếp để hạn chế nhiễu. Cả 8 mắt thấy đen thì xe tiếp tục đi thẳng qua giao lộ.
- **An toàn & điều khiển qua BLE (Adafruit Bluefruit LE Connect):**
  - Khởi động luôn ở chế độ **Standby** (dừng toàn bộ động cơ), chỉ chạy tự động khi bấm **Nút `1`**.
  - Truyền trạng thái cảm biến theo thời gian thực không chặn vòng lặp dò line (`non-blocking TX queue` mỗi `300 ms`).

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
│   ├── config.h                       # Cấu hình GPIO, tốc độ, power auto, PWM, BLE
│   ├── ble_controller.h               # BLE Nordic UART Service (Bluefruit)
│   ├── line_sensor.h                  # Đọc cảm biến & phân loại thẳng/trái/phải/dừng
│   ├── line_tracking.h                # Cập nhật dò line và cấu hình thời gian lùi
│   ├── motor_driver.h                 # Điều khiển 4 động cơ L298N
│   ├── servo_pwm.h                    # Helper LEDC 50Hz 14-bit dùng chung cho servo
│   ├── servo1.h / servo2.h / servo3.h # Giao diện điều khiển 3 servo độc lập
├── src/
│   ├── main.cpp                       # Vòng lặp chính & lệnh dò line if/else
│   ├── ble_controller.cpp             # Xử lý gói Control Pad & UART non-blocking
│   ├── line_sensor.cpp                # Đọc GPIO & các luật dò line cơ bản
│   ├── line_tracking.cpp              # Dò line, lùi về mốc và chạy chuỗi servo
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
   - Telemetry mỗi `300 ms`: mức GPIO, các bit giải mã, `U` đánh dấu mắt đang dùng và một trạng thái: `STOP`, `FORWARD`, `LEFT`, `RIGHT`, `CROSS`.
   - Lệnh nhanh:
     - `?` hoặc `H`: Xem chi tiết từng chân GPIO, mức logic, chế độ hiện tại.
     - `1`: Bắt đầu / Tạm dừng dò line.
     - `V`: Nhắc lại các luật dò line cơ bản; không đổi chế độ.
     - `I`: Đảo mức logic vạch đen (`LOW ↔ HIGH`).
     - `P`: Bật / Tắt `INPUT_PULLUP` cho các chân cảm biến.
     - `E`: Bật / Tắt đèn phát IR (khi dùng mạch có chân `IR_EN`).
     - `T`: Bật / Tắt luồng telemetry tự động.
     - `+` / `-`: Tăng / Giảm tốc độ cơ bản (`MOTOR_BASE_SPEED`, bước `40`).
     - `REV` / `REV <ms>`: Xem / đặt thời gian lùi trong chuỗi servo (VD `REV 2000`).
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
