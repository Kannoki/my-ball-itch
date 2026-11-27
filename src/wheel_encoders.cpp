#include "config.h"

#ifdef SENSOR_PHOTORESISTOR
#include "wheel_encoders.h"

#include <driver/gpio.h>

static constexpr WheelOdometryGeometry geometry{
    FRONT_WHEEL_DIAMETER_MM, FRONT_TRACK_WIDTH_MM, ENCODER_PULSES_PER_REV
};
static portMUX_TYPE encoderMux = portMUX_INITIALIZER_UNLOCKED;
static bool encodersReady = false;

// Quang duong di chuyen duoc tren moi xung (mm).
static constexpr float MM_PER_PULSE =
    3.14159265f * FRONT_WHEEL_DIAMETER_MM / ENCODER_PULSES_PER_REV;

struct WheelEncoder {
    int32_t pulses = 0;
    int direction = 0;
};

static WheelEncoder leftEncoder;
static WheelEncoder rightEncoder;

// Lich su dem xung de uoc luong van toc do theo cua so thoi gian.
struct VelocityWindow {
    int32_t pulses = 0;
    unsigned long startedAt = 0;
    bool initialized = false;
};

static VelocityWindow leftWindow;
static VelocityWindow rightWindow;

static void ARDUINO_ISR_ATTR countWheelPulse(void* argument) {
    auto& encoder = *static_cast<WheelEncoder*>(argument);
    portENTER_CRITICAL_ISR(&encoderMux);
    encoder.pulses += encoder.direction;
    portEXIT_CRITICAL_ISR(&encoderMux);
}

static bool availableEncoderPin(int pin) {
    if (!GPIO_IS_VALID_GPIO(pin)) return false;
    static constexpr int usedPins[] = {
        1, 3, // UART0
        PIN_LINE_S1, PIN_LINE_S2, PIN_LINE_S3, PIN_LINE_S4,
        PIN_LINE_S5, PIN_LINE_S6, PIN_LINE_S7, PIN_LINE_S8,
        PIN_MOTOR_RIGHT_ENA, PIN_MOTOR_RIGHT_IN1, PIN_MOTOR_RIGHT_IN2,
        PIN_MOTOR_LEFT_ENB, PIN_MOTOR_LEFT_IN3, PIN_MOTOR_LEFT_IN4,
        PIN_SERVO_1, PIN_SERVO_2, PIN_SERVO_3
    };
    for (const int usedPin : usedPins) {
        if (pin == usedPin) return false;
    }
    return true;
}

void wheelEncodersInit() {
    if (PIN_ENCODER_LEFT < 0 || PIN_ENCODER_RIGHT < 0 || geometry.wheelDiameterMm <= 0.0f) {
        Serial.println("[Encoder] Can cau hinh hai GPIO OUT va duong kinh banh truoc.");
        return;
    }
    if (PIN_ENCODER_LEFT == PIN_ENCODER_RIGHT ||
        !availableEncoderPin(PIN_ENCODER_LEFT) || !availableEncoderPin(PIN_ENCODER_RIGHT)) {
        Serial.println("[Encoder] GPIO khong hop le hoac trung voi chan dang dung.");
        return;
    }

    pinMode(PIN_ENCODER_LEFT, INPUT);
    pinMode(PIN_ENCODER_RIGHT, INPUT);
    attachInterruptArg(PIN_ENCODER_LEFT, countWheelPulse, &leftEncoder, RISING);
    attachInterruptArg(PIN_ENCODER_RIGHT, countWheelPulse, &rightEncoder, RISING);
    encodersReady = true;
}

bool wheelEncodersReady() {
    return encodersReady;
}

void resetWheelEncoderCounts() {
    portENTER_CRITICAL(&encoderMux);
    leftEncoder.pulses = 0;
    rightEncoder.pulses = 0;
    leftWindow.pulses = 0;
    leftWindow.startedAt = 0;
    leftWindow.initialized = false;
    rightWindow.pulses = 0;
    rightWindow.startedAt = 0;
    rightWindow.initialized = false;
    portEXIT_CRITICAL(&encoderMux);
}

void setWheelEncoderDirections(int left, int right) {
    if (left == leftEncoder.direction && right == rightEncoder.direction) return;
    portENTER_CRITICAL(&encoderMux);
    leftEncoder.direction = left;
    rightEncoder.direction = right;
    portEXIT_CRITICAL(&encoderMux);
}

// So xung da chuan hoa theo he so trim: dung cho ca YAW, quang trong goc quay
// va uoc luong van toc do. Khi ENCODER_PULSE_SCALE_* = 1.0 thi ho toan goc nguyen.
WheelEncoderCounts getWheelEncoderCounts() {
    portENTER_CRITICAL(&encoderMux);
    const int32_t leftPulses = leftEncoder.pulses;
    const int32_t rightPulses = rightEncoder.pulses;
    portEXIT_CRITICAL(&encoderMux);

    const WheelEncoderCounts counts{
        static_cast<int32_t>(static_cast<float>(leftPulses) * ENCODER_PULSE_SCALE_LEFT),
        static_cast<int32_t>(static_cast<float>(rightPulses) * ENCODER_PULSE_SCALE_RIGHT)
    };
    return counts;
}

const WheelOdometryGeometry& getWheelOdometryGeometry() {
    return geometry;
}

float getEncoderPulseScaleLeft() {
    return ENCODER_PULSE_SCALE_LEFT;
}

float getEncoderPulseScaleRight() {
    return ENCODER_PULSE_SCALE_RIGHT;
}

WheelEncoderVelocity getWheelEncoderVelocity(unsigned long now) {
    WheelEncoderVelocity velocity{0.0f, 0.0f, false};

    portENTER_CRITICAL(&encoderMux);
    const int32_t leftPulses = leftEncoder.pulses;
    const int32_t rightPulses = rightEncoder.pulses;

    // Khoi tao cua so lai lan dau.
    if (!leftWindow.initialized) {
        leftWindow.pulses = leftPulses;
        leftWindow.startedAt = now;
        leftWindow.initialized = true;
    }
    if (!rightWindow.initialized) {
        rightWindow.pulses = rightPulses;
        rightWindow.startedAt = now;
        rightWindow.initialized = true;
    }
    portEXIT_CRITICAL(&encoderMux);

    const unsigned long leftElapsed = now - leftWindow.startedAt;
    const unsigned long rightElapsed = now - rightWindow.startedAt;

    // Dung cua so lon nhat de ca hai ben cung chay tren cung mot moc thoi gian.
    const unsigned long elapsed = leftElapsed < rightElapsed ? leftElapsed : rightElapsed;
    if (elapsed < SPEED_PID_WINDOW_MS) {
        return velocity;
    }

    portENTER_CRITICAL(&encoderMux);
    const int32_t leftDelta = leftPulses - leftWindow.pulses;
    const int32_t rightDelta = rightPulses - rightWindow.pulses;
    leftWindow.pulses = leftPulses;
    leftWindow.startedAt = now;
    rightWindow.pulses = rightPulses;
    rightWindow.startedAt = now;
    portEXIT_CRITICAL(&encoderMux);

    const float seconds = static_cast<float>(elapsed) / 1000.0f;
    if (seconds <= 0.0f) {
        return velocity;
    }

    // Do duoc bang don vi quang duong thuc.
    const float leftPulsesScaled = static_cast<float>(leftDelta) * ENCODER_PULSE_SCALE_LEFT;
    const float rightPulsesScaled = static_cast<float>(rightDelta) * ENCODER_PULSE_SCALE_RIGHT;

    velocity.leftMmPerSecond = leftPulsesScaled * MM_PER_PULSE / seconds;
    velocity.rightMmPerSecond = rightPulsesScaled * MM_PER_PULSE / seconds;
    velocity.valid = true;
    return velocity;
}
#endif
