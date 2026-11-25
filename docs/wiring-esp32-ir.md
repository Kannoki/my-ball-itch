# ĐẤU DÂY COMBO 2: ESP32 38-PIN + CẢM BIẾN IR 8 MẮT + L298N + 3 SERVO

> **Combo:** `ESP32-WROOM-32 DevKit (38 chân)` + `Cảm biến IR 8 mắt (HY-S301 / TCRT5000)` + `L298N (4 động cơ)` + `3 Servo`  
> **Build env:** `[env:esp32-ir]` trong `platformio.ini` (`-D BOARD_ESP32_38PIN=1`)

---

## 1. NGUỒN ĐIỆN (PIN 2S 7.4V + LM2596 5.0V)

| Nhánh nguồn | Điểm kết nối | Lưu ý |
| :--- | :--- | :--- |
| **Pin 2S (+)** (7.4V – 8.4V) | L298N `12V`, LM2596 `IN+`, dây đỏ **Servo 1** | Chỉ nối Servo 1 trực tiếp nếu bản TD-8120MG chịu được 8.4V |
| **LM2596 `OUT+` (chỉnh 5.0V)** | ESP32 `5V/VIN`, dây đỏ **Servo 2 & 3**, `VCC` cảm biến | Bắt buộc chỉnh VOM về `5.0V` trước khi cắm tải |
| **GND chung** | Pin (−), LM2596 `IN−/OUT−`, ESP32, L298N, 3 Servo, Cảm biến | Bắt buộc nối chung toàn hệ thống |

---

## 2. ĐẤU DÂY CẢM BIẾN DÒ LINE IR 8 MẮT

Nhìn theo hướng xe chạy tới: `S1 (D1)` ngoài cùng bên **TRÁI** → `S8 (D8)` ngoài cùng bên **PHẢI**.

| Chân cảm biến | GPIO ESP32 38-pin | Ý nghĩa |
| :--- | :--- | :--- |
| **VCC** | **5V / VIN** (từ LM2596 5.0V) hoặc **3V3** | Nguồn nuôi thanh cảm biến |
| **GND** | **GND** | Mass chung |
| **IR** (hoặc **1R**) | **GPIO 4** (hoặc nối thẳng **VCC**) | **BẮT BUỘC NỐI** để bật 8 LED phát hồng ngoại |
| **D1 (S1)** | **GPIO 23** | Mắt ngoài cùng bên TRÁI |
| **D2 (S2)** | **GPIO 22** | Mắt lệch trái mức 2 |
| **D3 (S3)** | **GPIO 21** | Mắt lệch trái mức 3 |
| **D4 (S4)** | **GPIO 19** | Mắt giữa bên TRÁI |
| **D5 (S5)** | **GPIO 18** | Mắt giữa bên PHẢI |
| **D6 (S6)** | **GPIO 5** | Mắt lệch phải mức 3 |
| **D7 (S7)** | **GPIO 17** | Mắt lệch phải mức 2 |
| **D8 (S8)** | **GPIO 16** | Mắt ngoài cùng bên PHẢI |

- **An toàn chân ESP32 38-pin:** Cụm `GPIO 23, 22, 21, 19, 18, 5, 17, 16, 4` nằm gọn ở dãy phải, tránh Flash (`6–11`), tránh `UART0` (`1/3`) và hỗ trợ `INPUT_PULLUP` (`P`).
- **Chân `IR` / `1R`:** Nếu bỏ trống, 8 LED IR tắt → báo **FULL ĐEN**. Dùng camera điện thoại soi để kiểm tra ánh sáng mờ tím hồng.
- **Mức logic (`LINE_ACTIVE_STATE`):** Mặc định `LOW` (vạch đen = `0`, nền trắng = `1`). Gửi `I` qua BLE UART để đảo nhanh nếu ngược.

---

## 3. ĐẤU DÂY MẠCH ĐỘNG CƠ L298N (4 ĐỘNG CƠ)

> Tháo 2 jumper `ENA`, `ENB` để nhận PWM từ ESP32 38-pin. Để trống cọc `5V` trên L298N (vẫn giữ jumper `5V Enable`).

| Chân L298N | GPIO ESP32 38-pin | Chức năng |
| :--- | :--- | :--- |
| **12V** | Pin 2S (+) | Nguồn động lực 4 động cơ |
| **GND** | GND chung | Mass chung |
| **5V** | *(ĐỂ TRỐNG)* | Không nối vào ESP32 / LM2596 |
| **ENA** | **GPIO 32** | PWM tốc độ 2 động cơ **PHẢI** (`OUT1 & OUT2`) |
| **IN1** | **GPIO 33** | Chiều quay động cơ **PHẢI** 1 |
| **IN2** | **GPIO 25** | Chiều quay động cơ **PHẢI** 2 |
| **IN3** | **GPIO 26** | Chiều quay động cơ **TRÁI** 1 |
| **IN4** | **GPIO 27** | Chiều quay động cơ **TRÁI** 2 |
| **ENB** | **GPIO 14** | PWM tốc độ 2 động cơ **TRÁI** (`OUT3 & OUT4`) |

- **2 động cơ TRÁI** đấu song song vào `OUT3 & OUT4`; **2 động cơ PHẢI** đấu song song vào `OUT1 & OUT2` (nếu đấu ngược hai bên, đặt `SWAP_LEFT_RIGHT_MOTORS = true` trong `include/config.h`).
- Cụm `GPIO 32, 33, 25, 26, 27, 14` nằm liền mạch ở dãy trái board 38-pin (tránh chân strapping `GPIO 12`).

---

## 4. ĐẤU DÂY 3 ĐỘNG CƠ SERVO

| Servo | Dây tín hiệu (Cam/Vàng) | Dây nguồn (Đỏ) | Dây GND (Nâu/Đen) |
| :--- | :--- | :--- | :--- |
| **Servo 1 (TD-8120MG)** | **GPIO 13** | Pin 2S (+) (hoặc nguồn ổn áp phù hợp) | GND chung |
| **Servo 2** | **GPIO 15** | LM2596 `OUT+` (5.0V) | GND chung |
| **Servo 3** | **GPIO 2** | LM2596 `OUT+` (5.0V) | GND chung |

> Lưu ý: Không kéo ngoài `GPIO 2` lên mức cao trong lúc đang nạp code qua UART.

---

## 5. LẮP ĐẶT & NẠP CODE

- **Khoảng cách cảm biến xuống sàn:** `0,5 mm` | **Bước tâm 2 mắt:** `6 mm` (dài `42 mm`) | **Vạch đen:** `16 mm`.
- **Biên dịch & nạp:**

  ```bash
  pio run -e esp32-ir -t upload
  pio device monitor -b 115200
  ```

- **Kiểm tra nhanh:** Mở BLE UART (`Adafruit Bluefruit LE Connect`) → gửi `?` để kiểm tra cảm biến, dùng Control Pad nút `1` (dò line), `2/3/4` (test Servo 1/2/3), D-Pad (chạy thử động cơ ở Standby).
