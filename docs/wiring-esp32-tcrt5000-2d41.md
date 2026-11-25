# ĐẤU DÂY COMBO 4: ESP32 38-PIN + CẢM BIẾN TCRT5000 2D4.1 (5 MẮT) + L298N + 3 SERVO

> **Combo:** `ESP32-WROOM-32 DevKit (38 chân)` + `Cảm biến dò line 5 mắt TCRT5000 2D4.1 (header 7 chân)` + `L298N (4 động cơ)` + `3 Servo`  
> **Build env:** `[env:esp32-tcrt5000-2d41]` trong `platformio.ini` (`-D BOARD_ESP32_38PIN=1`, `-D SENSOR_TCRT5000_5CH=1`)

---

## 1. NGUỒN ĐIỆN (PIN 2S 7.4V + LM2596 5.0V)

| Nhánh nguồn | Điểm kết nối | Lưu ý |
| :--- | :--- | :--- |
| **Pin 2S (+)** (7.4V – 8.4V) | L298N `12V`, LM2596 `IN+`, dây đỏ **Servo 1** | Chỉ nối Servo 1 trực tiếp nếu bản TD-8120MG chịu được 8.4V |
| **LM2596 `OUT+` (chỉnh 5.0V)** | ESP32 `5V/VIN`, dây đỏ **Servo 2 & 3** | Bắt buộc chỉnh VOM về `5.0V` trước khi cắm tải |
| **Nguồn 3.3V** (khuyên dùng cho 2D4.1) | `VCC` cảm biến TCRT5000 2D4.1 | Cấp `3V3` từ ESP32 giúp ngõ ra `OUT1–OUT5` an toàn đúng mức logic 3.3V (module hỗ trợ `3.3V–5V`) |
| **GND chung** | Pin (−), LM2596 `IN−/OUT−`, ESP32, L298N, 3 Servo, Cảm biến | Bắt buộc nối chung toàn hệ thống |

---

## 2. ĐẤU DÂY CẢM BIẾN TCRT5000 2D4.1 (5 MẮT, 7 CHÂN)

Cụm dây tín hiệu dùng đúng 5 chân đầu trong dãy cảm biến bên phải của [docs/wiring-esp32-ir.md](wiring-esp32-ir.md) (`GPIO 23, 22, 21, 19, 18`).  
Nhìn theo hướng xe chạy tới: `S1 (OUT1)` ngoài cùng bên **TRÁI** → `S3 (OUT3)` ở **GIỮA** → `S5 (OUT5)` ngoài cùng bên **PHẢI**.

| Chân trên module 2D4.1 | GPIO ESP32 38-pin | Ý nghĩa |
| :--- | :--- | :--- |
| **VCC** | **3V3** (khuyên dùng) hoặc **5V** | Nguồn nuôi TCRT5000 2D4.1 (`3.3V–5V`) |
| **GND** | **GND** | Mass chung |
| **OUT1 / S1** | **GPIO 23** | Mắt ngoài cùng bên TRÁI |
| **OUT2 / S2** | **GPIO 22** | Mắt lệch trái |
| **OUT3 / S3** | **GPIO 21** | Mắt ở GIỮA |
| **OUT4 / S4** | **GPIO 19** | Mắt lệch phải |
| **OUT5 / S5** | **GPIO 18** | Mắt ngoài cùng bên PHẢI |

> *(Module TCRT5000 2D4.1 chỉ có 7 chân `VCC, GND, OUT1..OUT5`, **không có chân `IR`/`1R`** — 5 LED hồng ngoại tự sáng khi cấp nguồn. Các chân `GPIO 5, 17, 16, 4` để trống.)*

### Lưu ý riêng cho TCRT5000 2D4.1

- **Kiểm tra nhãn chân dưới mặt sau PCB:** Thứ tự 7 chân hàn dưới mạch có thể ghi `VCC, GND, OUT1..OUT5` hoặc `IN1..IN5` tùy đợt mạch — đối chiếu đúng chữ in trên bo trước khi cắm dây.
- **Chế độ bám mặc định (`EDGE_CENTER_LINE`):** Firmware tự đặt chế độ **Bám giữa vạch đen** (`S3 = 0.0`) phù hợp với 5 mắt cách nhau `~18 mm`. Nếu chạy sân ranh giới Đen–Trắng, bấm **`V`** trong BLE UART để chuyển sang chế độ bám viền.
- **Mức logic (`LINE_ACTIVE_STATE`):** Mặc định `LOW` (vạch đen = `0`, nền trắng = `1`). Nếu mạch xuất ngược, gửi **`I`** qua BLE UART để đảo nhanh hoặc chỉnh biến trở độ nhạy trên bo.

---

## 3. ĐẤU DÂY MẠCH ĐỘNG CƠ L298N (GIỐNG `docs/wiring-esp32-ir.md`)

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

---

## 4. ĐẤU DÂY 3 ĐỘNG CƠ SERVO (GIỐNG `docs/wiring-esp32-ir.md`)

| Servo | Dây tín hiệu (Cam/Vàng) | Dây nguồn (Đỏ) | Dây GND (Nâu/Đen) |
| :--- | :--- | :--- | :--- |
| **Servo 1 (TD-8120MG)** | **GPIO 13** | Pin 2S (+) (hoặc nguồn ổn áp phù hợp) | GND chung |
| **Servo 2** | **GPIO 15** | LM2596 `OUT+` (5.0V) | GND chung |
| **Servo 3** | **GPIO 2** | LM2596 `OUT+` (5.0V) | GND chung |

> Lưu ý: Không kéo ngoài `GPIO 2` lên mức cao trong lúc đang nạp code qua UART.

---

## 5. LẮP ĐẶT & NẠP CODE

- **Khoảng cách mắt TCRT5000 xuống sàn:** `2–5 mm` (tối ưu để phân biệt rõ trắng/đen mà không cạ sàn), chỉnh biến trở trên bo sao cho đèn báo trên mạch chuyển dứt khoát khi qua vạch đen.
- **Biên dịch & nạp:**

  ```bash
  pio run -e esp32-tcrt5000-2d41 -t upload
  pio device monitor -b 115200
  ```

- **Kiểm tra nhanh:** Mở BLE UART (`Adafruit Bluefruit LE Connect`) → gửi `?` để kiểm tra 5 mắt `S1..S5`, `I` để đảo logic nếu ngược, `V` để đổi chế độ bám giữa / bám viền, và dùng Control Pad nút `1` (chạy), `2/3/4` (test Servo 1/2/3), D-Pad (chạy thử động cơ ở Standby).
