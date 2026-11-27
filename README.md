# wdf-line-tracker

Firmware robot dò line dùng **PlatformIO** (Arduino framework) hỗ trợ **ESP32 38-pin** và **ESP32-S3**, điều khiển 4 động cơ qua **L298N**, 3 servo độc lập bằng **LEDC phần cứng** và giao tiếp không dây qua **Bluetooth Low Energy (Adafruit Bluefruit LE Connect)**.

---

## ⚙️ Các Build Environment (Combo Phần Cứng)

| Environment | Board | Cảm biến | Tài liệu đấu dây |
| :--- | :--- | :--- | :--- |
| **`esp32-ir`** *(mặc định)* | ESP32 38-pin (`esp32dev`) | IR 8 mắt (`HY-S301` / `TCRT5000`) | [docs/wiring-esp32-ir.md](docs/wiring-esp32-ir.md) |
| **`s3-ir`** | ESP32-S3 DevKitC-1 | IR 8 mắt (`HY-S301` / `TCRT5000`) | [docs/wiring-s3-ir.md](docs/wiring-s3-ir.md) |
| **`esp32-photores`** | ESP32 38-pin (`esp32dev`) | Quang trở (LDR) 8 kênh analog | [docs/wiring-esp32-photores.md](docs/wiring-esp32-photores.md) |
| **`esp32-tcrt5000-2d41`** | ESP32 38-pin (`esp32dev`) | TCRT5000 `2D4.1` (5 mắt, 7 chân) | [docs/wiring-esp32-tcrt5000-2d41.md](docs/wiring-esp32-tcrt5000-2d41.md) |

Xem hướng dẫn nguồn điện (Pin 2S 7.4V + mạch hạ áp LM2596 5.0V) và tổng quan hệ thống tại [docs/wiring.md](docs/wiring.md).

Cả `esp32-photores` và bản chẩn đoán `esp32-photores-log` xuất `LINE` và giá trị ADC 12 bit của tám mắt qua Serial UART 115200 baud mỗi 500 ms. [Cách nạp và đọc log](docs/wiring-esp32-photores.md#log-riêng-tín-hiệu-chân-quang-trở).

### Dò line và xử lý giao lộ

Ba cấu hình 8 mắt dùng **S1–S4 bên trái** và **S5–S8 bên phải**. Cấu hình TCRT5000 5 mắt dùng S1/S2 bên trái, S3 ở giữa và S4/S5 bên phải. Thấy S3 thì đi thẳng.

`getLineAction()` phân loại mẫu cảm biến cho telemetry và điều khiển giao lộ. [src/line_sensor.cpp](src/line_sensor.cpp)

- Không thấy mắt nào đọc đen → **mất line** (`STOP`).
- Chỉ thấy bên trái → **rẽ trái** (`LEFT`).
- Chỉ thấy bên phải → **rẽ phải** (`RIGHT`).
- Thấy cả hai bên hoặc mắt giữa của bản 5 mắt → **đủ tín hiệu để tiến** (`FORWARD`).
- Cả **8 mắt cùng đen** → **đi thẳng qua giao lộ** (`CROSS`). Bản 5 mắt đi thẳng tối đa 140 ms trên mẫu toàn đen.

Khi bám đường chính, bản 8 mắt dùng PD theo tâm các mắt đang thấy vạch. Hai bên được điều tốc độc lập kể cả khi telemetry báo `FORWARD`. Mẫu có hơn bốn mắt đen được đi qua với hai bên chạy đều. Bản 5 mắt vẫn dừng bánh phía trong khi rẽ. [Bộ điều khiển PD](src/motor_steering.cpp) · [Hệ số](include/config.h)

Photores lấy mẫu mỗi **5 ms** và các bản cảm biến số lấy mẫu mỗi **10 ms**. Xe xác nhận hai lần liên tiếp trước khi đổi loại thao tác.

- Photores chạy cả hai bánh tiến khi chỉ S1/S2 hoặc S7/S8 thấy vạch. Bánh phía vạch chạy chậm hơn để xe vừa tiến vừa rẽ nhẹ. Trên đường chính, hai mẫu mất vạch liên tiếp cho xe xoay tại chỗ về phía thấy vạch gần nhất. Khi chưa từng thấy vạch lệch về bên nào, xe tìm sang trái. Hai mẫu liên tiếp cùng loại có vạch cho xe bám lại vạch.
- Bản IR 8 mắt lùi về mốc khi mất line sau khi rẽ hoặc qua giao lộ nếu chưa bắt lại line thẳng đủ **140 ms**. Xe đảo cặp lệnh cuối mỗi đoạn rồi thử đi thẳng **140 ms**, tạm dừng đếm khi còn trên giao lộ. Mất line khi chưa có mốc thì xe dừng chờ.

Trên bản photores, PWM ổn định mặc định là **168/255 khi tiến/lùi bằng tay** và **154/255 khi tự lái đi thẳng**. Pha quay nhanh dùng **249/255 tại ngã T và dấu `+`**, hoặc **242/255 tại trạm servo**. Lệnh quay trái/phải bằng tay dùng **220/255**. Tự bám vạch dùng hệ số **88%** khi hai bên chạy đều và **92%** khi hai bên nhận lệnh khác nhau. [Cấu hình động cơ](include/config.h)

Ví dụ telemetry `R:00011000 [...##...] U:...##... FORWARD`. `R` là mức GPIO với cảm biến số, hoặc kết quả `ADC >= ngưỡng` với photores. `[...]` là mẫu đã giải mã theo `LINE_ACTIVE_STATE`, và `U` là mẫu dùng để dò line (`#` là đen, `.` là trắng). Telemetry hiển thị phân loại của mẫu hiện tại trước khi xác nhận hai lần đọc liên tiếp.

IR 8 mắt mặc định **HIGH = đen**, dựa trên mẫu nền trắng đã đo `GPIO: 00000000`. Dùng `I` để đảo nếu phần cứng ngược logic. Nếu trắng và đen đều cho cùng mức GPIO, cần kiểm tra độ cao, phát IR và ngưỡng module: phần mềm không thể phân biệt các bề mặt có cùng tín hiệu số. Xem [Pololu: cảm biến phản xạ](https://www.pololu.com/docs/0J13/all).

#### Đường chạy photores và ngã rẽ theo encoder

Sau khi bám vạch hẹp ở S4 hoặc S5 đủ **200 ms** để rời vạch xuất phát, bản photores chạy theo thứ tự sau.

1. Tại **ngã T**, quay trái **90°**. Firmware nhận cụm rẽ phía trái hoặc dải ngang rộng.
2. Sau khi bắt lại vạch mới, gặp **giao lộ +** thì quay phải **90°**. Firmware nhận cụm rẽ phía phải hoặc dải ngang rộng.
3. Sau khi bắt lại vạch mới ở phía phải, bắt đầu nhận tối đa **ba trạm servo**. Vạch ngang phủ S3–S6 có thể kích hoạt trạm khi xe đã bám thẳng đủ thời gian xác nhận.

Sau hai mẫu phù hợp với bước hiện tại, xe dùng encoder để đưa tâm xe tới giữa giao lộ. Mốc tiến tính từ mép đầu là **120 mm cộng nửa bề rộng dải ngang**. Xe quay đủ mốc xung rồi tìm hai mẫu vạch mới gần góc đích trước khi sang bước kế tiếp. Khi quá thời gian hoặc xung không hợp lệ, xe trở về bám vạch và giữ bước hiện tại để thử lại. [Sơ đồ và quy trình kiểm tra](docs/wiring-esp32-photores.md#7-nhận-dạng-ngã-rẽ-90-và-giao-lộ-ngang)

Bản IR 8 mắt dùng luồng thăm dò nhánh trái và phát lại lịch sử lệnh motor theo chiều ngược khi mất vạch ở cuối nhánh. Sau khi quay về, xe thử đi thẳng 140 ms rồi bám vạch.

#### Chuỗi servo trên xe 8 mắt

Khi bật dò line, servo 2 và 3 về **180°**. Xe bám vạch thẳng **200 ms** để xác nhận đã rời vạch xuất phát. Bản photores nhận trạm sau khi hoàn tất hai lượt quay T và +. Bản IR nhận trạm sau khi rời vạch xuất phát.

Mỗi lượt bật dò line xử lý tối đa **3 trạm**, tương ứng ba vạch ngang bên phải trên sân. Ở mỗi trạm, xe đi qua vạch ngang phủ S3–S6 rồi bắt lại vạch thẳng liên tục **200 ms** trước khi thực hiện các bước sau.

1. Xoay phải tại chỗ **90°**, rồi lùi thẳng **2 giây** theo cài đặt mặc định.
2. Đưa servo 2 về **0°**, chờ **600 ms** theo mặc định, rồi đưa servo 3 về **45°**.
3. Xoay phải thêm **45°**, rồi lần lượt đưa servo 3 và servo 2 về **180°**.
4. Xoay trái **45°**, tiến bằng thời gian đã lùi, xoay trái **90°**, rồi tiếp tục dò line.

`CROSS_REVERSE_DEFAULT_MS` ở đầu [include/config.h](include/config.h) đặt thời gian lùi mặc định. Gửi `REV` để xem thời gian hiện tại hoặc `REV 2500` để đặt 2,5 giây cho lần chạy tiếp theo. Dùng tab BLE UART hoặc Serial Monitor **115200 baud** có ký tự xuống dòng. Cài đặt qua UART có hiệu lực đến khi khởi động lại.

Sau khi hoàn thành một trạm, xe phải bám line thẳng liên tục **200 ms** trước khi được nhận trạm kế tiếp. Việc đọc lại vạch trong lúc thao tác hoặc trước khi đủ khoảng xác nhận này sẽ được bỏ qua. Sau trạm thứ ba, xe tiếp tục dò line. Bật lại chế độ dò line bắt đầu một lượt mới với bộ đếm về 0.

Bản photores phân biệt giao lộ `+` và vạch trạm theo thứ tự trên sân. Khoảng vạch thẳng giữa các trạm cần đủ thời gian xác nhận **200 ms**.

Bản photores dùng hai encoder bánh trước để tính góc thân xe và kết thúc các bước quay 45° và 90° ở trạm servo. Bấm `1` để bắt đầu dò line sẽ đặt hướng hiện tại về 0°. Các bản IR dùng `CROSS_TURN_45_MS` để đặt thời gian quay. [Chân encoder và cách đọc góc](docs/wiring-esp32-photores.md#5-encoder-hai-bánh-trước)

#### Vòng trong Speed PI và hiệu chỉnh encoder (bản photores)

Bản photores chạy **hai vòng điều khiển lồng nhau**:

- **Vòng ngoài — PD theo tâm vạch** ([src/motor_steering.cpp](src/motor_steering.cpp)): tính tốc độ chuẩn hoá cần thiết cho từng bánh.
- **Vòng trong — PI tốc độ từng bánh** ([src/speed_pid.cpp](src/speed_pid.cpp)): dùng encoder đo tốc độ thật và hiệu chỉnh duty, giữ cho hai bánh chạy đều nhau.

Một xung encoder tương đương **10,367 mm** (chu vi 66 mm ÷ 20 xung). Ở tốc độ hành thông khoảng 350 mm/s chỉ có ~34 xung/giây, tức ~0,17 xung mỗi mẫu 5 ms — vì vậy PI **không** chạy trên từng mẫu mà trên **cửa sổ thời gian 150 ms** (`SPEED_PID_WINDOW_MS`), chu kỳ tính **20 ms** (`SPEED_PID_PERIOD_MS`). Sai số tính ra bằng mm/s; phần hiệu chỉnh bị giới hạn ở **±15%** duty mở (`SPEED_PID_OUT_LIMIT`) nên một phép đo sai không thể làm mất ổn định xe. Tích phân có giới hạn và chống windup.

Tham số khởi đầu trong [include/config.h](include/config.h):

| Tham số | Giá trị | Ý nghĩa |
| --- | ---: | --- |
| `SPEED_PID_KP` | `0.35` | Hệ số ti lệ, duty hiệu chỉnh cho mỗi mm/s sai lệch |
| `SPEED_PID_KI` | `0.08` | Khử sai lệch dư khi đi thẳng |
| `SPEED_PID_KD` | `0.00` | Để 0 — sai lệch tốc độ quá lượng tử, thêm Kd chỉ tăng nhiễu |
| `SPEED_PID_I_LIMIT` | `60` | Giới hạn tích phân |
| `SPEED_PID_OUT_LIMIT` | `0.15` | Giới hạn hiệu chỉnh 15% duty mở |
| `ENCODER_PULSE_SCALE_LEFT/RIGHT` | `1.000` | Hiệu chỉnh sai số xung mỗi bánh (ảnh hưởng cả YAW) |
| `SPEED_TRIM_LEFT/RIGHT` | `0` | Hiệu chỉnh feed-forward để xe đi thẳng |
| `BASE_SPEED_RAMP_MS` | `600` | Thời gian tăng tốc từ 60% lên 100% khi bắt đầu dò line |

**Cách hiệu chỉnh** (theo thứ tự, mỗi bước kiểm tra lại bước trước):

1. **Speed PI**: đặt cùng lệnh cho hai bánh, chạy thẳng 1 m, đo sai lệch giữa hai bánh. Tăng `SPEED_PID_KP` cho tới khi sai lệch được sửa trong khoảng 200 ms, rồi tinh chỉnh `SPEED_PID_KI` để triệt tiêu sai lệch dư.
2. **Encoder trim**: chạy thẳng 1 m, đếm xung mỗi bánh từ log Serial. 1 m ≈ 96,5 xung. Đặt `ENCODER_PULSE_SCALE_LEFT = xung_đo / 96,5`. Sau đó dùng `SPEED_TRIM_LEFT/RIGHT` để xử lý phần trôi còn lại. Kiểm tra lại `YAW` vẫn gần 0° sau 1 m.
3. **Position PD**: chỉ chỉnh sau khi Speed PI ổn định. Giá trị hiện tại là `LINE_STEERING_KP = 38.0`, `LINE_STEERING_KD = 14.0`.

Ngoài ra bánh trong khi rẽ luôn được giữ ở tốc độ tối thiểu `MOTOR_MIN_SPEED` (647) và duty thực tế được sàn ở **150/255** *sau khi* nhân hệ số 88%/92% — trước đây sàn này được áp trước nên duty thật chỉ còn 151/255, bánh trong có thể chết.

---

## 📌 Tính Năng Chính

- **Điều khiển 4 động cơ L298N theo kiểu `LbMotion.runLR` (Leanbot-style):**
  - **Hệ số PWM tự lái** là `MOTOR_OUTPUT_PERCENT` khi hai bên chạy đều và `MOTOR_TURN_OUTPUT_PERCENT` khi chỉnh hướng theo vạch. Pha quay nhanh theo góc encoder và lệnh motor thường dùng hệ số 100%. Báo cáo `duty` là PWM thực tế sau khi áp dụng hệ số của lệnh đang chạy.
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
  - Các luật cơ bản: không thấy line thì dừng, lệch bên nào thì rẽ bên đó, thấy cả hai bên thì đi thẳng.
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
│   ├── junction_detect.h              # Phân loại ngã rẽ 90° & giao lộ ngang
│   ├── speed_pid.h                    # Vòng trong PI tốc độ từng bánh
│   ├── wheel_encoders.h               # Đếm xung encoder & ước lượng vận tốc
│   ├── wheel_odometry.h               # Hình học quay: xung <-> góc
│   ├── servo_pwm.h                    # Helper LEDC 50Hz 14-bit dùng chung cho servo
│   ├── servo1.h / servo2.h / servo3.h # Giao diện điều khiển 3 servo độc lập
├── src/
│   ├── main.cpp                       # Vòng lặp chính & lệnh dò line if/else
│   ├── ble_controller.cpp             # Xử lý gói Control Pad & UART non-blocking
│   ├── line_sensor.cpp                # Đọc GPIO & các luật dò line cơ bản
│   ├── line_tracking.cpp              # Dò line, quay tại chốt 90° & chạy chuỗi servo
│   ├── junction_detect.cpp            # Quy tắc nhận dạng ngã rẽ 90° (thuần, unit-test được)
│   ├── speed_pid.cpp                  # PI tốc độ từng bánh trên cửa sổ thời gian
│   ├── wheel_encoders.cpp             # ISR đếm xung & cửa sổ ước lượng vận tốc
│   ├── wheel_odometry.cpp             # Quy đổi xung encoder <-> góc quay
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

> ⚠️ **Bản photores — phải ngắt nguồn trước khi nạp lại firmware.**
> GPIO 2 là **chân strapping** của ESP32: nó phải ở mức LOW hoặc floating lúc reset. Chân này đang điều khiển **servo 3**, nên nếu servo giữ GPIO 2 ở mức HIGH khi ESP32 reset, board sẽ **không vào được bootloader** và không nạp được firmware.
>
> Firmware hiện đã ép GPIO 2 về LOW ngay dòng lệnh đầu tiên của `setup()` để giảm rủi ro, nhưng cách chắc chắn vẫn là: **ngắt nguồn board (rút USB), đợi vài giây, rồi cắm lại và nạp**. Nếu board không tự vào bootloader, giữ nút BOOT khi cắm nguồn.
>
> Bản IR và TCRT5000 không dùng GPIO 2 cho servo nên không bị ảnh hưởng.

---

## 🎮 Điều Khiển Qua App Adafruit Bluefruit LE Connect

1. Mở Bluetooth & Định vị trên điện thoại → kết nối thiết bị theo tên `BLE_DEVICE_NAME` trong [include/config.h](include/config.h) (không cần mã PIN).
2. **Tab Controller → Control Pad:**
   - **Nút `1`**: Bắt đầu / Tạm dừng dò line.
   - **Nút `2` / `3` / `4`**: Đảo góc Servo 1 (`0° ↔ 270°`), Servo 2 (`0° ↔ 180°`), Servo 3 (`0° ↔ 180°`).
   - **D-Pad (`↑ ↓ ← →`)**: Lái xe thủ công khi đang ở chế độ Standby.
3. **Tab UART / Terminal (gửi lệnh & xem cảm biến trực tiếp):**
   - Telemetry mỗi `300 ms` hiển thị các bit cảm biến, các bit giải mã, `U` đánh dấu mắt đang dùng và một trạng thái `STOP`, `FORWARD`, `LEFT`, `RIGHT`, `CROSS`.
   - Lệnh nhanh:
     - `?` hoặc `H`: Xem chi tiết từng chân, giá trị ADC và ngưỡng với photores, cùng trạng thái dò line.
     - `1`: Bắt đầu / Tạm dừng dò line.
     - `V`: Nhắc lại các luật dò line cơ bản; không đổi chế độ.
     - `I`: Đảo mức logic vạch đen hoặc chiều so sánh ADC với ngưỡng.
     - `P`: Bật / Tắt `INPUT_PULLUP` cho các cảm biến số.
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
