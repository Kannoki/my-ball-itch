# ĐẤU DÂY COMBO 1: ESP32-S3 + CẢM BIẾN IR 8 MẮT + L298N + 3 SERVO

> **Combo:** `ESP32-S3 WROOM-1 (DevKitC-1)` + `Cảm biến IR 8 mắt (HY-S301 / TCRT5000)` + `L298N (4 động cơ)` + `3 Servo`  
> **Build env:** `[env:s3-ir]` trong `platformio.ini`

---

## 1. NGUỒN ĐIỆN (PIN 2S 7.4V + LM2596 5.0V)

| Nhánh nguồn | Điểm kết nối | Lưu ý |
| :--- | :--- | :--- |
| **Pin 2S (+)** (7.4V – 8.4V) | L298N `12V`, LM2596 `IN+`, dây đỏ **Servo 1** | Chỉ nối Servo 1 trực tiếp nếu bản TD-8120MG chịu được 8.4V |
| **LM2596 `OUT+` (chỉnh 5.0V)** | ESP32-S3 `5V/VIN`, dây đỏ **Servo 2 & 3**, `VCC` cảm biến | Bắt buộc chỉnh VOM về `5.0V` trước khi cắm tải |
| **GND chung** | Pin (−), LM2596 `IN−/OUT−`, ESP32-S3, L298N, 3 Servo, Cảm biến | Bắt buộc nối chung toàn hệ thống |

---

## 2. ĐẤU DÂY CẢM BIẾN DÒ LINE IR 8 MẮT

Trên xe hiện tại, kiểm tra bằng Serial xác nhận mắt ngoài cùng bên **TRÁI** kích hoạt kênh `S8 / GPIO 18`. Cấu hình `s3-ir` dùng `LINE_SENSOR_REVERSED = 1`, đọc **S8 → S1** theo hướng **trái → phải khi xe tiến**. Bảng dưới giữ tên kênh đấu dây; nếu lắp thanh theo chiều ngược lại thì đổi cấu hình về `0`.

| Chân cảm biến | GPIO ESP32-S3 | Ý nghĩa |
| :--- | :--- | :--- |
| **VCC** | **5V / VIN** (từ LM2596 5.0V) hoặc **3V3** | Nguồn nuôi thanh cảm biến |
| **GND** | **GND** | Mass chung |
| **IR** (hoặc **1R**) | **GPIO 1** (hoặc nối thẳng **VCC**) | **BẮT BUỘC NỐI** để bật 8 LED phát hồng ngoại |
| **D1 (S1)** | **GPIO 4** | Mắt ngoài cùng bên PHẢI |
| **D2 (S2)** | **GPIO 5** | Mắt thứ 2 từ phải |
| **D3 (S3)** | **GPIO 6** | Mắt thứ 3 từ phải |
| **D4 (S4)** | **GPIO 7** | Mắt giữa bên PHẢI |
| **D5 (S5)** | **GPIO 15** | Mắt giữa bên TRÁI |
| **D6 (S6)** | **GPIO 16** | Mắt thứ 3 từ trái |
| **D7 (S7)** | **GPIO 17** | Mắt thứ 2 từ trái |
| **D8 (S8)** | **GPIO 18** | Mắt ngoài cùng bên TRÁI |

- **Chân `IR` / `1R`:** Nếu để trống, 8 LED IR tắt → cảm biến báo **FULL ĐEN**. Dùng camera điện thoại soi để thấy ánh sáng mờ tím hồng.
- **Mức logic (`LINE_ACTIVE_STATE`):** Mặc định `HIGH` cho thanh IR 8 mắt hiện dùng: đã đo nền trắng cho `GPIO: 00000000`, nên LOW là nền và HIGH được dùng làm line. Kiểm tra thêm trên vạch đen; gửi `I` qua BLE UART nếu module có logic ngược.

---

## 3. ĐẤU DÂY MẠCH ĐỘNG CƠ L298N (4 ĐỘNG CƠ)

> Tháo 2 jumper `ENA`, `ENB` để nhận PWM từ ESP32-S3. Để trống cọc `5V` trên L298N (vẫn giữ jumper `5V Enable`).

| Chân L298N | GPIO ESP32-S3 | Chức năng |
| :--- | :--- | :--- |
| **12V** | Pin 2S (+) | Nguồn động lực 4 động cơ |
| **GND** | GND chung | Mass chung |
| **5V** | *(ĐỂ TRỐNG)* | Không nối vào ESP32-S3 / LM2596 |
| **ENA** | **GPIO 8** | PWM tốc độ 2 động cơ **PHẢI** (`OUT1 & OUT2`) |
| **IN1** | **GPIO 9** | Chiều quay động cơ **PHẢI** 1 |
| **IN2** | **GPIO 10** | Chiều quay động cơ **PHẢI** 2 |
| **IN3** | **GPIO 11** | Chiều quay động cơ **TRÁI** 1 |
| **IN4** | **GPIO 12** | Chiều quay động cơ **TRÁI** 2 |
| **ENB** | **GPIO 13** | PWM tốc độ 2 động cơ **TRÁI** (`OUT3 & OUT4`) |

- **2 động cơ TRÁI** đấu song song vào `OUT3 & OUT4`; **2 động cơ PHẢI** đấu song song vào `OUT1 & OUT2` (nếu đấu ngược hai bên, đặt `SWAP_LEFT_RIGHT_MOTORS = true` trong `include/config.h`).

---

## 4. ĐẤU DÂY 3 ĐỘNG CƠ SERVO

| Servo | Dây tín hiệu (Cam/Vàng) | Dây nguồn (Đỏ) | Dây GND (Nâu/Đen) |
| :--- | :--- | :--- | :--- |
| **Servo 1 (TD-8120MG)** | **GPIO 14** | Pin 2S (+) (hoặc nguồn ổn áp phù hợp) | GND chung |
| **Servo 2** | **GPIO 21** | LM2596 `OUT+` (5.0V) | GND chung |
| **Servo 3** | **GPIO 47** | LM2596 `OUT+` (5.0V) | GND chung |

---

## 5. LẮP ĐẶT & NẠP CODE

- **Khoảng cách cảm biến xuống sàn:** `0,5 mm` | **Bước tâm 2 mắt:** `6 mm` (dài `42 mm`) | **Vạch đen:** `16 mm`.
- **Biên dịch & nạp:**

  ```bash
  pio run -e s3-ir -t upload
  pio device monitor -b 115200
  ```

- **Kiểm tra nhanh:** Mở BLE UART (`Adafruit Bluefruit LE Connect`) → gửi `?` để kiểm tra cảm biến, dùng Control Pad nút `1` (dò line), `2/3/4` (test Servo 1/2/3), D-Pad (chạy thử động cơ ở Standby).
