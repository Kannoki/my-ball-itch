#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==============================================================================
// CHE DO HOAT DONG CUA ROBOT
// ==============================================================================
enum RobotMode {
    MODE_STANDBY_BLE   = 0, // Khi khoi dong: DUNG YEN CHO, cho phep test tay qua BLE
    MODE_LINE_TRACKING = 1  // Che do tu dong bam vien (Edge Tracking): CHI CHAY KHI BAM PHIM 1
};

// ==============================================================================
// CHE DO BAM VIEN (EDGE TRACKING: RANH GIOI GIUA DEN VA TRANG)
// ==============================================================================
enum EdgeMode {
    EDGE_WHITE_LEFT_BLACK_RIGHT = 0, // Ben trai la Trang (0), ben phai la Den (1): chuyen tiep 0 -> 1
    EDGE_BLACK_LEFT_WHITE_RIGHT = 1, // Ben trai la Den (1), ben phai la Trang (0): chuyen tiep 1 -> 0
    EDGE_AUTO                   = 2  // Tu dong tim ranh gioi mep bien gan tam nhat
};

// Mac dinh: Bam vien trai (Trang ben trai, Den ben phai)
#define EDGE_FOLLOW_MODE EDGE_WHITE_LEFT_BLACK_RIGHT

// ==============================================================================
// 1. CAU HINH CHAN DIEU KHIEN DONG CO L298N (4 DONG CO)
// LM2596 cap 5 V cho ESP32-S3, Servo 2 va Servo 3. Servo 1 lay nguon truc tiep tu pin 2S 7.4 V.
// 2 dong co ben TRÁI: OUT3 & OUT4 (ENB: GPIO 13, IN3: GPIO 11, IN4: GPIO 12)
// 2 dong co ben PHẢI: OUT1 & OUT2 (ENA: GPIO 8,  IN1: GPIO 9,  IN2: GPIO 10)
// ==============================================================================
// Kenh ben PHAI (OUT1 & OUT2)
#define PIN_MOTOR_RIGHT_ENA 8   // PWM dieu khien toc do ben phai (OUT1, OUT2)
#define PIN_MOTOR_RIGHT_IN1 9   // Huong quay phai 1
#define PIN_MOTOR_RIGHT_IN2 10  // Huong quay phai 2

// Kenh ben TRAI (OUT3 & OUT4)
#define PIN_MOTOR_LEFT_IN3  11  // Huong quay trai 1
#define PIN_MOTOR_LEFT_IN4  12  // Huong quay trai 2
#define PIN_MOTOR_LEFT_ENB  13  // PWM dieu khien toc do ben trai (OUT3, OUT4)

// Thong so PWM cho ESP32-S3
#define PWM_FREQ        10000 // 10 kHz (em tieng dong co)
#define PWM_RESOLUTION  8     // Do phan giai 8-bit (0 - 255)
#define PWM_CH_LEFT     0     // Kenh PWM LEDC cho dong co trai (OUT3 & OUT4)
#define PWM_CH_RIGHT    1     // Kenh PWM LEDC cho dong co phai (OUT1 & OUT2)

// Toc do mac dinh
#define MOTOR_BASE_SPEED    185 // Toc do co ban khi di thang (0-255)
#define MOTOR_MAX_SPEED     250 // Toc do toi da khi tang toc (0-255)
#define MOTOR_MIN_PWM_LEFT  165 // PWM nho nhat khi dong co trai chay
#define MOTOR_MIN_PWM_RIGHT 165 // PWM nho nhat khi dong co phai chay
constexpr int MOTOR_MIN_SPEED = MOTOR_MIN_PWM_LEFT > MOTOR_MIN_PWM_RIGHT ? MOTOR_MIN_PWM_LEFT : MOTOR_MIN_PWM_RIGHT;
#define MOTOR_START_PWM     245 // Duty toi thieu trong nhip de-pa khi bat dau quay
#define MOTOR_START_MS      120 // Thoi gian de-pa truoc khi ve duty yeu cau
#define MOTOR_MANUAL_SPEED  220 // Toc do khi dieu khien bang tay qua BLE
#define MOTOR_SEARCH_SPEED  185 // Toc do xoay tim lai vach khi bi vang khoi ranh gioi
#define MOTOR_SPIN_SPEED    195 // Toc do xoay gat tai cho khi gap goc vuong 90 do / cua chu U

// Dao chieu quay theo ben vat ly cua xe, voi toc do duong la tien.
#define INVERT_LEFT_MOTORS     false
#define INVERT_RIGHT_MOTORS    true
// Mac dinh Trai=OUT3/4, Phai=OUT1/2. Dat true neu day hai ben dau vao kenh nguoc lai.
#ifndef SWAP_LEFT_RIGHT_MOTORS
#define SWAP_LEFT_RIGHT_MOTORS false
#endif

// ==============================================================================
// 2. CAU HINH CHAN CAM BIEN DO LINE (8 SENSORS)
// Thong so vat ly: Line den 1.6 cm, khoang cach tu mat doc xuong san 1 mm (0.1 cm)
// Khoang cach tu mat doc sensor den truc 2 banh truoc la 4.0cm
// ==============================================================================
#define NUM_SENSORS 8

#define PIN_LINE_S1     4   // Cam bien ngoai cung ben trai
#define PIN_LINE_S2     5
#define PIN_LINE_S3     6
#define PIN_LINE_S4     7   // Cam bien giua trai
#define PIN_LINE_S5     15  // Cam bien giua phai
#define PIN_LINE_S6     16
#define PIN_LINE_S7     17
#define PIN_LINE_S8     18  // Cam bien ngoai cung ben phai

// Chan kich hoat den LED phat hong ngoai (IR / 1R Enable):
// - Mach 8 mat co 1 chan ky hieu 'IR' (nhin giong '1R') dung de cap nguon/bat 8 den LED phat hong ngoai
// - Neu de trong chan nay: 8 den phat bi TAT HOAN TOAN -> khong co tia phan xa -> cam bien bao FULL DEN!
// - Cach dau: Co the noi truc tiep vao VCC (5V/3.3V), hoac noi vao GPIO 1 (ESP32-S3 se tu xuat HIGH)
#define PIN_LINE_IR_EN  1   // GPIO 1 xuat HIGH bat den IR (hoac noi thang chan nay vao VCC)

// Muc logic khi cam bien phat hien vach line den:
// - Tren thanh 8 mat (nhu HY-S301 / TCRT5000):
//   + Nen TRANG phan xa tia hong ngoai manh -> chan tin hieu xuat muc HIGH (1)
//   + Vach DEN hap thu tia hong ngoai khong phan xa -> chan tin hieu xuat muc LOW (0)
// - Do do, vach DEN tuong ung voi muc logic LOW (0)!
#define LINE_ACTIVE_STATE LOW

// Nguong phan loai cho goc cua gat (khi mep bien chay ra vung mat ngoai cung S2/S1 hoac S7/S8)
#define SENSORS_CROSS_LINE_THRESHOLD 6    // Nguong nhan dien vach ngang / nga tu (tu 6 mat tro len)
#define SHARP_TURN_THRESHOLD         2.5f // Chi xoay khi mep nam on dinh gan mat ngoai
#define SHARP_TURN_EXIT_THRESHOLD    1.0f // Thoat xoay khi mep tro ve gan tam
#define LINE_SEARCH_TIMEOUT_MS       750  // Dung dong co neu chua tim thay vien

// He so PID toi uu cho bam vien va be lai dut khoat
#define PID_KP                  45.0f // PWM tren moi buoc lech cam bien
#define PID_KD                  18.0f // He so vi phan quy ve chu ky lay mau danh dinh
#define LINE_ERROR_FILTER_ALPHA  0.45f
#define LINE_CURVE_SPEED_DROP   16.0f // Giam PWM co ban tren moi buoc lech cam bien

// ==============================================================================
// 3. THONG SO HINH HOC CO KHI & DELAY CANH BAO (LOOK-AHEAD DELAY CHO CUC LY 4CM)
// ==============================================================================
#define SENSOR_TO_AXLE_DISTANCE_CM  4.0f // Khoang cach tu mat sensor den truc 2 banh truoc (4cm)

// Do mat sensor di truoc banh xe 4cm, khi mat sensor thay goc vuong 90 do thi banh xe van con cach 4cm
// Delay chay thang nhe 65ms de 2 banh truoc tien sat toi dinh cua roi moi xoay (chong chem cua som)
#define SHARP_TURN_LEAD_DELAY_MS    65   // Tri hoan chay thang truoc khi xoay gat 90 do (ms)

// Thoi gian chay thang duy tri qua vach ngang/nga tu (ms) de 2 banh truoc vuot qua hoan toan khoang cach 4cm
#define CROSS_LINE_PASS_DELAY_MS    140  // Chay thang 140ms qua vach ngang

// Chu ky lay mau PD toi uu (10ms = 100Hz, moi chu ky xe di chuyen ~0.4cm o toc do 185)
#define PID_SAMPLE_INTERVAL_MS      10

// ==============================================================================
// 4. CAU HINH CHAN TIN HIEU CHO 3 SERVO (CHAY DOC LAP)
// ==============================================================================
#define PIN_SERVO_1     14  // Chan tin hieu Servo 1: TD-8120MG (20KG High Torque - Co cau day hanh trinh)
#define PIN_SERVO_2     21  // Chan tin hieu Servo 2 (GPIO 21)
#define PIN_SERVO_3     47  // Chan tin hieu Servo 3 (GPIO 47)

// Thong so chuan Datasheet cho Servo TD-8120MG tren CHAN GPIO 14 (20kg.cm Digital Metal Gear)
// Pulse Width Range: 500us -> 2500us (Trung tam Neutral: 1500us)
// Running Degree: 270 do (hoac 180 do)
#define SERVO1_MIN_PULSE_US   500   // Xung nho nhat (0 do / 0% hanh trinh)
#define SERVO1_MAX_PULSE_US   2500  // Xung lon nhat (270 do / 100% hanh trinh)
#define SERVO1_MAX_ANGLE      270   // Goc hanh trinh toi da theo datasheet TD-8120MG

// Thong so cho Servo 2 (GPIO 21)
#define SERVO2_MIN_PULSE_US   500
#define SERVO2_MAX_PULSE_US   2500
#define SERVO2_MAX_ANGLE      180

// ==============================================================================
// 4. CAU HINH BLUETOOTH LOW ENERGY (BLUEFRUIT BLE)
// ==============================================================================
#define BLE_DEVICE_NAME "xe dò line"

#endif // CONFIG_H
