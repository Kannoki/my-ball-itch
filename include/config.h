#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Chuoi thao tac sau giao lo 8 mat. Goc quay can hieu chinh tren xe that.
#define CROSS_REVERSE_DEFAULT_MS  2000
#define CROSS_FORWARD_CONFIRM_MS   200
#define CROSS_SEQUENCE_LIMIT          3 // Ba vach ngang thao tac sau vach xuat phat
// Initial on-floor calibration at MOTOR_PIVOT_SPEED and MOTOR_OUTPUT_PERCENT.
#define CROSS_RIGHT_45_MS          300
#define CROSS_LEFT_45_MS           300
#define MOTOR_PIVOT_SPEED          725  // Normalized wheel command (0..1000)
#define CROSS_SERVO_SETTLE_MS       600

// ==============================================================================
// CHE DO HOAT DONG CUA ROBOT
// ==============================================================================
enum RobotMode {
    MODE_STANDBY_BLE   = 0, // Khi khoi dong: DUNG YEN CHO, cho phep test tay qua BLE
    MODE_LINE_TRACKING = 1  // Tu dong do line: chi chay khi bam phim 1
};

// ==============================================================================
// 1. CAU HINH CHAN DIEU KHIEN DONG CO L298N (4 DONG CO)
// ==============================================================================
#ifdef BOARD_ESP32_38PIN
// Cau hinh chan an toan cho ESP32 38-pin (tranh GPIO 6-11 Flash, GPIO 1/3 UART0, GPIO 12 strapping)
// Kenh ben PHAI (OUT1 & OUT2)
#define PIN_MOTOR_RIGHT_ENA 32  // PWM dieu khien toc do ben phai (OUT1, OUT2)
#define PIN_MOTOR_RIGHT_IN1 33  // Huong quay phai 1
#define PIN_MOTOR_RIGHT_IN2 25  // Huong quay phai 2

// Kenh ben TRAI (OUT3 & OUT4)
#define PIN_MOTOR_LEFT_IN3  26  // Huong quay trai 1
#define PIN_MOTOR_LEFT_IN4  27  // Huong quay trai 2
#define PIN_MOTOR_LEFT_ENB  14  // PWM dieu khien toc do ben trai (OUT3, OUT4)
#else
// LM2596 cap 5 V cho ESP32-S3, Servo 2 va Servo 3. Servo 1 lay nguon truc tiep tu pin 2S 7.4 V.
// 2 dong co ben TRÁI: OUT3 & OUT4 (ENB: GPIO 13, IN3: GPIO 11, IN4: GPIO 12)
// 2 dong co ben PHẢI: OUT1 & OUT2 (ENA: GPIO 8,  IN1: GPIO 9,  IN2: GPIO 10)
// Kenh ben PHAI (OUT1 & OUT2)
#define PIN_MOTOR_RIGHT_ENA 8   // PWM dieu khien toc do ben phai (OUT1, OUT2)
#define PIN_MOTOR_RIGHT_IN1 9   // Huong quay phai 1
#define PIN_MOTOR_RIGHT_IN2 10  // Huong quay phai 2

// Kenh ben TRAI (OUT3 & OUT4)
#define PIN_MOTOR_LEFT_IN3  11  // Huong quay trai 1
#define PIN_MOTOR_LEFT_IN4  12  // Huong quay trai 2
#define PIN_MOTOR_LEFT_ENB  13  // PWM dieu khien toc do ben trai (OUT3, OUT4)
#endif

// Thong so PWM LEDC (Tach rieng timer cho dong co va 3 servo, tranh loi MCPWM tren ESP32-S3)
// TT 1:48 gearmotors via L298N ENB/ENA. Servo PWM uses separate channels.
#ifndef MOTOR_PWM_FREQ_HZ
#define MOTOR_PWM_FREQ_HZ 10000
#endif
#define PWM_RESOLUTION  8     // Do phan giai 8-bit (0 - 255)
#define PWM_CH_LEFT     0     // Kenh PWM LEDC 0 (Timer 0) cho dong co trai (OUT3 & OUT4)
#define PWM_CH_RIGHT    1     // Kenh PWM LEDC 1 (Timer 0) cho dong co phai (OUT1 & OUT2)
#define PWM_CH_SERVO1   2     // Kenh PWM LEDC 2 (Timer 1) cho Servo 1
#define PWM_CH_SERVO2   4     // Kenh PWM LEDC 4 (Timer 2) cho Servo 2
#define PWM_CH_SERVO3   6     // Kenh PWM LEDC 6 (Timer 3) cho Servo 3

// ==============================================================================
// VAN TOC DO (CHUAN HOA -1000 .. +1000, LAY CAM HUNG TU LbMotion.runLR CUA LEANBOT)
// ==============================================================================
// Frame cua robot: dau (+) = tien, dau (-) = lui, cho CA HAI ben.
// Phan cuc thuoc tinh vat ly (INVERT_*/SWAP_*) duoc apply o TANG DRIVER,
// nen runTurnLeft LUON xoay sang trai du doi mau chan hay kiem tra chan.
#define MOTOR_SPEED_SCALE        1000  // Gia tri toi da cua van toc do chuan hoa
#define MOTOR_SPEED_STEP          40   // Buoc tang/giam khi lenh '+'/'-' qua BLE (~10/255)
#define MOTOR_OUTPUT_PERCENT      88   // Chi ap dung cho do line tu dong; lenh tay mac dinh 100%

// Van toc do mac dinh (don vi chuan hoa)
#define MOTOR_BASE_SPEED         725   // ~185/255 : toc do co ban khi di thang
#define MOTOR_MAX_SPEED          980   // ~250/255 : toc do toi da khi tang toc
#define MOTOR_MIN_SPEED          647   // ~165/255 : nhanh nhat ma van chay duoc
#define MOTOR_MANUAL_SPEED       863   // ~220/255 : toc do khi dieu khien bang tay qua BLE

// Thong so PWM (mien driver, 0 - 255)
// MOTOR_MIN_PWM_*   : duty nho nhat ma van chay duoc (ngoai dai co truot)
// MOTOR_DEADBAND_*  : yeu cau duty nho hon ngan -> DUNG THAT (duty = 0).
//                     Dat bang MOTOR_MIN_PWM_* de che do "dung han" sach,
//                     dat NHO HON de tao vung "creep" (banh trong chay cham).
#define MOTOR_MIN_PWM_LEFT       165
#define MOTOR_MIN_PWM_RIGHT      165
#define MOTOR_DEADBAND_PWM_LEFT   120
#define MOTOR_DEADBAND_PWM_RIGHT  120
#define MOTOR_START_PWM          245   // Duty toi thieu trong nhip de-pa khi bat dau quay
#define MOTOR_START_MS           120   // Thoi gian de-pa truoc khi ve duty yeu cau
#define MOTOR_RAMP_MS            150   // Thoi gian de chuyen duty giua 2 muc (lam mat chay)

// Ty le van toc do ben trong khi re cong (%). 60 => ben trong chay 60% ben ngoai.
// Luu y: phai lon hon nguong dead-band (xem MOTOR_DEADBAND_PWM_*) de banh trong
// van chay duoc trong khi re cong; neu nho hon, ben trong se DUNG (duty 0).
#define MOTOR_ARC_INNER_RATIO     60

// Van toc do dung cho phep tu kiem tra dong co (phai >= MOTOR_MIN_SPEED)
#define MOTOR_SELFTEST_SPEED    700

// Dao chieu quay theo ben vat ly cua xe.
// LUU Y: 3 co nay la GIA TRI KHOI TAO cho tang driver. Chung co the doi runtime
// qua lenh BLE (XI / XR / XS) ma KHONG can nap lai firmware.
// Chúng duoc apply o TANG GHI GPIO, KHONG phai tren toc do da lenh, nen viec
// dao mot ben KHONG lam sai huong xoay (runTurnLeft luon xoay sang trai).
#define INVERT_LEFT_MOTORS     false
#define INVERT_RIGHT_MOTORS    true
// Mac dinh Trai=OUT3/4, Phai=OUT1/2. Dat true neu day hai ben dau vao kenh nguoc lai.
#ifndef SWAP_LEFT_RIGHT_MOTORS
#define SWAP_LEFT_RIGHT_MOTORS false
#endif

// ==============================================================================
// 2. CAU HINH CHAN CAM BIEN DO LINE (5 SENSORS TCRT5000 2D4.1 HOAC 8 SENSORS)
// ==============================================================================
#ifdef SENSOR_TCRT5000_5CH
#define NUM_SENSORS       5
#define SENSOR_HEIGHT_MM  3.0f  // Khoang cach toi uu TCRT5000 2D4.1 (2 - 5 mm)
#define SENSOR_PITCH_MM   18.0f // Buoc tam giua 2 mat TCRT5000 lien ke (~18 mm)
#define LINE_WIDTH_MM     16.0f // Be ngang vach line den (16 mm)
#else
#define NUM_SENSORS       8
#define SENSOR_HEIGHT_MM  0.5f  // Khoang cach tu mat doc sensor xuong mat dat (0.5 mm)
#define SENSOR_PITCH_MM   6.0f  // Khoang cach giua 2 mat doc lien ke (6 mm)
#ifdef SENSOR_PHOTORESISTOR
#define LINE_WIDTH_MM     16.0f
#else
#define LINE_WIDTH_MM     20.0f // Be ngang line tren san IR hien tai (~20 mm)
#endif
#endif
#define SENSOR_MASK       static_cast<uint8_t>((1U << NUM_SENSORS) - 1U)

// Bit 0 la S1 ngoai cung ben trai. Moi mat tham gia tinh line.
#ifdef SENSOR_TCRT5000_5CH
#define LINE_LEFT_MASK           0x03 // S1, S2
#define LINE_CENTER_MASK         0x04 // S3
#define LINE_RIGHT_MASK          0x18 // S4, S5
#else
#define LINE_LEFT_MASK           0x0F // S1..S4
#define LINE_CENTER_MASK         0x00
#define LINE_RIGHT_MASK          0xF0 // S5..S8
#endif
#define LINE_TRACKING_MASK static_cast<uint8_t>(LINE_LEFT_MASK | LINE_CENTER_MASK | LINE_RIGHT_MASK)

#ifdef BOARD_ESP32_38PIN
// Day chan cam bien cho ESP32 38-pin (cum GPIO ben phai, ho tro INPUT & INPUT_PULLUP)
#if !defined(SENSOR_PHOTORESISTOR) && !defined(SENSOR_TCRT5000_5CH)
// Mirrored IR bar: logical S1 is physical D8.
#define PIN_LINE_S1     16
#define PIN_LINE_S2     17
#define PIN_LINE_S3     5
#define PIN_LINE_S4     18
#define PIN_LINE_S5     19
#define PIN_LINE_S6     21
#define PIN_LINE_S7     22
#define PIN_LINE_S8     23
#else
#define PIN_LINE_S1     23  // Cam bien ngoai cung ben trai
#define PIN_LINE_S2     22
#define PIN_LINE_S3     21  // Giua (voi 5 mat) / Lech trai (voi 8 mat)
#define PIN_LINE_S4     19  // Lech phai (voi 5 mat) / Giua trai (voi 8 mat)
#define PIN_LINE_S5     18  // Ngoai cung phai (voi 5 mat) / Giua phai (voi 8 mat)
#ifndef SENSOR_TCRT5000_5CH
#define PIN_LINE_S6     5
#define PIN_LINE_S7     17
#define PIN_LINE_S8     16  // Cam bien ngoai cung ben phai
#endif

#endif

#if defined(SENSOR_PHOTORESISTOR) || defined(SENSOR_TCRT5000_5CH)
#define PIN_LINE_IR_EN  -1  // Quang tro va TCRT5000 2D4.1 (7 chan) khong co chan IR Enable
#else
#define PIN_LINE_IR_EN  4   // GPIO 4 xuat HIGH bat den IR (tranh GPIO 1 TX0 cua ESP32 38-pin)
#endif
#else
#define PIN_LINE_S1     18
#define PIN_LINE_S2     17
#define PIN_LINE_S3     16
#define PIN_LINE_S4     15
#define PIN_LINE_S5     7
#define PIN_LINE_S6     6
#define PIN_LINE_S7     5
#define PIN_LINE_S8     4

// Chan kich hoat den LED phat hong ngoai (IR / 1R Enable):
// - Mach 8 mat co 1 chan ky hieu 'IR' (nhin giong '1R') dung de cap nguon/bat 8 den LED phat hong ngoai
// - Neu de trong chan nay: 8 den phat bi TAT HOAN TOAN -> khong co tia phan xa -> cam bien bao FULL DEN!
// - Cach dau: Co the noi truc tiep vao VCC (5V/3.3V), hoac noi vao GPIO 1 (ESP32-S3 se tu xuat HIGH)
#define PIN_LINE_IR_EN  1   // GPIO 1 xuat HIGH bat den IR (hoac noi thang chan nay vao VCC)
#endif

// Muc logic khi cam bien phat hien vach line den:
// - IR 8 mat dang dung: da do GPIO 00000000 tren nen trang -> LOW la nen, HIGH la line.
// - Quang tro (LDR) 8 mat: mac dinh vach DEN -> HIGH (1).
// - TCRT5000 2D4.1 giu mac dinh LOW; dung lenh I de dao neu phan cung khac.
#ifdef SENSOR_TCRT5000_5CH
#define LINE_ACTIVE_STATE LOW
#else
#define LINE_ACTIVE_STATE HIGH
#endif

// ==============================================================================
// 3. DO LINE CO BAN
// ==============================================================================
#if NUM_SENSORS == 5
#define PID_KP 62.0f
#define PID_KD 24.0f
#define LINE_CURVE_SPEED_DROP 24.0f
#else
#define PID_KP 45.0f
#define PID_KD 18.0f
#define LINE_CURVE_SPEED_DROP 16.0f
#endif
#define LINE_ERROR_FILTER_ALPHA 0.45f
#define COURSE_BEND_CONFIRM_MS 40
#define LINE_SAMPLE_INTERVAL_MS    10   // Doc 100 lan/giay, xac nhan hai lan cung ket luan
#define CROSS_LINE_PASS_DELAY_MS  140   // Gioi han di qua mau toan den cua ban 5 mat
#define LINE_RECOVERY_FORWARD_MS  140   // Giu moc sau khi bat lai line va thu di thang

// ==============================================================================
// 4. CAU HINH CHAN TIN HIEU CHO 3 SERVO (CHAY DOC LAP)
// ==============================================================================
#ifdef BOARD_ESP32_38PIN
#define PIN_SERVO_1     13  // Chan tin hieu Servo 1: TD-8120MG
#define PIN_SERVO_2     15  // Chan tin hieu Servo 2
#define PIN_SERVO_3     2   // Chan tin hieu Servo 3 (GPIO 47 khong co tren ESP32 38-pin)
#else
#define PIN_SERVO_1     14  // Chan tin hieu Servo 1: TD-8120MG (20KG High Torque - Co cau day hanh trinh)
#define PIN_SERVO_2     21  // Chan tin hieu Servo 2 (GPIO 21)
#define PIN_SERVO_3     2  // Chan tin hieu Servo 3 (GPIO 47)
#endif

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

// Thong so cho Servo 3 (GPIO 47)
#define SERVO3_MIN_PULSE_US   500
#define SERVO3_MAX_PULSE_US   2500
#define SERVO3_MAX_ANGLE      180

// ==============================================================================
// 4. CAU HINH BLUETOOTH LOW ENERGY (BLUEFRUIT BLE)
// ==============================================================================
#define BLE_DEVICE_NAME "xe dò line s3"

#endif // CONFIG_H
