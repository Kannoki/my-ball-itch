# HƯỚNG DẪN ĐẤU DÂY VÀ CẤU HÌNH PHẦN CỨNG (WIRING GUIDE)

> **Dự án:** Robot Dò Line 8 Mắt + 4 Động Cơ (L298N) + 3 Servo + Điều Khiển Bluetooth LE (Adafruit Bluefruit)  
> **Vi điều khiển:** ESP32-S3 WROOM-1 (DevKitC-1)

---

## 1. SƠ ĐỒ KHỐI KIẾN TRÚC HỆ THỐNG

```
Pin 2S (7,4 V danh định)
├── Cực (+) ── Servo 1 TD-8120MG (dây đỏ, cấp trực tiếp)
├── Cực (+) ── L298N (cọc 12V, cấp cho 4 động cơ)
├── Cực (+) ── LM2596 (IN+)
│              └── OUT+ 5,0 V ── ESP32-S3, Servo 2, Servo 3, cảm biến dò line
└── Cực (−) ── GND chung của Servo 1, L298N, LM2596 (IN−/OUT−),
              ESP32-S3, Servo 2, Servo 3 và cảm biến dò line

Tín hiệu ESP32-S3
├── GPIO 14, 21, 47 ── PWM cho Servo 1, 2, 3 tương ứng
├── GPIO 8–13 ── PWM và chiều quay cho L298N
└── GPIO 4–7, 15–18 ── tín hiệu từ 8 mắt dò line
```

---

## 2. NGUỒN ĐIỆN VÀ NGUYÊN TẮC AN TOÀN VỚI LM2596 (CỰC KỲ QUAN TRỌNG)

### 2.1. Cấu hình Mạch Hạ Áp Buck LM2596 (5V - 3A)
Mạch LM2596 cấp nguồn 5 V cho ESP32-S3, Servo 2, Servo 3 và cảm biến dò line.

1. **Đấu ngõ vào (Input):**
   - Nối chân **IN+** của LM2596 vào cực dương (+) của pin 2S 7,4 V.
   - Chân **IN-** của LM2596: Nối vào cực Âm (-) của Pin nguồn chính.
2. **Cân chỉnh điện áp ngõ ra (BẮT BUỘC TRƯỚC KHI CẮM TẢI):**
   - Trước khi nối tải vào nhánh 5 V, dùng đồng hồ vạn năng đo điện áp giữa hai chân **OUT+** và **OUT-**.
   - Dùng tua vít nhỏ xoay nhẹ biến trở tinh chỉnh màu xanh trên mạch LM2596 ngược chiều kim đồng hồ cho đến khi màn hình VOM hiển thị chính xác **`5.0V`** (hoặc dao động từ `5.0V - 5.1V`).
3. **Đấu ngõ ra (Output 5V 3A):**
   - Chân **OUT+ (5V)**: Nối chung cấp nguồn cho 3 thành phần:
     + Chân **5V (hoặc VIN)** của board ESP32-S3.
     + Dây đỏ cấp nguồn của **Servo 2** và **Servo 3**.
     + Chân **VCC** của module cảm biến dò line 8 mắt.
   - Chân **OUT- (GND)**: Nối vào hệ thống Mass chung (Common GND).

### 2.2. Trạng thái cọc 5V của mạch L298N (LƯU Ý ĐẶC BIỆT)
> [!IMPORTANT]
> **CỌC 5V TRÊN MẠCH L298N PHẢI ĐỂ TRỐNG (KHÔNG NỐI VÀO ESP32)!**
> - Để riêng hai nguồn 5 V của IC 78M05 trên L298N và LM2596 để tránh dòng chạy ngược khi hai đầu ra chênh điện áp.
> - **Jumper 5V Enable trên L298N:** **Vẫn cắm nguyên jumper này** để IC 78M05 tự lấy nguồn từ cọc 12V nuôi các cổng logic bên trong L298N.
> - **Cọc GND của L298N:** Bắt buộc nối chung với **GND của LM2596 và GND của ESP32-S3** để đảm bảo mức logic 0V đồng nhất.

---

## 3. BẢNG ĐẤU NỐI DÂY CHI TIẾT (ESP32-S3 PINOUT)

### 3.1. Bảng Đấu Nối Mạch Hạ Áp LM2596

| Chân LM2596 | Điểm kết nối | Chức năng |
| :--- | :--- | :--- |
| **IN +** | Cực dương (+) của pin 2S 7,4 V | Nguồn đầu vào |
| **IN -** | Cực Âm (-) của Pin | Nguồn đầu vào (Mass) |
| **OUT +** (5.0V) | **5V/VIN** của ESP32-S3, **VCC** của Servo 2 và Servo 3, **VCC** của cảm biến dò line | Nguồn 5 V |
| **OUT -** (GND) | **GND** của ESP32-S3 + **GND** 3 Servo + **GND** L298N + Cảm biến | Chung Mass toàn hệ thống |

---

### 3.2. Đấu nối Mạch Điều Khiển Động Cơ L298N (Đúng chuẩn thực tế)

> **Lưu ý:** Tháo 2 nắp jumper ở chân `ENA` và `ENB` trên L298N để cắm dây bẹ điều khiển tốc độ PWM bằng ESP32-S3.

| Chân L298N | Chân ESP32-S3 | Chức năng | Ghi chú |
| :--- | :--- | :--- | :--- |
| **12V** | Cực dương (+) của pin 2S 7,4 V | Nguồn động lực cấp cho 4 động cơ | Điện áp pin 2S |
| **GND** | **GND** hệ thống / Pin (-) | Nối Mass chung (Common Ground) | Bắt buộc chung GND |
| **5V** | *(ĐỂ TRỐNG)* | Không nối vào ESP32 | ESP32 đã dùng 5V từ LM2596 |
| **ENA** | **GPIO 8** | Điều khiển tốc độ (PWM) 2 động cơ PHẢI | Rút jumper ENA trên L298N |
| **IN1** | **GPIO 9** | Điều khiển chiều quay PHẢI 1 | Logic HIGH/LOW |
| **IN2** | **GPIO 10** | Điều khiển chiều quay PHẢI 2 | Logic HIGH/LOW |
| **IN3** | **GPIO 11** | Điều khiển chiều quay TRÁI 1 | Logic HIGH/LOW |
| **IN4** | **GPIO 12** | Điều khiển chiều quay TRÁI 2 | Logic HIGH/LOW |
| **ENB** | **GPIO 13** | Điều khiển tốc độ (PWM) 2 động cơ TRÁI | Rút jumper ENB trên L298N |

#### Đấu 4 Động cơ vào cọc OUT của L298N:

* **2 Động cơ bên TRÁI (OUT3 & OUT4):**
  - Đấu song song 2 động cơ bên TRÁI vào cọc **OUT3** và **OUT4**.
  - Kẹp chung 2 dây đỏ vào OUT3, 2 dây đen vào OUT4.

* **2 Động cơ bên PHẢI (OUT1 & OUT2):**
  - Đấu song song 2 động cơ bên PHẢI vào cọc **OUT1** và **OUT2**.
  - Kẹp chung 2 dây của 2 động cơ bên Phải vào OUT1 và OUT2. (Vì động cơ bên phải lắp đối xứng cơ học 180° nên phần mềm đã tự động đảo cờ `INVERT_RIGHT_MOTORS = true` để xe đi đúng chiều).

> [!NOTE]
> `INVERT_LEFT_MOTORS` / `INVERT_RIGHT_MOTORS` / `SWAP_LEFT_RIGHT_MOTORS` chỉ ảnh hưởng **chiều quay vật lý của từng bên**,
> được áp dụng ở tầng ghi GPIO nên **không làm sai hướng xoay** của xe (`runTurnLeft` luôn xoay sang trái).
> Nếu đấu sai, có thể sửa ngay trên app bằng lệnh `XI` / `XR` / `XS` — xem [mục 6.1](#61-quy-trình-chẩn-đoán-nhanh-qua-ble-không-cần-nạp-lại-firmware).

Nếu dây bên trái đã vào `OUT1/OUT2` và dây bên phải vào `OUT3/OUT4`, đặt `SWAP_LEFT_RIGHT_MOTORS` thành `true` trong `include/config.h`. Thử mũi tên trái và phải ở Standby để kiểm tra hướng quay thực tế.

---

### 3.2. Đấu nối Thanh Cảm Biến Dò Line 8 Mắt (8 Sensor Array)

> Tài liệu đấu nối riêng theo từng combo phần cứng:
> - **ESP32-S3 + Cảm biến IR 8 mắt:** [docs/wiring-s3-ir.md](wiring-s3-ir.md)
> - **ESP32 38-pin + Cảm biến IR 8 mắt:** [docs/wiring-esp32-ir.md](wiring-esp32-ir.md)
> - **ESP32 38-pin + Cảm biến Quang trở 8 mắt:** [docs/wiring-esp32-photores.md](wiring-esp32-photores.md)
> - **ESP32 38-pin + Cảm biến TCRT5000 2D4.1 (5 mắt):** [docs/wiring-esp32-tcrt5000-2d41.md](wiring-esp32-tcrt5000-2d41.md)

Thanh cảm biến xếp từ trái sang phải: `S1 -> S2 -> S3 -> S4 -> S5 -> S6 -> S7 -> S8`.

| Chân Cảm biến | Chân ESP32-S3 | Vị trí / Ý nghĩa |
| :--- | :--- | :--- |
| **VCC** | **5V** (hoặc 3.3V) | Lấy từ 5V của ESP32 hoặc LM2596 |
| **GND** | **GND** | Nối chung Mass |
| **IR (hoặc 1R)** | **VCC** hoặc **GPIO 1** | **BẮT BUỘC NỐI:** Kích hoạt 8 đèn LED phát hồng ngoại (để trống đèn phát bị TẮT -> báo Full Đen) |
| **D1 (S1)** | **GPIO 4** | Mắt ngoài cùng bên TRÁI (Lệch cực trái) |
| **D2 (S2)** | **GPIO 5** | Mắt lệch trái mức 2 |
| **D3 (S3)** | **GPIO 6** | Mắt lệch trái mức 3 |
| **D4 (S4)** | **GPIO 7** | Mắt giữa bên TRÁI |
| **D5 (S5)** | **GPIO 15** | Mắt giữa bên PHẢI |
| **D6 (S6)** | **GPIO 16** | Mắt lệch phải mức 3 |
| **D7 (S7)** | **GPIO 17** | Mắt lệch phải mức 2 |
| **D8 (S8)** | **GPIO 18** | Mắt ngoài cùng bên PHẢI (Lệch cực phải) |

> [!WARNING]
> **VỀ CHÂN "1R" (CHÍNH LÀ CHỮ "IR" - INFRARED ENABLE):**
> Ký hiệu trên mạch in thường là **`IR`** (nhìn rất giống **`1R`**). Đây là chân cấp nguồn bật/tắt toàn bộ 8 bóng LED phát hồng ngoại (IR Transmitter).
> - **Khi để trống chân IR:** 8 bóng LED phát hồng ngoại bị **TẮT HOÀN TOÀN**. Cảm biến không nhận được bất kỳ tia phản xạ nào nên luôn hiểu là **TỐI ĐEN (FULL ĐEN)** dù bạn đặt trên nền trắng tinh!
> - **Cách nối:**
>   - Cách 1 (Khuyên dùng): Nối thẳng chân **IR** vào chân **VCC** (5V hoặc 3.3V) để 8 đèn phát luôn sáng.
>   - Cách 2: Nối chân **IR** vào chân **GPIO 1** của ESP32-S3 (firmware đã tự động cấu hình xuất mức `HIGH` để bật đèn).
> - **Cách kiểm tra:** Sau khi nối IR vào nguồn, bật camera điện thoại soi vào 8 mắt phát: bạn sẽ thấy 8 bóng phát ánh sáng mờ màu tím hồng!

> **Cách kiểm tra mức logic:** IR 8 mắt hiện mặc định `LINE_ACTIVE_STATE HIGH`, dựa trên mẫu nền trắng đã đo là `GPIO: 00000000`. TCRT5000 2D4.1 5 mắt vẫn mặc định LOW. Kiểm tra trên cả nền trắng và vạch đen; dùng lệnh `I` để đảo nếu module khác logic, hoặc cấu hình trong `include/config.h`:
>
> ```cpp
> #define LINE_ACTIVE_STATE HIGH
> ```

#### Kích thước lắp đặt & hình học thanh cảm biến

- **Khoảng cách từ cảm biến xuống mặt sàn:** `0,5 mm` (lưu ý lắp song song mặt sàn để tránh cạ xuống đường chạy).
- **Khoảng cách giữa các mắt đọc (bước tâm–tâm):** `6 mm` (tổng khoảng cách từ tâm `S1` đến tâm `S8` là `42 mm`).
- **Bề ngang vạch line đen:** `16 mm`.

| Mắt từ trái sang phải | S1 | S2 | S3 | S4 | S5 | S6 | S7 | S8 |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Vị trí tâm mắt so với tâm dãy (mm) | −21 | −15 | −9 | −3 | +3 | +9 | +15 | +21 |

---

### 3.3. Đấu nối 3 động cơ servo

Mỗi servo có 3 dây màu:

- **Dây Nâu / Đen:** GND
- **Dây đỏ** nhận điện áp theo nhánh nguồn của từng servo.
- **Dây Cam / Vàng:** Signal (Xung PWM điều khiển)

| Thiết bị | Dây tín hiệu (cam/vàng) | Dây nguồn (đỏ) | Dây GND (nâu/đen) |
| :--- | :--- | :--- | :--- |
| **Servo 1 TD-8120MG** | **GPIO 14** | Cực (+) pin 2S 7,4 V, cấp trực tiếp | Cực (−) pin, nối chung GND ESP32-S3 |
| **Servo 2** | **GPIO 21** | LM2596 OUT+ (5 V) | LM2596 OUT− |
| **Servo 3** | **GPIO 47** | LM2596 OUT+ (5 V) | LM2596 OUT− |

Pin LiPo 2S có điện áp danh định **7,4 V** và đạt **8,4 V** khi sạc đầy. Chỉ đấu Servo 1 trực tiếp nếu nhãn hoặc datasheet của phiên bản đang dùng cho phép **8,4 V**. Nếu giới hạn thấp hơn, dùng nguồn ổn áp trong dải điện áp của servo. [Hướng dẫn pin LiPo](https://www.spektrumrc.com/on/demandware.static/Sites-spektrum-us-Site/Sites-horizon-master/default/Manuals/TRA6407X-Manual_EN.pdf) · [Datasheet TD-8120MG bản 4,8–8,4 V](https://www.tinytronics.nl/product_files/002172_Data%20Sheet%20of%20TD-8120MG%20Digital%20Servo%20Motor.pdf)

*(Các chân GPIO 14, 21, 47 được chọn là chân Digital I/O an toàn, không bị ảnh hưởng bởi quá trình boot hay nạp flash của ESP32-S3).*

---

## 4. HƯỚNG DẪN ĐIỀU KHIỂN QUA APP "ADAFRUIT BLUEFRUIT LE CONNECT"

Dự án tích hợp giao thức Nordic UART Service (NUS) tương thích 100% với ứng dụng chính thức của Adafruit:

1. **Cài đặt ứng dụng:**
   - **iOS:** Tải `Adafruit Bluefruit LE Connect` trên Apple App Store.
   - **Android:** Tải `Adafruit Bluefruit LE Connect` trên Google Play Store.
2. **Kết nối:**
   - Bật Bluetooth và Định vị trên điện thoại.
   - Mở ứng dụng, quét tìm thiết bị theo tên **`BLE_DEVICE_NAME`** cấu hình trong `include/config.h` (không cần nhập mật khẩu, bấm là kết nối ngay).
   - Bấm **Connect**.
3. **Các chế độ điều khiển:**
   - **Mục Controller -> Control Pad:**
     - **Nút 1:** **BẬT / TẮT DÒ VIỀN (START / STOP):**
       + Khi vừa bật nguồn: Xe đứng yên chờ ở chế độ Standby.
       + Bấm **Nút 1 lần đầu**: Xe **BẮT ĐẦU CHẠY** bám theo viền mép giữa màu Đen và Trắng.
       + Bấm **Nút 1 lần nữa**: Xe **DỪNG NGAY LẬP TỨC** (phanh dừng khẩn cấp).
     - **Nút 2:** Điều khiển **Servo 1 (TD-8120MG trên GPIO 14)** gạt kịch 100% hành trình (270 độ / 2500µs).
     - **Nút 3:** Điều khiển **Servo 2 (GPIO 21)** xoay góc (0° <-> 180°).
     - **Nút 4:** Điều khiển **Servo 3 (GPIO 47)** xoay góc (0° <-> 180°).
     - **D-Pad (Mũi tên UP / DOWN / LEFT / RIGHT):** Khi ở chế độ MANUAL BLE:
       - Nhấn giữ UP: Tiến về phía trước.
       - Nhấn giữ DOWN: Lùi xe.
       - Nhấn giữ LEFT: Xoay tại chỗ sang trái.
       - Nhấn giữ RIGHT: Xoay tại chỗ sang phải.
       - Thả tay: Xe tự động dừng ngay lập tức.
   - **Mục UART Terminal (Theo dõi thời gian thực & Cân chỉnh cảm biến):**
     - Màn hình hiển thị liên tục mỗi 300ms trạng thái 8 mắt:
       `R:00011000 [...##...] U:-.-##-.- FORWARD`
     - Các lệnh gửi nhanh:
       - `?` hoặc `H`: In báo cáo chẩn đoán chi tiết từng chân GPIO và trạng thái logic.
       - `I`: Đảo mức logic nhận diện vạch đen (`HIGH` <-> `LOW`).
       - `P`: Bật / Tắt trở kéo `INPUT_PULLUP` trên 8 chân GPIO.
       - `T`: Bật / Tắt luồng telemetry tự động.
       - `1`: Bật / Tắt dò viền (tương đương Nút 1).
       - `2`: Gạt Servo 1 (TD-8120MG trên GPIO 14, tương đương Nút 2).
       - `3`: Gạt Servo 2 (GPIO 21, tương đương Nút 3).
       - `4`: Gạt Servo 3 (GPIO 47, tương đương Nút 4).
       - `F`, `B`, `L`, `R`, `S`: Tiến, lùi, rẽ, dừng xe khi ở Standby.
       - `2 <goc>` hoặc `s1 <goc>`: Đặt góc/xung cụ thể cho Servo 1 (VD: `2 2500` hoặc `2 270`).
       - `3 <goc>` hoặc `s2 <goc>`: Đặt góc cụ thể cho Servo 2 (VD: `3 90`, `3 180`).
       - `4 <goc>` hoặc `s3 <goc>`: Đặt góc cụ thể cho Servo 3 (VD: `4 90`, `4 180`).

---

## 5. CHECKLIST TRƯỚC KHI BẬT NGUỒN (PRE-FLIGHT CHECK)

1. [ ] **Kiểm tra hai nhánh nguồn.** LM2596 đã chỉnh về **5,0 V** trước khi cắm ESP32-S3, Servo 2, Servo 3 và cảm biến. Dây đỏ Servo 1 đi trực tiếp vào cực (+) pin 2S sau khi xác nhận đúng phiên bản chịu được **8,4 V**.
2. [ ] **Cọc 5V của L298N để trống:** Đảm bảo không nối cọc 5V của L298N vào chân 5V của LM2596 hay ESP32.
3. [ ] **Đấu chung MASS (Common GND):** Đã nối chung tất cả chân GND (Pin Âm, LM2596 OUT-, ESP32-S3 GND, L298N GND, Cảm biến Line GND, Servo GND).
4. [ ] **Jumper ENA & ENB:** Đã rút 2 nắp jumper trên L298N và cắm dây bẹ tới GPIO 8 và GPIO 13.
5. [ ] **Kiểm tra hướng động cơ.** Đấu mặc định trái vào OUT3/OUT4 và phải vào OUT1/OUT2. Khi thử lệnh tiến trong Standby, các bánh ở cả hai bên cùng đẩy xe về trước. Dùng `INVERT_LEFT_MOTORS` hoặc `INVERT_RIGHT_MOTORS` để chỉnh chiều quay của từng bên vật lý nếu cần.
6. [ ] **An toàn chạy thử:** Đã kê bánh xe của robot hổng lên khỏi mặt bàn khi cắm nạp code lần đầu tiên.
7. [ ] **Kiểm tra Serial:** Mở Serial Monitor ở tốc độ Baud `115200` để xem log và trạng thái cảm biến 8 mắt.

---

## 6. XE KHẮC PHỤC SỰ CỐ MOTOR KHÔNG CHẠY

### 6.1. Quy trình chẩn đoán nhanh (qua BLE, KHÔNG cần nạp lại firmware)

Kết nối app **Adafruit Bluefruit LE Connect** → tab **UART Terminal** → gửi lệnh:

| Bước | Lệnh | Làm gì |
| :--- | :--- | :--- |
| 1 | `X` | In báo cáo: duty hiện tại, mức GPIO `IN1..IN4`, cực quay, dead-band. **Quan sất mục "KIEM TRA JUMPER ENA/ENB"** — nếu báo `CANH BAO` thì xem mục 6.2. |
| 2 | `X1` | Tự kiểm tra: **trái tiến → phải tiến → trái lùi → phải lùi**. **Quan sát 2 bên có quay cùng chiều không.** |
| 3 | `XI` / `XR` | Nếu 2 bên quay **ngược chiều nhau**, gửi `XI` hoặc `XR` để đảo đúng bên. Lặp lại `X1` để xác nhận. |
| 4 | `XS` | Nếu bạn thấy `X1` quay **đúng bên trái nhưng là bên phải** (hoặc ngược lại), dùng `XS` để hoán đổi kênh vật lý. |
| 5 | `XM 170 175` | Nếu xe chạy nhưng kéo yếu / rung, tăng duty tối thiểu. |
| 6 | `XD 120 120` | Nếu bánh trong của vòng cong cứ quay kéo (không dừng hẳn), **giảm** ngưỡng dead-band. |

> Sau khi tìm được giá trị đúng, **chép ngược lại vào [`include/config.h`](../include/config.h)**
> (`INVERT_LEFT_MOTORS`, `INVERT_RIGHT_MOTORS`, `SWAP_LEFT_RIGHT_MOTORS`) để giữ nguyên sau khi nạp lại firmware.

### 6.2. ⚠️ Nguyên nhân số 1: CÒN JUMPER 5V TRÊN ENA / ENB

**Đây là nguyên nhân gây "động cơ không chạy" phổ biến nhất.**

L298N có 2 nắp jumper trên chân `ENA` và `ENB`:
- **Còn jumper** → chân `ENA`/`ENB` bị **ký sinh cứng ở mức 5 V**, chặn tín hiệu PWM từ ESP32.
  Kết quả: động cơ chạy **hết tốc** hoặc **không chạy**, và ESP32 **không điều khiển được**.
- **Đã rút jumper** → ESP32 điều khiển tốc độ bằng PWM như thiết kế.

**Cách kiểm tra nhanh:** gửi lệnh `X`. Nếu báo `CANH BAO: ENA/ENB dang keo len HIGH khi duty=0` → **rút jumper**.

> [!IMPORTANT]
> **Phải rút CẢ HAI jumper `ENA` VÀ `ENB`.** Rút mới đúng một bên vẫn sẽ có một bánh không chạy đúng.

### 6.3. Nguyên nhân số 2: Thiếu Mass chung (Common GND)

Tất cả phải nối **chung GND**: Pin (−) ↔ LM2596 `OUT-` ↔ ESP32 `GND` ↔ L298N `GND` ↔ cảm biến `GND` ↔ servo `GND`.
Nếu thiếu mass chung, tín hiệu `IN1..IN4` không xác định → động cơ đứng yên hoặc chạy lung tung.

### 6.4. Nguyên nhân số 3: Điện áp nguồn sai

| Hạng mục | Yêu cầu | Cách kiểm tra |
| :--- | :--- | :--- |
| LM2596 ra | **5,0 V** (chỉnh **trước khi** cắm tải) | VOM giữa `OUT+` và `OUT-` |
| L298N `12V` | Cấp từ pin 2S (7,4 V) | VOM giữa `12V` và `GND` |
| Cọc `5V` của L298N | **ĐỂ TRỐNG** (không nối vào ESP32/LM2596) | Kiểm tra bằng mắt |
| Jumper `5V Enable` trên L298N | **VẪN CẮM** (để IC 78M05 tự lấy nguồn) | Kiểm tra bằng mắt |

> [!WARNING]
> Nếu LM2596 **chưa chỉnh đúng 5,0 V** trước khi cắm ESP32, ESP32 có thể chết. Luôn chỉnh **trước**, đo lại **sau**.

### 6.5. Nguyên nhân số 4: Duty quá thấp (động cơ không đủ mô-men khởi động)

L298N có tổn thất điện áp khoảng **1,4–2 V** trên cầu H-bridge. Với pin 2S (7,4 V) và tải nặng (xe + 4 động cơ),
duty dưới khoảng **165/255 (65 %)** thường **không kéo được** động cơ.

Cách xử lý:
1. Firmware có **nhịp đề-pa** cấu hình `245/255` trong `120 ms`. Dò line tự động áp dụng thêm hệ số 88% khi hai bên chạy đều và 92% khi rẽ. Điều khiển tay dùng hệ số 100%. Nếu xe không chạy, kiểm tra nguồn và PWM thực tế cần cho tải.
2. Tăng duty tối thiểu qua BLE: `XM 180 180` (rồi chép giá trị này vào `config.h`).
3. Kiểm tra 2 động cơ cùng bên **có được đấu song song đúng cực** (2 dây đỏ vào một cọc, 2 dây đen vào cọc kia).
   Nếu 1 động cơ đấu ngược cực với động cơ kia, chúng **triệt tiêu nhau** và bên đó không quay.

### 6.6. Bảng tra nhanh

| Triệu chứng | Nguyên nhân khả dĩ | Cách xử lý |
| :--- | :--- | :--- |
| Không bên nào chạy | Còn jumper `ENA`/`ENB` | Rút cả 2 jumper (mục 6.2) |
| Không bên nào chạy | Thiếu mass chung | Nối chung GND (mục 6.3) |
| Không bên nào chạy | Sai điện áp LM2596 | Chỉnh lại 5,0 V (mục 6.4) |
| Chạy hết tốc, không điều khiển được | Còn jumper `ENA`/`ENB` | Rút jumper (mục 6.2) |
| Xe xoay tại chỗ thay vì đi thẳng | Cực quay 2 bên ngược nhau | `X1` → dùng `XI`/`XR` (mục 6.1) |
| Bên trái chạy, bên phải không | 2 động cơ phải đấu ngược cực | Kiểm tra lại dây, dùng `XR` |
| Xe kéo yếu, rung | Duty dưới ngưỡng kéo | `XM 180 180` (mục 6.5) |
| Bánh trong vòng cong kéo theo | Dead-band quá cao | `XD 120 120` (mục 6.1) |
| Xe lắc, mất vạch | Tốc độ cao hoặc cảm biến lệch ngưỡng | Giảm `MOTOR_OUTPUT_PERCENT` cho auto; kiểm tra trạng thái trái/phải trên Serial |
