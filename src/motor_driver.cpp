#include "motor_driver.h"
#include <esp_arduino_version.h>
#include <mutex>

static_assert(MOTOR_MIN_PWM_LEFT > 0 && MOTOR_MIN_PWM_RIGHT > 0 &&
              MOTOR_MIN_SPEED <= MOTOR_MAX_SPEED && MOTOR_BASE_SPEED <= MOTOR_MAX_SPEED &&
              MOTOR_MAX_SPEED <= MOTOR_SPEED_SCALE,
              "Invalid motor speed limits");
static_assert(MOTOR_START_PWM > 0 && MOTOR_START_PWM <= 255, "Invalid motor startup boost");
static_assert(MOTOR_START_MS > 0 && MOTOR_RAMP_MS > 0, "Invalid motor timing");
static_assert(MOTOR_AUTO_START_MS > 0 && MOTOR_AUTO_RESTART_MS >= MOTOR_AUTO_START_MS,
              "Invalid automatic startup timing");
static_assert(MOTOR_ARC_INNER_RATIO > 0 && MOTOR_ARC_INNER_RATIO < 100, "Invalid arc ratio");
static_assert(MOTOR_OUTPUT_PERCENT > 0 && MOTOR_OUTPUT_PERCENT <= 100, "Invalid motor output scale");

// ------------------------------------------------------------------------------
// Trang thai toc do chuan hoa
// ------------------------------------------------------------------------------
int motorBaseSpeed = MOTOR_BASE_SPEED;

// ------------------------------------------------------------------------------
// Cau hinh runtime (khoi tao tu config.h, doi duoc qua BLE)
// ------------------------------------------------------------------------------
static MotorRuntimeConfig runtimeConfig = {
    INVERT_LEFT_MOTORS,
    INVERT_RIGHT_MOTORS,
    SWAP_LEFT_RIGHT_MOTORS,
    MOTOR_MIN_PWM_LEFT,
    MOTOR_MIN_PWM_RIGHT,
    MOTOR_DEADBAND_PWM_LEFT,
    MOTOR_DEADBAND_PWM_RIGHT,
};

const MotorRuntimeConfig& getMotorRuntimeConfig() {
    return runtimeConfig;
}

MotorRuntimeConfig getMotorConfigCopy() {
    return runtimeConfig;
}

void applyMotorRuntimeConfig(const MotorRuntimeConfig& config) {
    runtimeConfig = config;
    runtimeConfig.minPwmLeft = constrain(runtimeConfig.minPwmLeft, 0, 255);
    runtimeConfig.minPwmRight = constrain(runtimeConfig.minPwmRight, 0, 255);
    runtimeConfig.deadbandLeft = constrain(runtimeConfig.deadbandLeft, 0, 255);
    runtimeConfig.deadbandRight = constrain(runtimeConfig.deadbandRight, 0, 255);
    // Van toc do co ban phai nam trong khoang cho phep
    motorBaseSpeed = constrain(motorBaseSpeed, MOTOR_MIN_SPEED, MOTOR_MAX_SPEED);
}

void toggleInvertLeft() {
    runtimeConfig.invertLeft = !runtimeConfig.invertLeft;
}

void toggleInvertRight() {
    runtimeConfig.invertRight = !runtimeConfig.invertRight;
}

void toggleSwapLeftRight() {
    runtimeConfig.swapLeftRight = !runtimeConfig.swapLeftRight;
}

void setMotorMinPwm(int left, int right) {
    runtimeConfig.minPwmLeft = constrain(left, 0, 255);
    runtimeConfig.minPwmRight = constrain(right, 0, 255);
}

void setMotorDeadband(int left, int right) {
    runtimeConfig.deadbandLeft = constrain(left, 0, 255);
    runtimeConfig.deadbandRight = constrain(right, 0, 255);
}

// ------------------------------------------------------------------------------
// Kenh dong co
// ------------------------------------------------------------------------------
struct MotorChannel {
    const int in1;
    const int in2;
    const int pwmPin;
    const int pwmChannel;
    int outputPercent;
    int minimumOutputDuty;     // Auto: san PWM THUC TE sau scale; manual = 0 (giu ramp cu)

    // Huong dien tinh hien tai tren chan huong (dang giu khi PWM = 0)
    int electricalDirection;   // -1 / 0 / +1
    int commandedDirection;     // Huong duoc lenh moi nhat (chua apply)

    int targetDuty;             // Duty mong muon
    int appliedDuty;            // Duty thuc te dang xuat ra
    int commandedSpeed;         // Van toc do lenh theo frame robot
    int rampAccum;              // Vi tri ramp tinh theo milli-duty (1 duty = 1000)

    bool boosting;
    unsigned long boostStartedAt;
    unsigned long lastRampAt;

    // Thoi gian chan ENA/ENB o muc HIGH trong khi ta phai giu LOW (phat hien jumper)
    unsigned long enableHighSince;

    bool hasBeenPowered;
    unsigned long lastPoweredAt;
};

// Kenh vat ly: B = OUT3/OUT4 (trai mac dinh), A = OUT1/OUT2 (phai mac dinh)
static MotorChannel channelB{
    PIN_MOTOR_LEFT_IN3, PIN_MOTOR_LEFT_IN4, PIN_MOTOR_LEFT_ENB, PWM_CH_LEFT,
    100, 0, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, false, 0
};
static MotorChannel channelA{
    PIN_MOTOR_RIGHT_IN1, PIN_MOTOR_RIGHT_IN2, PIN_MOTOR_RIGHT_ENA, PWM_CH_RIGHT,
    100, 0, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, false, 0
};

// Lenh BLE chay tren task khac voi vong lap cap nhat PWM.
static std::mutex motorMutex;

// ------------------------------------------------------------------------------
// Ghi PWM / GPIO
// ------------------------------------------------------------------------------
static int motorOutputDuty(const MotorChannel& motor, int duty) {
    if (duty == 0) return 0;
    const int scaled = (constrain(duty, 0, 255) * motor.outputPercent) / 100;
    if (motor.minimumOutputDuty == 0) return scaled; // Manual giu hanh vi cu.
    // De-pa auto phai du luc; khong scale no xuong duoi nguong khoi hanh.
    if (motor.boosting) return max(motor.minimumOutputDuty, MOTOR_START_PWM);
    // Power chi giam phan tren san, khong nhan ca san: 165 + (185-165)*65% = 178.
    const int aboveMinimum = max(0, constrain(duty, 0, 255) - motor.minimumOutputDuty);
    return motor.minimumOutputDuty + aboveMinimum * motor.outputPercent / 100;
}

static void rememberPowered(MotorChannel& motor, unsigned long now) {
    if (motorOutputDuty(motor, motor.appliedDuty) > 0) {
        motor.hasBeenPowered = true;
        motor.lastPoweredAt = now;
    }
}

static void writeMotorPwm(const MotorChannel& motor, int duty) {
    // Scale sau min PWM/dead-band/de-pa de khong bi cac nguong nay day toc do len lai.
    const int outputDuty = motorOutputDuty(motor, duty);
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    ledcWrite(motor.pwmPin, outputDuty);
#else
    ledcWrite(motor.pwmChannel, outputDuty);
#endif
}

static void writeDirectionPins(MotorChannel& motor, int direction) {
    digitalWrite(motor.in1, direction > 0 ? HIGH : LOW);
    digitalWrite(motor.in2, direction < 0 ? HIGH : LOW);
}

// ------------------------------------------------------------------------------
// Khoi tao
// ------------------------------------------------------------------------------
void motorDriverInit() {
    // Cau hinh cac chan huong IN1..IN4 la OUTPUT
    pinMode(PIN_MOTOR_RIGHT_IN1, OUTPUT);
    pinMode(PIN_MOTOR_RIGHT_IN2, OUTPUT);
    pinMode(PIN_MOTOR_LEFT_IN3, OUTPUT);
    pinMode(PIN_MOTOR_LEFT_IN4, OUTPUT);

    // Dung dong co ban dau
    digitalWrite(PIN_MOTOR_RIGHT_IN1, LOW);
    digitalWrite(PIN_MOTOR_RIGHT_IN2, LOW);
    digitalWrite(PIN_MOTOR_LEFT_IN3, LOW);
    digitalWrite(PIN_MOTOR_LEFT_IN4, LOW);

    // Thiet lap PWM cho ENB (OUT3/4) va ENA (OUT1/2).
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    // Arduino ESP32 Core 3.x
    ledcAttach(PIN_MOTOR_LEFT_ENB, PWM_FREQ, PWM_RESOLUTION);
    ledcAttach(PIN_MOTOR_RIGHT_ENA, PWM_FREQ, PWM_RESOLUTION);
    ledcWrite(PIN_MOTOR_LEFT_ENB, 0);
    ledcWrite(PIN_MOTOR_RIGHT_ENA, 0);
#else
    // Arduino ESP32 Core 2.x
    ledcSetup(PWM_CH_LEFT, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(PIN_MOTOR_LEFT_ENB, PWM_CH_LEFT);
    ledcWrite(PWM_CH_LEFT, 0);

    ledcSetup(PWM_CH_RIGHT, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(PIN_MOTOR_RIGHT_ENA, PWM_CH_RIGHT);
    ledcWrite(PWM_CH_RIGHT, 0);
#endif

    auto resetChannel = [](MotorChannel& motor) {
        motor.outputPercent = 100;
        motor.minimumOutputDuty = 0;
        motor.electricalDirection = 0;
        motor.commandedDirection = 0;
        motor.targetDuty = 0;
        motor.appliedDuty = 0;
        motor.commandedSpeed = 0;
        motor.rampAccum = 0;
        motor.boosting = false;
        motor.boostStartedAt = 0;
        motor.lastRampAt = millis();
        motor.enableHighSince = 0;
        motor.hasBeenPowered = false;
        motor.lastPoweredAt = 0;
    };
    resetChannel(channelA);
    resetChannel(channelB);

    Serial.println("[Motor] L298N Trai=OUT3/4, Phai=OUT1/2 (doi cuc qua BLE: XI/XR/XS)");
}

// ------------------------------------------------------------------------------
// Chuyen van toc do chuan hoa -> duty PWM
// ------------------------------------------------------------------------------
static int normalizedSpeedToDuty(int speed) {
    const int magnitude = constrain(abs(speed), 0, MOTOR_SPEED_SCALE);
    return (int)((long)magnitude * 255L / MOTOR_SPEED_SCALE);
}

// ------------------------------------------------------------------------------
// Ramp duty + phat hien jumper ENA/ENB
// ------------------------------------------------------------------------------
// Ramp duty tu appliedDuty toi targetDuty trong khoang MOTOR_RAMP_MS.
// Day la thay the cho viec nhay duty gac ngay, giup xe khong "giat" khi
// chuyen huong va tao duong cong khi ben trong giam toc do.
// Vi tri ramp duoc giu voi don vi "milli-duty" (1 duty = 1000) de buoc
// tang khong bi cat bang so nguyen.
static void rampMotorChannel(MotorChannel& motor, unsigned long now) {
    // --- Het nhip de-pa: chuyen sang ramp binh thuong ---
    const unsigned long boostMs = motor.minimumOutputDuty > 0 ? MOTOR_AUTO_START_MS : MOTOR_START_MS;
    if (motor.boosting && now - motor.boostStartedAt >= boostMs) {
        motor.boosting = false;
        motor.rampAccum = motor.appliedDuty * 1000;
    }

    // --- Chuyen huong dien tinh ---
    if (motor.commandedDirection != 0 && motor.commandedDirection != motor.electricalDirection) {
        // Cat PWM truoc khi doi huong de tranh nhoe/spark
        writeMotorPwm(motor, 0);
        motor.appliedDuty = 0;
        motor.rampAccum = 0;
        motor.electricalDirection = motor.commandedDirection;
        writeDirectionPins(motor, motor.electricalDirection);
        motor.lastRampAt = now;
        // KHONG return o day: nhip de-pa phai duoc ap ngay trong cung mot
        // vong lap de dong co bat len dau chay ngay khi lenh moi den.
    }

    // --- Nhip de-pa: ap duty cao ngay lap tuc de pha ma sat tinh ---
    if (motor.boosting) {
        const int boostDuty = max(motor.targetDuty, MOTOR_START_PWM);
        if (motor.appliedDuty != boostDuty) {
            motor.appliedDuty = boostDuty;
            motor.rampAccum = boostDuty * 1000;
            writeMotorPwm(motor, motor.appliedDuty);
        }
        return;
    }

    // --- Ramp duty toi target ---
    const unsigned long elapsed = now - motor.lastRampAt;
    if (elapsed == 0) return;
    motor.lastRampAt = now;

    // Buoc tang toi da = 255 duty / MOTOR_RAMP_MS (tinh theo milli-duty)
    const int maxStepMilli = (255 * 1000 * (int)elapsed) / (int)MOTOR_RAMP_MS;
    if (maxStepMilli < 1) return;

    const int targetMilli = motor.targetDuty * 1000;
    int accum = motor.rampAccum;

    if (accum == targetMilli) {
        // Da den dich: neu dang dung, tha chan huong de khong "chan" dong co
        if (motor.appliedDuty == 0 && motor.commandedDirection == 0 && motor.electricalDirection != 0) {
            writeDirectionPins(motor, 0);
            motor.electricalDirection = 0;
        }
        return;
    }

    int delta = targetMilli - accum;
    if (delta > maxStepMilli) delta = maxStepMilli;
    else if (delta < -maxStepMilli) delta = -maxStepMilli;

    accum += delta;
    motor.rampAccum = accum;

    const int newDuty = accum / 1000;
    if (newDuty != motor.appliedDuty) {
        motor.appliedDuty = newDuty;
        writeMotorPwm(motor, motor.appliedDuty);
    }

    if (motor.appliedDuty == 0 && motor.commandedDirection == 0 && motor.electricalDirection != 0) {
        writeDirectionPins(motor, 0);
        motor.electricalDirection = 0;
    }
}

// Phat hien jumper 5V tren ENA/ENB: neu ta phai giu chan ở LOW
// (duty = 0) ma chan van doc HIGH, thi chan bi keo len 5V boi jumper.
static void checkEnableJumper(MotorChannel& motor, int enablePin, bool& warned) {
    if (motor.appliedDuty != 0 || motor.targetDuty != 0) {
        return;  // Dang chay, khong the kiem tra
    }
    if (digitalRead(enablePin) == HIGH) {
        if (motor.enableHighSince == 0) {
            motor.enableHighSince = millis();
        } else if (millis() - motor.enableHighSince > 300 && !warned) {
            warned = true;
            Serial.printf("[Motor] CANH BAO: GPIO %d (ENA/ENB) doc HIGH du duty=0.\n",
                          enablePin);
            Serial.println("[Motor] Hay RUT jumper ENA & ENB tren L298N de ESP32 dieu khien PWM!");
        }
    } else {
        motor.enableHighSince = 0;
    }
}

bool motorDriverUpdate() {
    const std::lock_guard<std::mutex> lock(motorMutex);
    const unsigned long now = millis();

    static bool jumperWarned = false;

    rampMotorChannel(channelA, now);
    rampMotorChannel(channelB, now);

    // Khi duty khong doi, ramp khong ghi PWM lai, nhung kenh van dang duoc cap dien.
    rememberPowered(channelA, now);
    rememberPowered(channelB, now);

    checkEnableJumper(channelA, PIN_MOTOR_RIGHT_ENA, jumperWarned);
    checkEnableJumper(channelB, PIN_MOTOR_LEFT_ENB, jumperWarned);

    return false;
}

// ------------------------------------------------------------------------------
// Cap nhat 1 kenh dong co theo van toc do da lenh (frame robot)
// ------------------------------------------------------------------------------
// speed     : van toc do lenh trong frame robot (+ = tien)
// invert    : co dao chieu quay vat ly cua kenh nay khong
// deadband  : yeu cau duty nho hon -> DUNG THAT
// minPwm    : duty nho nhat ma van chay duoc
static void updateMotorChannel(MotorChannel& motor, int speed, bool invert,
                               int deadband, int minPwm, unsigned long now) {
    // Cuc thuoc tinh vat ly: chi doi dau, KHONG doi do lon yeu cau
    const int physicalSpeed = invert ? -speed : speed;
    const int direction = (physicalSpeed > 0) - (physicalSpeed < 0);

    motor.commandedSpeed = speed;
    motor.commandedDirection = direction;

    if (direction == 0) {
        // Dung that: ramp duty xuong 0 roi tha chan huong
        motor.boosting = false;
        motor.targetDuty = 0;
        return;
    }

    int duty = normalizedSpeedToDuty(abs(physicalSpeed));
    if (duty > 255) duty = 255;

    // Vung dead-band: yeu cau qua nho -> DUNG THAT thay vi keo hai
    if (duty < deadband) {
        motor.boosting = false;
        motor.targetDuty = 0;
        return;
    }

    // Ngoai dai co truot: luon chay o duty nho nhat da biet la chay duoc
    if (duty < minPwm) duty = minPwm;

    // Auto chi de-pa lan dau hoac sau khi ngat PWM du lau. Khong de-pa lai
    // moi lan banh trong tam ngung vai ms de chinh huong/xac nhan line.
    // Manual giu nhip de-pa cu moi lan tu dung sang chay.
    if (motor.targetDuty == 0) {
        motor.boosting = motor.minimumOutputDuty == 0 || !motor.hasBeenPowered ||
                         now - motor.lastPoweredAt >= MOTOR_AUTO_RESTART_MS;
        if (motor.boosting) motor.boostStartedAt = now;
    }

    motor.targetDuty = duty;
}

// ------------------------------------------------------------------------------
// API DIEU KHIEN CHINH
// ------------------------------------------------------------------------------
void runLR(int vL, int vR, int powerPercent, bool immediate) {
    vL = constrain(vL, -MOTOR_SPEED_SCALE, MOTOR_SPEED_SCALE);
    vR = constrain(vR, -MOTOR_SPEED_SCALE, MOTOR_SPEED_SCALE);

    const std::lock_guard<std::mutex> lock(motorMutex);
    const unsigned long now = millis();

    // Kenh vat ly: B = OUT3/OUT4, A = OUT1/OUT2.
    // SWAP_LEFT_RIGHT_MOTORS = "ben trai cua xe lai noi OUT1/OUT2".
    // Vì vậy moi kenh vat ly phai giu "cuc" (INVERT) CUA BEN XE no phuc vu,
    // chứ không phải của chính kênh đó.
    MotorChannel* leftChannel = runtimeConfig.swapLeftRight ? &channelA : &channelB;
    MotorChannel* rightChannel = runtimeConfig.swapLeftRight ? &channelB : &channelA;

    rememberPowered(*leftChannel, now);
    rememberPowered(*rightChannel, now);
    leftChannel->minimumOutputDuty = immediate ? runtimeConfig.minPwmLeft : 0;
    rightChannel->minimumOutputDuty = immediate ? runtimeConfig.minPwmRight : 0;

    // Khi dung giu power cu de khong tang PWM bat ngo trong luc ramp xuong 0.
    if (vL != 0 && leftChannel->outputPercent != constrain(powerPercent, 0, 100)) {
        leftChannel->outputPercent = constrain(powerPercent, 0, 100);
        writeMotorPwm(*leftChannel, leftChannel->appliedDuty);
    }
    if (vR != 0 && rightChannel->outputPercent != constrain(powerPercent, 0, 100)) {
        rightChannel->outputPercent = constrain(powerPercent, 0, 100);
        writeMotorPwm(*rightChannel, rightChannel->appliedDuty);
    }

    updateMotorChannel(*leftChannel, vL, runtimeConfig.invertLeft,
                        runtimeConfig.deadbandLeft, runtimeConfig.minPwmLeft, now);
    updateMotorChannel(*rightChannel, vR, runtimeConfig.invertRight,
                        runtimeConfig.deadbandRight, runtimeConfig.minPwmRight, now);

    if (immediate) {
        // Auto bo ramp nhung van de-pa NGAN khi tu dung sang chay.
        auto applyNow = [now](MotorChannel& motor) {
            if (motor.targetDuty == 0 || now - motor.boostStartedAt >= MOTOR_AUTO_START_MS) {
                motor.boosting = false;
            }
            const int direction = motor.targetDuty == 0 ? 0 : motor.commandedDirection;
            if (direction != motor.electricalDirection) {
                writeMotorPwm(motor, 0);
                writeDirectionPins(motor, direction);
                motor.electricalDirection = direction;
            }
            motor.appliedDuty = motor.targetDuty;
            motor.rampAccum = motor.appliedDuty * 1000;
            motor.lastRampAt = now;
            writeMotorPwm(motor, motor.appliedDuty);
        };
        applyNow(*leftChannel);
        applyNow(*rightChannel);
        static unsigned long lastAutoLogAt = 0;
        if (now - lastAutoLogAt >= 500) {
            lastAutoLogAt = now;
            Serial.printf("[AutoMotor] Cmd L=%d R=%d | PWM L=%d R=%d | Min L=%d R=%d\n",
                          vL, vR, motorOutputDuty(*leftChannel, leftChannel->appliedDuty),
                          motorOutputDuty(*rightChannel, rightChannel->appliedDuty),
                          runtimeConfig.minPwmLeft, runtimeConfig.minPwmRight);
        }
        return;
    }

    // Ramp ngay neu vua lenh (neu chay o che do do line, motorDriverUpdate se lo phan ramp)
    rampMotorChannel(channelB, now);
    rampMotorChannel(channelA, now);
}

void runForward(int speed) {
    runLR(speed, speed);
}

void runBackward(int speed) {
    runLR(-speed, -speed);
}

void runTurnLeft(int speed) {
    // Xoay sang TRAI: ben phai chay tien, ben trai chay lui
    runLR(-speed, speed);
}

void runTurnRight(int speed) {
    // Xoay sang PHAI: ben trai chay tien, ben phai chay lui
    runLR(speed, -speed);
}

void runArcLeft(int speed) {
    // Re cong sang trai: ben trong (trai) chay CHAM, ben ngoai (phai) chay nhanh.
    // Ca 2 ben cung chieu tien (khong doi chieu) - ben trong chi cham hon.
    const int inner = (int)((long)speed * MOTOR_ARC_INNER_RATIO / 100L);
    runLR(inner, speed);
}

void runArcRight(int speed) {
    // Re cong sang phai: ben trong (phai) chay CHAM, ben ngoai (trai) chay nhanh.
    const int inner = (int)((long)speed * MOTOR_ARC_INNER_RATIO / 100L);
    runLR(speed, inner);
}

void stopMotion() {
    runLR(0, 0);
}

// ------------------------------------------------------------------------------
// WRAPPER CU (van toc do don vi PWM 0-255 nhu truoc)
// ------------------------------------------------------------------------------
// Chuyen duty PWM (0-255) sang van toc do chuan hoa (-1000..+1000)
static int pwmToNormalized(int pwm) {
    return (int)((long)constrain(pwm, -255, 255) * MOTOR_SPEED_SCALE / 255L);
}

void setMotorSpeeds(int leftSpeed, int rightSpeed) {
    // Chuyen tu PWM 0-255 sang van toc do chuan hoa
    runLR(pwmToNormalized(leftSpeed), pwmToNormalized(rightSpeed));
}

void moveForward(int speed) {
    runForward(pwmToNormalized(constrain(speed, 0, 255)));
}

void moveBackward(int speed) {
    runBackward(pwmToNormalized(constrain(speed, 0, 255)));
}

void turnLeft(int speed) {
    // Re cong (khong doi chieu) - ben trong chay cham
    const int outer = pwmToNormalized(constrain(speed, 0, 255));
    const int inner = (int)((long)outer * MOTOR_ARC_INNER_RATIO / 100L);
    runLR(inner, outer);
}

void turnRight(int speed) {
    const int outer = pwmToNormalized(constrain(speed, 0, 255));
    const int inner = (int)((long)outer * MOTOR_ARC_INNER_RATIO / 100L);
    runLR(outer, inner);
}

void spinLeft(int speed) {
    runTurnLeft(pwmToNormalized(constrain(speed, 0, 255)));
}

void spinRight(int speed) {
    runTurnRight(pwmToNormalized(constrain(speed, 0, 255)));
}

void stopMotors() {
    stopMotion();
}

// ------------------------------------------------------------------------------
// CHAN DOAN
// ------------------------------------------------------------------------------
String getMotorDiagnostics() {
    const std::lock_guard<std::mutex> lock(motorMutex);

    String s = "\n=== CHAN DOAN DONG CO ===\n";
    s += "Cuc  : INV_L=" + String(runtimeConfig.invertLeft ? "ON" : "OFF");
    s += " INV_R=" + String(runtimeConfig.invertRight ? "ON" : "OFF");
    s += " SWAP=" + String(runtimeConfig.swapLeftRight ? "ON" : "OFF") + "\n";

    s += "PWM  : min_L=" + String(runtimeConfig.minPwmLeft) + " min_R=" + String(runtimeConfig.minPwmRight);
    s += " dead_L=" + String(runtimeConfig.deadbandLeft) + " dead_R=" + String(runtimeConfig.deadbandRight) + "\n";

    s += "Base : " + String(motorBaseSpeed) + " (chuan hoa -" + String(MOTOR_SPEED_SCALE) +
         "..+" + String(MOTOR_SPEED_SCALE) + ")\n";
    s += "Auto power: " + String(MOTOR_OUTPUT_PERCENT) + "% | Manual: 100%\n";

    s += "\n-- BEN TRAI (OUT3/OUT4, ENB=GPIO " + String(PIN_MOTOR_LEFT_ENB) + ") --\n";
    s += "  lenh=" + String(channelB.commandedSpeed) + " dir=" + String(channelB.electricalDirection);
    s += " duty=" + String(motorOutputDuty(channelB, channelB.appliedDuty)) + "/" + String(motorOutputDuty(channelB, channelB.targetDuty));
    if (channelB.boosting) s += " [DEP-A]";
    s += "\n";
    s += "  IN3(GPIO " + String(PIN_MOTOR_LEFT_IN3) + ")=" + String(digitalRead(PIN_MOTOR_LEFT_IN3));
    s += " IN4(GPIO " + String(PIN_MOTOR_LEFT_IN4) + ")=" + String(digitalRead(PIN_MOTOR_LEFT_IN4));
    s += " ENB=" + String(digitalRead(PIN_MOTOR_LEFT_ENB)) + "\n";

    s += "\n-- BEN PHAI (OUT1/OUT2, ENA=GPIO " + String(PIN_MOTOR_RIGHT_ENA) + ") --\n";
    s += "  lenh=" + String(channelA.commandedSpeed) + " dir=" + String(channelA.electricalDirection);
    s += " duty=" + String(motorOutputDuty(channelA, channelA.appliedDuty)) + "/" + String(motorOutputDuty(channelA, channelA.targetDuty));
    if (channelA.boosting) s += " [DEP-A]";
    s += "\n";
    s += "  IN1(GPIO " + String(PIN_MOTOR_RIGHT_IN1) + ")=" + String(digitalRead(PIN_MOTOR_RIGHT_IN1));
    s += " IN2(GPIO " + String(PIN_MOTOR_RIGHT_IN2) + ")=" + String(digitalRead(PIN_MOTOR_RIGHT_IN2));
    s += " ENA=" + String(digitalRead(PIN_MOTOR_RIGHT_ENA)) + "\n";

    s += "\n-- KIEM TRA JUMPER ENA/ENB --\n";
    const int enaLevel = digitalRead(PIN_MOTOR_RIGHT_ENA);
    const int enbLevel = digitalRead(PIN_MOTOR_LEFT_ENB);
    const bool enaTied = (enaLevel == HIGH) && channelA.appliedDuty == 0 && channelA.targetDuty == 0;
    const bool enbTied = (enbLevel == HIGH) && channelB.appliedDuty == 0 && channelB.targetDuty == 0;
    if (enaTied || enbTied) {
        s += "  CANH BAO: ENA/ENB dang keo len HIGH khi duty=0.\n";
        s += "  => CON JUMPER 5V tren L298N! Hay RUT jumper ENA & ENB.\n";
    } else {
        s += "  ENA=" + String(enaLevel) + " ENB=" + String(enbLevel) + " (OK)\n";
    }
    s += "==========================\n";
    return s;
}

// ------------------------------------------------------------------------------
// TU KIEM TRA DONG CO
// ------------------------------------------------------------------------------
namespace {
struct SelfTestStep {
    int leftSpeed;
    int rightSpeed;
    const char* label;
};

const SelfTestStep kSelfTestSteps[] = {
    {MOTOR_SELFTEST_SPEED, 0, "TRAI tien"},
    {0, MOTOR_SELFTEST_SPEED, "PHAI tien"},
    {-MOTOR_SELFTEST_SPEED, 0, "TRAI lui"},
    {0, -MOTOR_SELFTEST_SPEED, "PHAI lui"},
};

constexpr size_t kSelfTestStepCount = sizeof(kSelfTestSteps) / sizeof(kSelfTestSteps[0]);
}  // namespace

bool runMotorSelfTest(unsigned long stepMs) {
    static size_t step = 0;
    static unsigned long startedAt = 0;
    static bool running = false;

    const unsigned long now = millis();
    if (!running) {
        running = true;
        step = 0;
        startedAt = now;
        Serial.println("[SelfTest] BAT DAU tu kiem tra dong co...");
    }

    if (step < kSelfTestStepCount) {
        if (now - startedAt < stepMs) {
            return false;
        }
        const SelfTestStep& s = kSelfTestSteps[step];
        Serial.printf("[SelfTest] %s\n", s.label);
        runLR(s.leftSpeed, s.rightSpeed);
        ++step;
        startedAt = now;
        return false;
    }

    stopMotion();
    running = false;
    Serial.println("[SelfTest] XONG. Neu 2 ben khong cung chieu, dung XI/XR de sua.");
    return true;
}
