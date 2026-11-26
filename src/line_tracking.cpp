#include "line_tracking.h"

namespace {
enum State { FOLLOW, OBSERVE, TURN_LEFT, TURN_RIGHT, VERIFY, RECOVER, STOPPED };

struct Sample {
    uint8_t raw;
    unsigned count;
    unsigned clusters;
    float position;
    bool left;
    bool right;
    bool centered;
    bool line;
};

State state = FOLLOW;
unsigned long startedAt = 0, lastSampleAt = 0, recoveryStartedAt = 0;
unsigned long eventStartedAt = 0;
uint8_t previousRaw = 0, olderRaw = 0;
uint8_t filteredRaw = 0;
unsigned sampleCount = 0, centerMs = 0, straightMs = 0, exitMs = 0, cornerMs = 0;
unsigned recoveryAttempts = 0;
bool recoveryActive = false;
bool eventActive = false, hasSeenLine = false;
bool foundLeft = false, foundRight = false, turnDeparted = false;
float lastPosition = 0.0f, previousError = 0.0f;
bool hasError = false;
int lastTurn = 0, recoveryDirection = 1, cornerDirection = 0;
int leftPwm = 0, rightPwm = 0;

void drive(int left, int right) {
    leftPwm = left;
    rightPwm = right;
}

void enter(State next, unsigned long now) {
    const char* previous = getLineTrackingStatus();
    state = next;
    startedAt = now;
    centerMs = straightMs = exitMs = cornerMs = 0;
    cornerDirection = 0;
    hasError = false;
    Serial.printf("[Line] %s -> %s | Raw=0x%02X\n", previous, getLineTrackingStatus(), static_cast<unsigned>(previousRaw));
}

// Neu co nhieu cum, chi lay cum gan line truoc do. Khong lay trung binh qua khoang trang.
Sample analyze(uint8_t raw) {
    Sample s{};
    s.raw = raw;
    s.left = (raw & 0x03) != 0;
    s.right = (raw & 0xC0) != 0;
    float nearest = 0.0f;
    for (int i = 0; i < 8; ++i) {
        if (!(raw & (1U << i))) continue;
        const int first = i;
        ++s.count;
        while (i + 1 < 8 && (raw & (1U << (i + 1)))) { ++i; ++s.count; }
        const float position = (first + i) * 0.5f - 3.5f;
        const float distance = fabsf(position - lastPosition);
        if (s.clusters == 0 || distance < nearest) {
            nearest = distance;
            s.position = position;
        }
        ++s.clusters;
    }
    // Mot line 20 mm voi buoc mat 6 mm thuong kich hoat 3-4 mat.
    s.line = s.count > 0 && s.count <= 4 && s.clusters == 1;
    s.centered = s.line && (raw & 0x18) && !s.left && !s.right && fabsf(s.position) <= 0.5f;
    return s;
}

bool suspicious(const Sample& s) {
    return s.count >= 5 || s.clusters > 1 ||
           (s.left && s.position <= -1.0f) || (s.right && s.position >= 1.0f);
}

void turn(int direction, int speed) {
    // Hai motor moi ben dung chung mot kenh. Tha cap trong, cap ngoai tien;
    // khong dao chieu ca cap trong khi bam line hay quet tim line.
    if (direction < 0) drive(0, speed);
    else drive(speed, 0);
}

void follow(const Sample& s, int speed, unsigned long elapsed) {
    lastPosition = s.position;
    // Cum 3 mat o giua co tam +/-0.5 do so mat le, van la di thang.
    // Dung cung quy tac centered voi phan nhan line, khong vua cong nhan
    // o giua vua phat lenh hai cap khac PWM (185/165) lam xe tu lech.
    if (s.centered) {
        previousError = 0.0f;
        hasError = false;
        drive(speed, speed);
        return;
    }
    const float error = hasError ? previousError + LINE_ERROR_FILTER_ALPHA * (s.position - previousError) : s.position;
    const float derivative = hasError && elapsed ? (error - previousError) * PID_SAMPLE_INTERVAL_MS / elapsed : 0.0f;
    previousError = error;
    hasError = true;
    const float correction = PID_KP * error + PID_KD * derivative;
    // O speed=165, neu vua kep san 165 vua kep tran 165 thi mat het chenh lech PD.
    // Cho banh ngoai tang toi tran chinh huong; ca hai cap van tien khi bam line.
    // Chi TURN/RECOVER moi ngat cap trong, khong nhay 165 -> 0 o lech nhe.
    const int steeringLimit = min(MOTOR_MAX_SPEED_PWM, max(speed, LINE_STEER_PWM));
    drive(constrain(static_cast<int>(speed + correction), MOTOR_MIN_PWM_LEFT, steeringLimit),
          constrain(static_cast<int>(speed - correction), MOTOR_MIN_PWM_RIGHT, steeringLimit));
}

void beginRecovery(unsigned long now) {
    eventActive = false;
    if (!hasSeenLine) {
        enter(STOPPED, now);
        drive(0, 0);
        return;
    }
    if (!recoveryActive) {
        recoveryStartedAt = now;
        recoveryActive = true;
        recoveryAttempts = 0;
    }
    if (recoveryAttempts >= LINE_RECOVER_MAX_ATTEMPTS || now - recoveryStartedAt >= LINE_RECOVER_TOTAL_MS) {
        enter(STOPPED, now);
    } else {
        ++recoveryAttempts;
        recoveryDirection = lastTurn != 0 ? lastTurn : lastPosition < -0.25f ? -1 : 1;
        enter(RECOVER, now);
    }
    drive(0, 0);
}

void beginObserve(const Sample& s, unsigned long now) {
    if (!eventActive) {
        eventActive = true;
        eventStartedAt = now;
    }
    foundLeft = s.left;
    foundRight = s.right;
    enter(OBSERVE, now);
    drive(0, 0); // Giam ngay truoc khi bo cham de quan sat.
}

void beginTurn(int direction, const Sample& s, unsigned long now, int speed) {
    lastTurn = direction;
    turnDeparted = !s.centered;
    enter(direction < 0 ? TURN_LEFT : TURN_RIGHT, now);
    turn(direction, speed);
}
} // namespace

void resetLineTracking() {
    state = FOLLOW;
    startedAt = lastSampleAt = recoveryStartedAt = 0;
    eventStartedAt = 0;
    previousRaw = olderRaw = 0;
    filteredRaw = 0;
    sampleCount = centerMs = straightMs = exitMs = cornerMs = recoveryAttempts = 0;
    recoveryActive = foundLeft = foundRight = turnDeparted = hasError = false;
    eventActive = hasSeenLine = false;
    lastPosition = previousError = 0.0f;
    lastTurn = 0;
    recoveryDirection = 1;
    cornerDirection = 0;
    drive(0, 0);
}

void updateLineTracking(uint8_t raw, unsigned long now, int baseSpeedPwm) {
    const unsigned long elapsed = sampleCount == 0 ? LINE_SAMPLE_INTERVAL_MS : now - lastSampleAt;
    lastSampleAt = now;
    // Khong tinh mot lan loop bi tre thanh nhieu mau xac nhan lien tiep.
    const unsigned tick = static_cast<unsigned>(min(elapsed, static_cast<unsigned long>(LINE_SAMPLE_INTERVAL_MS)));
    const uint8_t filtered = (raw & previousRaw) | (raw & olderRaw) | (previousRaw & olderRaw);
    filteredRaw = filtered;
    olderRaw = previousRaw;
    previousRaw = raw;
    if (sampleCount < 3) ++sampleCount;
    if (sampleCount < 3) { drive(0, 0); return; }

    const Sample s = analyze(filtered);
    const Sample direct = analyze(raw);
    const int speed = constrain(baseSpeedPwm, MOTOR_MIN_SPEED_PWM, MOTOR_MAX_SPEED_PWM);
    const int slowSpeed = min(speed, LINE_SLOW_PWM);
    const int turnSpeed = min(speed, LINE_TURN_PWM);
    const bool centered = s.centered && direct.centered;
    const bool trackable = s.line && direct.line && !suspicious(s) && !suspicious(direct);

    if (state == STOPPED) {
        // Dung van doc lai sensor. Chi khoi dong lai khi line GIUA on dinh;
        // khong tu quet lai neu van mat line, nhieu cum hoac toan active.
        drive(0, 0);
        centerMs = centered ? centerMs + tick : 0;
        if (centerMs >= LINE_VERIFY_MS) {
            recoveryActive = eventActive = false;
            recoveryAttempts = 0;
            lastTurn = 0;
            hasSeenLine = true;
            enter(VERIFY, now);
            follow(s, slowSpeed, elapsed);
        }
        return;
    }
    // Chua tung bat duoc line thi khong bo toi de "kham pha" mot nen toan den.
    if (!hasSeenLine && !s.line) {
        enter(STOPPED, now);
        drive(0, 0);
        return;
    }
    if (recoveryActive && now - recoveryStartedAt >= LINE_RECOVER_TOTAL_MS) {
        enter(STOPPED, now);
        drive(0, 0);
        return;
    }
    if (eventActive && now - eventStartedAt >= LINE_EVENT_TIMEOUT_MS) {
        beginRecovery(now);
        return;
    }
    if (s.line && direct.line) hasSeenLine = true;

    if (state == RECOVER) {
        if (centered) {
            centerMs += tick;
            drive(0, 0);
            if (centerMs >= LINE_TURN_CENTER_MS) enter(VERIFY, now);
            return;
        }
        centerMs = 0;
        const unsigned long age = now - startedAt;
        if (age < LINE_RECOVER_PAUSE_MS) { drive(0, 0); return; }
        const unsigned long sweep = age - LINE_RECOVER_PAUSE_MS;
        if (sweep < LINE_RECOVER_SWEEP_MS) turn(recoveryDirection, turnSpeed);
        else if (sweep < LINE_RECOVER_SWEEP_MS * 3UL) turn(-recoveryDirection, turnSpeed);
        else if (sweep < LINE_RECOVER_SWEEP_MS * 4UL) turn(recoveryDirection, turnSpeed);
        else beginRecovery(now);
        return;
    }

    if (state == TURN_LEFT || state == TURN_RIGHT) {
        const int direction = state == TURN_LEFT ? -1 : 1;
        if (now - startedAt >= LINE_TURN_TIMEOUT_MS) { beginRecovery(now); return; }
        // Phai roi line cu truoc, tranh thay lai line cu roi ket luan da re xong.
        if (!(s.raw & 0x18) && !(direct.raw & 0x18)) turnDeparted = true;
        if (turnDeparted && centered) {
            centerMs += tick;
            drive(0, 0);
            if (now - startedAt >= LINE_TURN_MIN_MS && centerMs >= LINE_TURN_CENTER_MS) {
                enter(VERIFY, now);
                follow(s, slowSpeed, elapsed);
            }
        } else {
            centerMs = 0;
            turn(direction, turnSpeed);
        }
        return;
    }

    if (state == OBSERVE) {
        // Ghi nho nhanh xuat hien trong qua trinh, khong chi mot anh chup sensor.
        foundLeft |= s.left && direct.left;
        foundRight |= s.right && direct.right;
        const unsigned long age = now - startedAt;
        exitMs = trackable ? exitMs + tick : 0;

        // Line da ra khoi hai mat giua va chi con o mot ben: dung tien ngay.
        // Xac nhan tren ca mau tho va mau loc roi re, khong cho het 240 ms.
        const bool leftOnly = s.left && direct.left && !s.right && !direct.right;
        const bool rightOnly = s.right && direct.right && !s.left && !direct.left;
        const bool sideOnly = !(s.raw & 0x18) && !(direct.raw & 0x18) && (leftOnly || rightOnly);
        const int direction = sideOnly ? (leftOnly ? -1 : 1) : 0;
        cornerMs = sideOnly ? (direction == cornerDirection ? cornerMs + tick : tick) : 0;
        cornerDirection = direction;
        if (sideOnly) {
            drive(0, 0);
            if (cornerMs >= LINE_EXIT_CONFIRM_MS) beginTurn(direction, s, now, turnSpeed);
            return;
        }

        // Line don da ro lai thi bam theo, khong de nhanh cu giu lenh re/dung.
        if (exitMs >= LINE_EXIT_CONFIRM_MS) {
            enter(VERIFY, now);
            follow(s, slowSpeed, elapsed);
            return;
        }
        if ((!trackable && age >= LINE_OBSERVE_MAX_MS) ||
            (age >= LINE_OBSERVE_MIN_MS && s.count == 0 && direct.count == 0)) {
            if (s.count == 8) {
                beginRecovery(now); // Nen den rong/khong co phan xa: khong chay thang vo han.
            } else if (foundLeft) {
                beginTurn(-1, s, now, turnSpeed);
            } else if (foundRight) {
                beginTurn(1, s, now, turnSpeed);
            } else {
                beginRecovery(now);
            }
            return;
        }
        // Mat line khi quan sat thi dung tai cho, khong tiep tuc lao qua goc.
        if (s.count == 0 || direct.count == 0) drive(0, 0);
        else if (s.line) follow(s, slowSpeed, elapsed);
        else drive(slowSpeed, slowSpeed);
        return;
    }

    if (state == VERIFY) {
        // Sau re phai tien cham va bam duoc line, khong dung yen roi coi la thanh cong.
        if (s.count == 0) { beginRecovery(now); return; }
        if (suspicious(s)) { beginObserve(s, now); return; } // Cho phep zigzag khong co cooldown dai.
        // Cho phep line don hoi lech de PD tiep tuc chinh, khong bat buoc
        // dung tam tuyet doi roi moi cong nhan da bat lai line.
        centerMs = trackable ? centerMs + tick : 0;
        if (centerMs >= LINE_VERIFY_MS) {
            recoveryActive = false;
            eventActive = false;
            recoveryAttempts = 0;
            lastTurn = 0;
            enter(FOLLOW, now);
        } else if (now - startedAt >= LINE_VERIFY_MAX_MS) {
            beginRecovery(now);
            return;
        }
        follow(s, slowSpeed, elapsed);
        return;
    }

    // FOLLOW: toc do theo do tin cay. Khong doi den khi mat line moi giam toc.
    if (s.count == 0) { beginRecovery(now); return; }
    if (suspicious(s)) { beginObserve(s, now); return; }
    const bool stable = centered && (!hasError || fabsf(s.position - previousError) <= 0.5f);
    straightMs = stable ? min(straightMs + tick, static_cast<unsigned>(LINE_STRAIGHT_STABLE_MS)) : 0;
    const bool accelerating = straightMs >= LINE_STRAIGHT_STABLE_MS;
    const int cruise = accelerating ? speed : slowSpeed;
    follow(s, cruise, elapsed);
}

int getLineLeftPwm() { return leftPwm; }
int getLineRightPwm() { return rightPwm; }
uint8_t getLineFilteredRaw() { return filteredRaw; }

const char* getLineTrackingStatus() {
    switch (state) {
        case OBSERVE: return "OBSERVE";
        case TURN_LEFT: return "TURN_LEFT";
        case TURN_RIGHT: return "TURN_RIGHT";
        case VERIFY: return "VERIFY";
        case RECOVER: return "RECOVER";
        case STOPPED: return "STOPPED";
        default: return "FOLLOW";
    }
}
