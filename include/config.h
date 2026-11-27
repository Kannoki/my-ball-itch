#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Chuoi thao tac sau giao lo 8 mat. Goc quay can hieu chinh tren xe that.
#define CROSS_REVERSE_DEFAULT_MS  2000
#define CROSS_FORWARD_CONFIRM_MS   200
#define CROSS_SEQUENCE_LIMIT          3 // Ba vach ngang thao tac sau vach xuat phat
#define CROSS_TURN_45_MS           300
#define CROSS_SERVO_SETTLE_MS       600

// ------------------------------------------------------------------------------
// CHUONG TRINH DUONG CHAY (phan biet giao lo theo THU TU, khong theo hinh hoc)
// ------------------------------------------------------------------------------
// Giao lo (+) va tram servo deu la mot dai vach ngang cat qua thanh cam bien
// ("..####..", ".######.", "########"...) -> HAI LOAI NAY CUNG MOT DAU HIEU CAM
// BIEN, khong the phan biet bang hinh hoc. Chi phan biet duoc bang thu tu:
//   buoc 0: cho nga 3 (chu T)     -> quay TRAI 90 do
//   buoc 1: cho giao lo (+)       -> quay PHAI 90 do
//   buoc 2: 3 tram servo          -> chuoi thao tac servo
//   buoc 3: het chuong trinh      -> chi di thang
#define COURSE_STEP_WAIT_TEE     0
#define COURSE_STEP_WAIT_CROSS   1
#define COURSE_STEP_STATIONS     2
#define COURSE_STEP_DONE         3

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
#ifdef SENSOR_PHOTORESISTOR
// Nhuong GPIO 32, 33 va 25 cho ADC cua cam bien quang tro.
#define PIN_MOTOR_RIGHT_ENA 23
#define PIN_MOTOR_RIGHT_IN1 22
#define PIN_MOTOR_RIGHT_IN2 21
#else
#define PIN_MOTOR_RIGHT_ENA 32  // PWM dieu khien toc do ben phai (OUT1, OUT2)
#define PIN_MOTOR_RIGHT_IN1 33  // Huong quay phai 1
#define PIN_MOTOR_RIGHT_IN2 25  // Huong quay phai 2
#endif

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
#define PWM_FREQ        10000 // 10 kHz (em tieng dong co)
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
#define MOTOR_OUTPUT_PERCENT      88   // He so PWM tu lai khi hai ben chay deu
#define MOTOR_TURN_OUTPUT_PERCENT 92   // He so PWM tu lai khi hai ben chay khac nhau

// Van toc do mac dinh (don vi chuan hoa)
#ifdef SENSOR_PHOTORESISTOR
#define MOTOR_BASE_SPEED         687   // 154/255 khi tu lai di thang
#define MOTOR_MANUAL_SPEED       659   // 168/255 khi tien/lui bang tay
#define MOTOR_ENCODER_TURN_SPEED 950   // 242/255 khi quay theo encoder
#define MOTOR_EDGE_SPEED_DELTA    100   // Chenh lech toc do hai banh khi bam vach o mep
#else
#define MOTOR_BASE_SPEED         865   // 220/255 truoc khi scale, 193/255 khi tu lai di thang
#define MOTOR_MANUAL_SPEED       824   // 210/255 khi dieu khien bang tay qua BLE
#endif
#define MOTOR_MAX_SPEED          980   // ~250/255 : toc do toi da khi tang toc
#define MOTOR_MIN_SPEED          647   // ~165/255 : nhanh nhat ma van chay duoc
// Toc do chuan hoa toi thieu de banh THUC SU chay (duty 120 = nguong dead-band).
// Dung khi ramp-up: lenh duoi nguong nay se bi driver dung that.
#define MOTOR_MIN_MOVING_SPEED   471
#define MOTOR_MANUAL_TURN_SPEED  863   // 220/255 khi quay trai/phai bang tay

// Thong so PWM (mien driver, 0 - 255)
// MOTOR_MIN_PWM_*   : duty nho nhat ma van chay duoc (ngoai dai co truot)
// MOTOR_DEADBAND_*  : yeu cau duty nho hon ngan -> DUNG THAT (duty = 0).
//                     Dat bang MOTOR_MIN_PWM_* de che do "dung han" sach,
//                     dat NHO HON de tao vung "creep" (banh trong chay cham).
#define MOTOR_MIN_PWM_LEFT       150
#define MOTOR_MIN_PWM_RIGHT      150
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
#define SENSOR_PITCH_MM   6.0f  // Khoang cach giua 2 mat doc lien ke (6 mm)
#ifdef SENSOR_PHOTORESISTOR
#define SENSOR_HEIGHT_MM  12.0f // Mat doc cach san 1.2 cm
#define LINE_WIDTH_MM     16.0f // Be ngang line 1.6 cm
// shortcut: nguong khoi dau 12-bit, thay bang trung diem den/trang do tren san.
constexpr uint16_t PHOTORES_ADC_THRESHOLDS[NUM_SENSORS] = {
    3000, 3000, 3000, 3000, 3000, 3000, 3000, 3000 // S1..S8
};
#else
#define SENSOR_HEIGHT_MM  0.5f  // Khoang cach tu mat doc sensor xuong mat dat (0.5 mm)
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
#ifdef SENSOR_PHOTORESISTOR
// S1..S2 dung ADC2, S3..S8 dung ADC1 (firmware dung BLE).
#define PIN_LINE_S1     4   // Ngoai cung trai
#define PIN_LINE_S2     25
#define PIN_LINE_S3     33
#define PIN_LINE_S4     32
#define PIN_LINE_S5     35
#define PIN_LINE_S6     34
#define PIN_LINE_S7     39  // VN
#define PIN_LINE_S8     36  // VP, ngoai cung phai
#define PIN_LINE_IR_EN  -1

// Encoder mot ngo ra OUT, dem canh len o hai banh truoc.
#define PIN_ENCODER_LEFT         18
#define PIN_ENCODER_RIGHT        19
#define ENCODER_PULSES_PER_REV   20
#define FRONT_WHEEL_DIAMETER_MM  66.0f
#define FRONT_TRACK_WIDTH_MM     134.0f
#define LINE_SENSOR_FORWARD_MM   120.0f
#define ENCODER_TURN_SLOW_DEG    15.0f
#define ENCODER_TURN_TIMEOUT_MS  2500UL

// ------------------------------------------------------------------------------
// HIEU CHINH ENCODER (do sai so xung giua hai banh)
// ------------------------------------------------------------------------------
// ENCODER_PULSE_SCALE_*: he so nhan so xung thuc do voi so xung ly thuyet.
//   > 1.0 khi banh do lai CHAY DUONG HON mong doi (truot) - dung khi YAW/odometry
//   lech khuong.
//   Cach do: chay thang 1000 mm, dem xung tren Serial,
//     scale = xung_do / (1000 / (PI * 66 / 20)) = xung_do / 96.5
// SPEED_TRIM_*: lech feed-forward (donvi chuan hoa) khi hai ben khong cung toc do.
//   Dung khi Speed PI da khop nhung xe van lech khuong khi di thang.
//   Lech sang phai (+): tang SPEED_TRIM_LEFT, giam SPEED_TRIM_RIGHT.
#define ENCODER_PULSE_SCALE_LEFT   1.000f
#define ENCODER_PULSE_SCALE_RIGHT  1.000f
#define SPEED_TRIM_LEFT            0
#define SPEED_TRIM_RIGHT           0

// ------------------------------------------------------------------------------
// RAMP-UP TOC DO CO BAN (khi bat dau do line)
// ------------------------------------------------------------------------------
// Tang dan toc do co ban tu 60% -> 100% trong 600 ms de banh khong truot
// khi khoi hanh. Gia tri ap dung len LENH toc do, khong sua motorBaseSpeed,
// nen lenh '+'/'-' qua BLE van tac dong tren muc tieu.
#define BASE_SPEED_RAMP_MS        600UL
#define BASE_SPEED_RAMP_START     0.60f  // 60% cua MOTOR_BASE_SPEED luc bat dau

#else
// Day chan cam bien cho ESP32 38-pin (cum GPIO ben phai, ho tro INPUT & INPUT_PULLUP)
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

#ifdef SENSOR_TCRT5000_5CH
#define PIN_LINE_IR_EN  -1  // TCRT5000 2D4.1 (7 chan) khong co chan IR Enable
#else
#define PIN_LINE_IR_EN  4   // GPIO 4 xuat HIGH bat den IR (tranh GPIO 1 TX0 cua ESP32 38-pin)
#endif
#endif
#else
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
#endif

// Muc logic khi cam bien phat hien vach line den:
// - IR 8 mat dang dung: da do GPIO 00000000 tren nen trang -> LOW la nen, HIGH la line.
// - Quang tro (LDR) 8 mat: HIGH la ADC >= nguong, LOW la ADC < nguong.
// - TCRT5000 2D4.1 giu mac dinh LOW; dung lenh I de dao neu phan cung khac.
#ifdef SENSOR_PHOTORESISTOR
#define LINE_ACTIVE_STATE LOW
#elif defined(SENSOR_TCRT5000_5CH)
#define LINE_ACTIVE_STATE LOW
#else
#define LINE_ACTIVE_STATE HIGH
#endif

// ==============================================================================
// 3. DO LINE CO BAN
// ==============================================================================
#define LINE_STEERING_KP            38.0f // PWM tren moi buoc lech mat cam bien
#define LINE_STEERING_KD            14.0f // PWM theo thay doi sai lech moi chu ky lay mau
#define LINE_STEERING_REFERENCE_MS  10UL // Chu ky tham chieu cua bo loc va he so D
#define LINE_ERROR_FILTER_ALPHA      0.40f
#define LINE_CURVE_SPEED_DROP       14.0f // Giam PWM co so khi line lech tam

// ------------------------------------------------------------------------------
// NGA RE 90 DO (nhanh re trai / phai)
// ------------------------------------------------------------------------------
// Quy tac nhanh dung hinh hoc, kh dung so sanh mask chinh xac:
//   Ben TRAI  : S1 va S2 den, S8 trang  -> chap nhan "###....." va "####...."
//   Ben PHAI  : S7 va S8 den, S1 trang  -> chap nhan ".....###" va "....####"
// Xac nhan JUNCTION_CONFIRM_SAMPLES mau lien tiep truoc khi quay.
#define JUNCTION_PIVOT_DEGREES    90
#define JUNCTION_CONFIRM_SAMPLES  2
#define JUNCTION_COOLDOWN_MS      1200UL
#define JUNCTION_TURN_SLOW_DEG    20.0f   // Giu toc do cao den gan goc dich
#define JUNCTION_FINE_TURN_SPEED  776     // ~197/255 khi xoay, ~181/255 khi quet tim vach
#define JUNCTION_POST_PIVOT_SETTLE_MS 100UL
#define JUNCTION_LINE_YAW_DEG     20.0f   // Cua so huong quanh nhanh moi khi tim lai vach
#define JUNCTION_MIN_BLACK_EDGE   2      // So mat den toi bien (S1+S2 hoac S7+S8)
#define JUNCTION_PIVOT_TIMEOUT_MS 2500UL
// So mau lien tiep phai mat vach sau khi quay xong truoc khi coi la "vao nhanh".
#define JUNCTION_REACQUIRE_SAMPLES 2

// ------------------------------------------------------------------------------
// VONG TRONG: SPEED PI (PID TUNG BANH) - CHAY DUOI VONG NGOAI POSITION PD
// ------------------------------------------------------------------------------
// Luu y: 1 xung encoder = PI * 66 / 20 = 10.367 mm.
//   O toc do hanh thong ~350 mm/s chi co ~34 xung/s => ~0.17 xung moi mau 5 ms.
//   VI vay PI phai chay tren CUA SO THOI GIAN (150 ms) chu khong phai tren tung mau.
// Tham so mau moi 5 ms -> dung bang millis(), khong dung delay() trong vong dieu khien.
#define SPEED_PID_PERIOD_MS        20UL   // Chu ky tinh PI (tach khoi mau cam bien 5 ms)
#define SPEED_PID_WINDOW_MS       150UL   // Cua so dem xung de uoc luong van toc do
#define SPEED_PID_KP              0.35f  // He so ti le (donvi duty cho moi mm/s sai lech)
#define SPEED_PID_KI              0.08f  // He so integral (chong lech tinh khi di thang)
#define SPEED_PID_KD              0.00f  // De 0: sai lech van toc do luoi, Kd chi lam nhieu
#define SPEED_PID_I_LIMIT         60.0f  // Gioi han integral (chong windup)
#define SPEED_PID_OUT_LIMIT       0.15f  // Gioi han lech so voi duty mo (15%)
#define SPEED_PID_REFERENCE_MM_S  230.0f // Van toc do tai duty 165/255 (duoc hieu chinh)
// Nguong bao ho: neu cua so xung rong qua lan nhieu -> coi nhu mat encoder.
#define SPEED_PID_STALL_WINDOWS   3

// ------------------------------------------------------------------------------
// RAMP-UP TOC DO CO BAN (khi bat dau do line)
// ------------------------------------------------------------------------------
// Tang dan toc do co ban tu 60% -> 100% trong 600 ms de banh khong truot
// khi khoi hanh. Gia tri ap dung len LENH toc do, khong sua motorBaseSpeed,
// nen lenh '+'/'-' qua BLE van tac dong tren muc tieu.
#define BASE_SPEED_RAMP_MS        600UL
#define BASE_SPEED_RAMP_START     0.60f  // 60% cua MOTOR_BASE_SPEED luc bat dau
#ifdef SENSOR_PHOTORESISTOR
#define LINE_SAMPLE_INTERVAL_MS     5UL  // Doc 200 lan/giay
#else
#define LINE_SAMPLE_INTERVAL_MS    10   // Doc 100 lan/giay, xac nhan hai lan cung ket luan
#endif
#define CROSS_LINE_PASS_DELAY_MS  140   // Gioi han di qua mau toan den cua ban 5 mat
#define LINE_RECOVERY_FORWARD_MS  140   // Giu moc sau khi bat lai line va thu di thang
#define FIRST_BRANCH_TURN_TIMEOUT_MS 1500 // Thoi gian tim line khi quay vao nhanh trai dau tien

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
