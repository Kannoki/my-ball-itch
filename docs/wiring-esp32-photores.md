# ĐẤU DÂY COMBO 3: ESP32 38-PIN + CẢM BIẾN QUANG TRỞ 8 MẮT + L298N + 3 SERVO

> **Combo:** `ESP32-WROOM-32 DevKit (38 chân)` + `Cảm biến quang trở (LDR) 8 kênh analog` + `L298N (4 động cơ)` + `3 Servo`
>
> **Build env:** `[env:esp32-photores]` trong `platformio.ini` (`-D BOARD_ESP32_38PIN=1`, `-D SENSOR_PHOTORESISTOR=1`)

---

## 1. NGUỒN ĐIỆN (PIN 2S 7.4V + LM2596 5.0V)

| Nhánh nguồn | Điểm kết nối | Lưu ý |
| :--- | :--- | :--- |
| **Pin 2S (+)** (7.4V – 8.4V) | L298N `12V`, LM2596 `IN+`, dây đỏ **Servo 1** | Chỉ nối Servo 1 trực tiếp nếu bản TD-8120MG chịu được 8.4V |
| **LM2596 `OUT+` (chỉnh 5.0V)** | ESP32 `5V/VIN`, dây đỏ **Servo 2 & 3** | Chỉnh VOM về `5.0V` trước khi cắm tải |
| **GND chung** | Pin (−), LM2596 `IN−/OUT−`, ESP32, L298N, 3 Servo, Cảm biến | Bắt buộc nối chung toàn hệ thống |

---

## 2. ĐẤU DÂY CẢM BIẾN QUANG TRỞ (PHOTORESISTOR) 8 MẮT

Nhìn theo hướng xe chạy tới, firmware đánh số `S1` ở ngoài cùng bên **trái** và `S8` ở ngoài cùng bên **phải**. Bảng dưới đây ghi thứ tự GPIO sau khi đảo dãy tám kênh trong firmware.

| Chân cảm biến | GPIO ESP32 38-pin | Chức năng |
| :--- | :--- | :--- |
| **VCC** | **3V3** nếu cảm biến hỗ trợ | Nếu cảm biến cần 5 V, kiểm tra dải điện áp của các ngõ ra analog trước khi đấu vào ESP32 |
| **GND** | **GND** | Mass chung |
| **S1** | **GPIO 4** | ADC2, ngoài cùng bên trái |
| **S2** | **GPIO 25** | ADC2 |
| **S3** | **GPIO 33** | ADC1 |
| **S4** | **GPIO 32** | ADC1, giữa bên trái |
| **S5** | **GPIO 35** | ADC1, giữa bên phải |
| **S6** | **GPIO 34** | ADC1 |
| **S7** | **GPIO 39 (VN)** | ADC1 |
| **S8** | **GPIO 36 (VP)** | ADC1, ngoài cùng bên phải |

- Dùng tám ngõ ra analog của thanh cảm biến. Giữ tín hiệu đưa vào ESP32 trong khoảng 0–3,3 V, ưu tiên tối đa khoảng 3,1 V để ADC tránh bão hòa. Nếu mức ra có thể lên tới 5 V, hạ áp riêng cho từng kênh trước khi đấu. [Dải đo ADC của Espressif](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/adc.html#analogsetattenuation)
- Firmware giữ trở kéo tắt. Hai kênh S1 và S2 dùng ADC2, đọc được trong cấu hình hiện tại dùng BLE. Giữ Wi-Fi tắt để ESP32 đọc được hai kênh này. [Bảng chân ADC của Espressif](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html#gpio-summary)

---

## 3. ĐẤU DÂY MẠCH ĐỘNG CƠ L298N (4 ĐỘNG CƠ)

> Tháo 2 jumper `ENA`, `ENB` để nhận PWM từ ESP32 38-pin. Để trống cọc `5V` trên L298N (vẫn giữ jumper `5V Enable`).

| Chân L298N | GPIO ESP32 38-pin | Chức năng |
| :--- | :--- | :--- |
| **12V** | Pin 2S (+) | Nguồn động lực 4 động cơ |
| **GND** | GND chung | Mass chung |
| **5V** | *(ĐỂ TRỐNG)* | Không nối vào ESP32 / LM2596 |
| **ENA** | **GPIO 23** | PWM tốc độ 2 động cơ **PHẢI** (`OUT1 & OUT2`) |
| **IN1** | **GPIO 22** | Chiều quay động cơ **PHẢI** 1 |
| **IN2** | **GPIO 21** | Chiều quay động cơ **PHẢI** 2 |
| **IN3** | **GPIO 26** | Chiều quay động cơ **TRÁI** 1 |
| **IN4** | **GPIO 27** | Chiều quay động cơ **TRÁI** 2 |
| **ENB** | **GPIO 14** | PWM tốc độ 2 động cơ **TRÁI** (`OUT3 & OUT4`) |

- **2 động cơ TRÁI** đấu song song vào `OUT3 & OUT4`; **2 động cơ PHẢI** đấu song song vào `OUT1 & OUT2` (nếu đấu ngược hai bên, đặt `SWAP_LEFT_RIGHT_MOTORS = true` trong `include/config.h`).

---

## 4. ĐẤU DÂY 3 ĐỘNG CƠ SERVO

| Servo | Dây tín hiệu (Cam/Vàng) | Dây nguồn (Đỏ) | Dây GND (Nâu/Đen) |
| :--- | :--- | :--- | :--- |
| **Servo 1 (TD-8120MG)** | **GPIO 13** | Pin 2S (+) (hoặc nguồn ổn áp phù hợp) | GND chung |
| **Servo 2** | **GPIO 15** | LM2596 `OUT+` (5.0V) | GND chung |
| **Servo 3** | **GPIO 2** | LM2596 `OUT+` (5.0V) | GND chung |

> ⚠️ **Lưu ý quan trọng về GPIO 2 (Servo 3).** GPIO 2 là **chân strapping** của ESP32: lúc reset nó phải ở mức LOW hoặc floating. Vì chân này điều khiển Servo 3, nếu servo giữ GPIO 2 ở mức HIGH lúc board reset thì ESP32 **sẽ không vào được bootloader** và không nạp được firmware.
>
> - **Không kéo ngoài `GPIO 2` lên mức cao.**
> - **Luôn ngắt nguồn (rút USB) trước khi nạp lại firmware**, đợi vài giây rồi cắm lại. Firmware đã ép GPIO 2 về LOW ngay dòng lệnh đầu tiên của `setup()` để giảm rủi ro, nhưng power-cycle vẫn là cách chắc chắn.
> - Nếu board không tự vào bootloader, giữ nút **BOOT** trong lúc cắm nguồn.
>
> Nếu cần đổi sang chân khác thì phải sửa `PIN_SERVO_3` trong [include/config.h](../include/config.h) **và** đổi dây — hiện tại firmware giữ nguyên pin mapping theo yêu cầu.

---

## 5. ENCODER HAI BÁNH TRƯỚC

| Dây encoder | Chân ESP32 |
| :--- | :--- |
| **OUT bánh trước trái** | **GPIO 18** |
| **OUT bánh trước phải** | **GPIO 19** |
| **GND của hai encoder** | **GND chung** |
| **VCC** | Theo thông số của module. Tín hiệu đưa vào GPIO phải ở mức 3,3 V; nếu OUT lên 5 V thì hạ mức tín hiệu trước khi nối |

Firmware đếm cạnh lên của một tín hiệu `OUT` trên mỗi bánh, theo cấu hình **20 xung/vòng**. Chiều của xung được suy ra từ chiều motor đang cấp cho từng bên. Đây là góc ước lượng từ bánh xe, nên độ chính xác phụ thuộc vào độ bám sàn và đường kính lăn. [GPIO ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/gpio.html)

Đường kính bánh là **66 mm**, khoảng cách tâm bánh trái/phải **134 mm** và khoảng cách trục trước/sau **120 mm**. Mỗi xung tương ứng khoảng **10,37 mm** bánh lăn. Góc thân xe bằng chênh lệch quãng đường có dấu của bánh phải và trái chia cho bề ngang 134 mm rồi đổi sang độ. Một xung chênh lệch cho khoảng **4,43°**. [Mô hình truyền động vi sai](https://docs.wpilib.org/en/stable/docs/software/kinematics-and-odometry/differential-drive-kinematics.html)

| Góc quay tại chỗ | Độ lớn chênh lệch xung | Xung mỗi bánh khi quay đối xứng |
| :--- | ---: | ---: |
| **45°** | 10 | khoảng 5 |
| **90°** | 20 | khoảng 10 |

Khi bấm **`1` để bắt đầu dò line**, góc thân xe `YAW` được đặt về **0°** theo hướng xe lúc đó. Quay trái cho góc dương, quay phải cho góc âm. Các bước quay 45° và 90° tại trạm servo dùng mốc xung ở bảng và PWM **242/255** trong pha quay nhanh. Xe giảm tốc gần đích và dừng chuỗi nếu thiếu xung đúng chiều hoặc không tới đích trong 2,5 giây. Khi vào nhánh trái, xe ghi góc quay thực đo và dùng tín hiệu vạch để nhận biết đã vào nhánh.

Serial UART 115200 baud của `esp32-photores` xuất `YAW`, số xung từng bánh và `LINE_ANGLE`. `LINE_ANGLE` là góc ngắm từ tâm xe đến điểm thấy vạch trên dãy cảm biến cách tâm **120 mm**. Giá trị `--` xuất hiện khi mất vạch hoặc mẫu quá rộng để xác định tâm vạch. Dòng `[Turn]` in góc mục tiêu `TARGET`, góc vừa quay `TURN` và góc thân xe `YAW`.

---

## 6. LẮP ĐẶT & NẠP CODE

- Mặt đọc của các mắt cảm biến đặt cách sàn **12 mm (1,2 cm)**, theo kích thước lắp đặt đã chọn cho xe.
- Bề rộng line là **16 mm (1,6 cm)**. Hai kích thước được khai báo bằng `SENSOR_HEIGHT_MM` và `LINE_WIDTH_MM` trong cấu hình `SENSOR_PHOTORESISTOR`.
- Firmware đọc ADC 12 bit (`0–4095`). Mặc định, cả tám mắt dùng ngưỡng `3000` trong `PHOTORES_ADC_THRESHOLDS`. Giá trị dưới ngưỡng là đen (`#`), từ ngưỡng trở lên là nền (`.`). [Cấu hình](../include/config.h)
- Khi dò line, firmware lấy mẫu tám mắt mỗi **5 ms**. PWM ổn định khi đi thẳng là **154/255 ở chế độ tự lái** và **168/255 ở chế độ lái tay**.
- `LINE=#.......` hoặc `LINE=.#......` cho xe rẽ nhẹ sang trái. `LINE=......#.` hoặc `LINE=.......#` cho xe rẽ nhẹ sang phải. Cả hai bánh tiếp tục chạy tiến. Trên đường chính, nếu hai mẫu liên tiếp là `LINE=........`, xe xoay tại chỗ về phía thấy vạch gần nhất để tìm lại.
- **Ngã rẽ 90°** được nhận theo bề rộng, vị trí vạch dưới thanh cảm biến và thứ tự đường chạy. Xem [các mẫu và bước chạy](#71-thứ-tự-đường-chạy-và-mẫu-cảm-biến).
- **Biên dịch & nạp:**

  ```bash
  pio run -e esp32-photores -t upload
  pio device monitor -b 115200
  ```

> ⚠️ **Phải ngắt nguồn trước khi nạp lại firmware.**
> GPIO 2 là **chân strapping** của ESP32: lúc reset nó phải ở mức LOW hoặc floating. Chân này đang điều khiển **servo 3** (xem [mục 4](#4-đấu-dây-3-động-cơ-servo)), nên nếu servo giữ GPIO 2 ở mức HIGH khi board reset, ESP32 sẽ **không vào được bootloader** và không nạp được firmware.
>
> Firmware đã ép GPIO 2 về LOW ngay dòng lệnh đầu tiên của `setup()` để giảm rủi ro, nhưng cách chắc chắn vẫn là: **rút USB (ngắt nguồn), đợi vài giây, cắm lại rồi nạp**. Nếu board không tự vào bootloader, giữ nút BOOT trong lúc cắm nguồn.

- **Kiểm tra nhanh:** Mở BLE UART (`Adafruit Bluefruit LE Connect`) → gửi `?` để kiểm tra cảm biến (`I` nếu cần đảo logic), dùng Control Pad nút `1` (dò line), `2/3/4` (test Servo 1/2/3), D-Pad (chạy thử động cơ ở Standby).

### Log riêng tín hiệu chân quang trở

Cả `esp32-photores` và `esp32-photores-log` xuất `LINE` và giá trị ADC của S1–S8 qua Serial UART **115200 baud**, mỗi **500 ms**. Bản log riêng giữ hai chân Enable của motor ở mức LOW trong lúc đo. [Chương trình log riêng](../src/photores_log/main.cpp)

```bash
pio run -e esp32-photores-log -t upload
pio device monitor -e esp32-photores-log -b 115200
```

Với mẫu ADC dưới đây, S2 và S6 đọc từ 3000 trở lên nên được đánh dấu là nền (`.`).

```text
[1000 ms] LINE=#.###.## ADC S1=2659 S2=3011 S3=2743 S4=2480 S5=2291 S6=3093 S7=2697 S8=2672
```

### Hiệu chỉnh ngưỡng đen và trắng

1. Ở đúng chiều cao lắp và ánh sáng sân, ghi vài mẫu ADC khi từng mắt đi qua nền trắng và vạch đen. Dịch thanh ngang qua vạch để đo cả tám mắt.
2. Nếu cần đổi ngưỡng cho một mắt, lấy trung điểm giữa hai giá trị đại diện rồi cập nhật `PHOTORES_ADC_THRESHOLDS` theo thứ tự S1–S8. Ví dụ, nền `3300` và vạch `2700` cho ngưỡng `(3300 + 2700) / 2 = 3000`.
3. Nếu ADC trên vạch cao hơn trên nền, gửi `I` qua BLE để thử quy tắc `ADC >= ngưỡng`. Để lưu chiều so sánh qua lần khởi động sau, đặt `LINE_ACTIVE_STATE` thành `HIGH` trong nhánh `SENSOR_PHOTORESISTOR` và nạp lại firmware.
4. Gửi `?` qua BLE và kiểm tra từng mắt. `#` đánh dấu mắt nằm trên vạch, `.` đánh dấu mắt nằm trên nền. Nếu hai dải ADC chồng lên nhau, kiểm tra lại độ cao và ánh sáng trước khi chọn ngưỡng.

---

## 7. NHẬN DẠNG NGÃ RẼ 90° VÀ GIAO LỘ NGANG

### 7.1 Thứ tự đường chạy và mẫu cảm biến

Bit 0 là **S1** ở ngoài cùng trái, bit 7 là **S8** ở ngoài cùng phải.

| Bước | Mẫu cảm biến điển hình | Hành động |
| --- | --- | --- |
| Vạch xuất phát | `########`, sau đó thấy vạch hẹp tại S4 hoặc S5 trong 200 ms | Đi qua vạch xuất phát, sau đó chờ ngã T |
| Ngã T | `###.....`, `####....`, `#####...`, `########` | Quay trái 90° |
| Giao lộ + | `..####..`, `##.###.#`, `########`, `.....###`, `....####` | Quay phải 90° |
| Ba trạm thao tác | Vạch ngang phủ S3–S6, sau đó bắt lại vạch thẳng đủ 200 ms | Chạy chuỗi servo |
| Vạch lệch mép | `#.......`, `.#......`, `......#.`, `.......#` | Vừa tiến vừa chỉnh về phía thấy vạch |

Firmware xác nhận hai mẫu liên tiếp theo thứ tự ngã T rồi đến `+`. Cả hai có thể tạo dải ngang giống nhau. Dải có ít nhất 5 mắt đen vẫn được nhận khi có mắt trắng xen giữa. Lúc rời vạch xuất phát, firmware chấp nhận một mẫu trống xen giữa các mẫu vạch hẹp ở S4 hoặc S5.

Xe ghi số xung hai bánh tại mép đầu của dải ngang rồi đo tiếp đến mép sau. Từ mép đầu, mốc cắt PWM bằng **120 mm cộng nửa bề rộng dải ngang**. Với bề rộng mặc định 16 mm, mốc là **128 mm**, khoảng **12,35 xung trung bình mỗi bánh**. Xe chờ 150 ms trước khi quay. PWM lúc quay nhanh là 249/255, sau đó giảm còn 197/255 khi cách góc đích khoảng 20°. Xe chờ 100 ms sau khi cắt PWM rồi tìm vạch mới trong cửa sổ ±20° quanh góc đích. Xe tiến chậm để thoát dải ngang hoặc xoay tìm vạch ở PWM khoảng 181/255. Khi thấy vạch hẹp gần giữa thanh, xe cắt PWM để xác nhận hai mẫu trước khi bám tiếp. [Phân loại mẫu cảm biến](../src/junction_detect.cpp) · [Luồng xử lý giao lộ](../src/line_tracking.cpp)

| Tham số | Mặc định | Ý nghĩa |
| --- | ---: | --- |
| `JUNCTION_PIVOT_DEGREES` | `90` | Góc quay tại chỗ |
| `JUNCTION_CONFIRM_SAMPLES` | `2` | Số mẫu liên tiếp khớp bước hiện tại |
| `JUNCTION_COOLDOWN_MS` | `1200` | Thời gian tạm bỏ qua giao lộ sau một lần quay lỗi |
| `JUNCTION_PIVOT_TIMEOUT_MS` | `2500` | Giới hạn riêng cho pha tiến đến tâm, quay và tìm lại vạch |
| `JUNCTION_REACQUIRE_SAMPLES` | `2` | Số mẫu vạch mới sau khi đạt góc |

### 7.2 Kiểm tra sau khi nạp

1. Dùng `?` qua BLE để đọc từng mắt. Đưa vạch dưới S1 rồi S8 và kiểm tra đúng thứ tự trên UART.
2. Khi ở Standby, dùng `2` / `3` / `4` để kiểm tra lần lượt các servo. Kiểm tra D-pad để xác nhận chiều hai bánh khi tiến và quay.
3. Bấm `1` để bắt đầu. Serial báo `[Encoder] START YAW=0.0 deg`. Cho xe đi qua vạch xuất phát và theo dõi `[Course] Da roi vach xuat phat`.
4. Đưa xe qua ngã T. So sánh `TARGET` trong `[Junction] MARK` với `PIVOT_START` để kiểm tra quãng tiến và quán tính sau khi cắt PWM. Dòng `[Pivot] TARGET=+90` báo khoảng `TURN=+88.7` tại mốc 20 xung chênh lệch. Đo thêm góc thực của thân xe.
5. Khi thấy `[Junction] LOCK` và thông báo hoàn thành ngã T, đưa xe tới giao lộ `+`. Kiểm tra `TARGET=-90` và dòng `LOCK` sau khi xe bắt được nhánh phải. `MS` trong `[Pivot]` là thời gian quay, còn `MS` trong `LOCK` là thời gian bắt lại vạch tính từ lúc cắt PWM ở góc đích.
6. Kiểm tra ba vạch trạm. Mỗi vạch kích hoạt chuỗi servo một lần sau khi xe bắt lại vạch thẳng. Sau trạm thứ ba, xe tiếp tục bám vạch.
7. Tạo đoạn mất vạch trên đường chính. Xe giữ lệnh bánh ở mẫu trống đầu tiên, xoay tìm sau hai mẫu trống liên tiếp và bám lại khi thấy vạch.
8. Để kiểm tra lỗi encoder, tắt nguồn trước khi tháo một tín hiệu `OUT`. Cố định xe trên giá kê trước khi cấp nguồn lại. Khi lệnh quay không đạt mốc, xe báo lỗi và giữ bước hiện tại.

---

## 8. HIỆU CHỈNH SPEED PI VÀ ENCODER

Bản photores chạy vòng ngoài PD theo tâm vạch và vòng trong PI tốc độ từng bánh ([src/speed_pid.cpp](../src/speed_pid.cpp)).

### 8.1 Vì sao phải dùng cửa sổ thời gian

Một xung encoder tương đương **10,367 mm** (chu vi bánh 66 mm ÷ 20 xung). Ở tốc độ hành thông khoảng 350 mm/s chỉ có ~34 xung/giây, tức **~0,17 xung mỗi mẫu 5 ms**. Vì vậy PI **không** thể chạy trên từng mẫu — nó chạy trên **cửa sổ 150 ms** (`SPEED_PID_WINDOW_MS`), chu kỳ tính **20 ms** (`SPEED_PID_PERIOD_MS`), tách rời khỏi nhịp lấy mẫu cảm biến.

Sai số tính bằng mm/s. Phần hiệu chỉnh bị giới hạn ở **±15%** duty mở (`SPEED_PID_OUT_LIMIT`) nên một phép đo sai không thể làm mất ổn định xe. Tích phân có giới hạn (`SPEED_PID_I_LIMIT`) và chống windup. `SPEED_PID_KD` mặc định **0** vì sai lệch tốc độ quá lượng tử, bật Kd chỉ làm tăng nhiễu.

### 8.2 Thứ tự hiệu chỉnh (làm theo thứ tự, kiểm tra lại bước trước)

**Bước 1 — Speed PI.** Chạy thẳng 1 m với cùng lệnh cho hai bánh. Đo sai lệch giữa hai bánh. Tăng `SPEED_PID_KP` cho tới khi sai lệch được sửa trong khoảng 200 ms, rồi tinh chỉnh `SPEED_PID_KI` để triệt tiêu sai lệch dư. Mục tiêu: hai bánh lệch nhau **≤ 5%**, và chạy thẳng 1 m lệch khỏi vạch **≤ 30 mm**.

**Bước 2 — Encoder trim.** Chạy thẳng 1 m, đếm xung mỗi bánh từ log Serial. 1 m tương đương `1000 / 10,367 ≈ 96,5` xung.

```text
ENCODER_PULSE_SCALE_LEFT  = xung_đo_bên_trái  / 96,5
ENCODER_PULSE_SCALE_RIGHT = xung_đo_bên_phải / 96,5
```

> `ENCODER_PULSE_SCALE_*` được áp **ngay khi đọc số xung**, nên nó sửa cả góc YAW lẫn khoảng cách đi được. Giá trị **> 1.0** dùng khi bánh đó lăn xa hơn mong đợi (bị trượt).

Sau đó dùng `SPEED_TRIM_LEFT` / `SPEED_TRIM_RIGHT` để xử lý phần trôi còn lại mà PI tỉ lệ không xử lý được. Xe trôi sang **phải** → tăng `SPEED_TRIM_LEFT`, giảm `SPEED_TRIM_RIGHT`. Kiểm tra lại `YAW` phải gần 0° (±2°) sau 1 m chạy thẳng.

**Bước 3 — Position PD.** Chỉ chỉnh sau khi Speed PI đã ổn định. Giá trị hiện tại: `LINE_STEERING_KP = 38.0`, `LINE_STEERING_KD = 14.0`, `LINE_ERROR_FILTER_ALPHA = 0.40`, `LINE_CURVE_SPEED_DROP = 14.0`.

### 8.3 Bảng tham số

| Tham số | Mặc định | Ý nghĩa |
| --- | ---: | --- |
| `SPEED_PID_KP` | `0.35` | Duty hiệu chỉnh cho mỗi mm/s sai lệch |
| `SPEED_PID_KI` | `0.08` | Khử sai lệch dư khi đi thẳng |
| `SPEED_PID_KD` | `0.00` | Để 0 (nhiễu) |
| `SPEED_PID_I_LIMIT` | `60` | Giới hạn tích phân |
| `SPEED_PID_OUT_LIMIT` | `0.15` | Giới hạn hiệu chỉnh 15% duty mở |
| `SPEED_PID_WINDOW_MS` | `150` | Cửa sổ đo xung (≥ 5 xung ở tốc độ hành thông) |
| `SPEED_PID_PERIOD_MS` | `20` | Chu kỳ tính PI |
| `ENCODER_PULSE_SCALE_LEFT/RIGHT` | `1.000` | Hiệu chỉnh sai số xung mỗi bánh |
| `SPEED_TRIM_LEFT/RIGHT` | `0` | Feed-forward chống trôi khi đi thẳng |
| `BASE_SPEED_RAMP_MS` | `600` | Tăng tốc từ 60% lên 100% khi bắt đầu dò line |

### 8.4 Sàn PWM tối thiểu

`MOTOR_MIN_PWM_LEFT` và `MOTOR_MIN_PWM_RIGHT` được hạ từ **165** xuống **150**, và sàn này được áp **sau khi** nhân hệ số công suất 88%/92%. Trước đây sàn áp trước nên duty thật của bánh trong chỉ còn 151/255 — thấp hơn mức bảo đảm bánh chạy. Nay:

- Đi thẳng: 687 → 175 → ×88% = **154/255** (không đổi).
- Bánh trong khi rẽ: duty **≥ 150/255** thật sự.
- Bánh trong còn được sàn ở `MOTOR_MIN_SPEED` (647) khi vẫn còn thấy vạch, nên không bao giờ dừng hẳn trong lúc sửa lệch.
