#include "line_sensor.h"

// Mang chua cac chan GPIO cua cam bien theo thu tu tu trai qua phai
static const uint8_t sensorPins[NUM_SENSORS] = {
#if NUM_SENSORS == 5
    PIN_LINE_S1, PIN_LINE_S2, PIN_LINE_S3, PIN_LINE_S4, PIN_LINE_S5
#else
    PIN_LINE_S1, PIN_LINE_S2, PIN_LINE_S3, PIN_LINE_S4,
    PIN_LINE_S5, PIN_LINE_S6, PIN_LINE_S7, PIN_LINE_S8
#endif
};

// Trong so tuong ung cho tung cam bien tu trai qua phai
static const float sensorWeights[NUM_SENSORS] = {
#if NUM_SENSORS == 5
    -2.0f, -1.0f, 0.0f, 1.0f, 2.0f
#else
    -3.5f, -2.5f, -1.5f, -0.5f, 0.5f, 1.5f, 2.5f, 3.5f
#endif
};

static float lastKnownPos = 0.0f;
static bool lineCurrentlyLost = false;

// Muc logic hien tai (mac dinh lay tu config.h, co the dao nhanh bang lenh 'I')
uint8_t lineActiveState = LINE_ACTIVE_STATE;

// Tro keo (mac dinh TAT / dung INPUT thuan de khong lam lech nguong dien ap cua mach phan ap tren thanh cam bien)
bool sensorPullupEnabled = false;

void setSensorPullup(bool enable) {
    sensorPullupEnabled = enable;
    for (int i = 0; i < NUM_SENSORS; i++) {
        pinMode(sensorPins[i], enable ? INPUT_PULLUP : INPUT);
    }
    Serial.printf("[Sensor] Tro keo INPUT_PULLUP: %s\n", enable ? "BAT (INPUT_PULLUP)" : "TAT (INPUT)");
}

void toggleSensorPullup() {
    setSensorPullup(!sensorPullupEnabled);
}

void toggleLineActiveState() {
    lineActiveState = (lineActiveState == LOW) ? HIGH : LOW;
    Serial.printf("[Sensor] DA DAO MUC LOGIC SANG: %s (Vach den = muc %d)\n",
                  (lineActiveState == HIGH) ? "HIGH (1)" : "LOW (0)", lineActiveState);
}

uint8_t readDirectGpioPins() {
    uint8_t direct = 0;
    for (int i = 0; i < NUM_SENSORS; i++) {
        if (digitalRead(sensorPins[i]) == HIGH) {
            direct |= (1 << i);
        }
    }
    return direct;
}

static bool irEmitterState = (PIN_LINE_IR_EN >= 0);

void setSensorIrEmitter(bool enable) {
#if defined(PIN_LINE_IR_EN) && (PIN_LINE_IR_EN >= 0)
    irEmitterState = enable;
    pinMode(PIN_LINE_IR_EN, OUTPUT);
    digitalWrite(PIN_LINE_IR_EN, enable ? HIGH : LOW);
    Serial.printf("[Sensor] Chan IR Enable (GPIO %d) -> %s\n", PIN_LINE_IR_EN, enable ? "HIGH (BAT 8 LED PHAT)" : "LOW (TAT 8 LED PHAT)");
#else
    (void)enable;
    irEmitterState = false;
#endif
}

bool isSensorIrEmitterEnabled() {
    return irEmitterState;
}

void lineSensorInit() {
    setSensorIrEmitter(true);
    setSensorPullup(sensorPullupEnabled);
#if defined(PIN_LINE_IR_EN) && (PIN_LINE_IR_EN >= 0)
    Serial.printf("[Sensor] Khoi tao %d mat do line! IR Enable: GPIO %d (HIGH) | Tro keo: %s | Muc bat vach den: %s\n",
                  NUM_SENSORS,
                  PIN_LINE_IR_EN,
                  sensorPullupEnabled ? "INPUT_PULLUP" : "INPUT",
                  (lineActiveState == HIGH) ? "HIGH (1)" : "LOW (0)");
#else
    Serial.printf("[Sensor] Khoi tao %d mat cam bien! IR Enable: KHONG DUNG | Tro keo: %s | Muc bat vach den: %s\n",
                  NUM_SENSORS,
                  sensorPullupEnabled ? "INPUT_PULLUP" : "INPUT",
                  (lineActiveState == HIGH) ? "HIGH (1)" : "LOW (0)");
#endif
}

static uint8_t decodeSensorPins(uint8_t pins) {
    return ((lineActiveState == HIGH) ? pins : static_cast<uint8_t>(~pins)) & SENSOR_MASK;
}

uint8_t readRawSensors() {
    return decodeSensorPins(readDirectGpioPins());
}

float getLinePosition() {
    uint8_t raw = readRawSensors();
    float weightedSum = 0.0f;
    int activeCount = 0;

    for (int i = 0; i < NUM_SENSORS; i++) {
        if (raw & (1 << i)) {
            weightedSum += sensorWeights[i];
            activeCount++;
        }
    }

    if (activeCount == 0) {
        lineCurrentlyLost = true;
        const float maxWeight = (NUM_SENSORS - 1) * 0.5f;
        return (lastKnownPos > 0) ? maxWeight : -maxWeight;
    }

    lineCurrentlyLost = false;
    float currentPos = weightedSum / (float)activeCount;
    lastKnownPos = currentPos;
    return currentPos;
}

uint8_t edgeFollowMode = EDGE_FOLLOW_MODE;

void toggleEdgeFollowMode() {
    if (edgeFollowMode == EDGE_WHITE_LEFT_BLACK_RIGHT) {
        edgeFollowMode = EDGE_BLACK_LEFT_WHITE_RIGHT;
    } else if (edgeFollowMode == EDGE_BLACK_LEFT_WHITE_RIGHT) {
        edgeFollowMode = EDGE_AUTO;
    } else if (edgeFollowMode == EDGE_AUTO) {
        edgeFollowMode = EDGE_CENTER_LINE;
    } else {
        edgeFollowMode = EDGE_WHITE_LEFT_BLACK_RIGHT;
    }
    Serial.printf("[Sensor] Doi che do do line sang: %s\n", getEdgeFollowModeName());
}

const char* getEdgeFollowModeName() {
    switch (edgeFollowMode) {
        case EDGE_WHITE_LEFT_BLACK_RIGHT: return "TRANG TRAI - DEN PHAI";
        case EDGE_BLACK_LEFT_WHITE_RIGHT: return "DEN TRAI - TRANG PHAI";
        case EDGE_AUTO:                   return "TU DONG (AUTO)";
        case EDGE_CENTER_LINE:            return "BAM GIUA VACH (CENTER)";
        default:                          return "UNKNOWN";
    }
}

bool findLineEdge(uint8_t raw, uint8_t mode, float reference, LineEdge& edge) {
    if (mode == EDGE_CENTER_LINE) {
        if (raw == 0x00 || raw == SENSOR_MASK) return false;
        float weightedSum = 0.0f;
        int activeCount = 0;
        for (int i = 0; i < NUM_SENSORS; i++) {
            if (raw & (1 << i)) {
                weightedSum += sensorWeights[i];
                activeCount++;
            }
        }
        if (activeCount == 0) return false;
        edge = {weightedSum / static_cast<float>(activeCount), EDGE_CENTER_LINE};
        return true;
    }

    float minDiff = NUM_SENSORS;
    bool found = false;
    const float centerPairOffset = (NUM_SENSORS - 2) * 0.5f;

    for (int i = 0; i < NUM_SENSORS - 1; i++) {
        const bool blackLeft = raw & (1 << i);
        const bool blackRight = raw & (1 << (i + 1));
        if (blackLeft == blackRight) continue;

        const EdgeMode polarity = blackRight ? EDGE_WHITE_LEFT_BLACK_RIGHT : EDGE_BLACK_LEFT_WHITE_RIGHT;
        if (mode != EDGE_AUTO && mode != polarity) continue;

        const float position = i - centerPairOffset;
        const float distance = fabsf(position - reference);
        if (distance < minDiff) {
            minDiff = distance;
            edge = {position, polarity};
            found = true;
        }
    }
    return found;
}

bool isAllWhite() {
    return (readRawSensors() == 0x00);
}

bool isAllBlack() {
    return (readRawSensors() == SENSOR_MASK);
}

bool isLineLost() {
    return lineCurrentlyLost;
}

static bool hasManyBlackSensors(uint8_t raw) {
    int count = 0;
    for (int i = 0; i < NUM_SENSORS; i++) {
        if (raw & (1 << i)) count++;
    }
    return (count >= SENSORS_CROSS_LINE_THRESHOLD);
}

bool isCrossLine() {
    return hasManyBlackSensors(readRawSensors());
}

float getLastKnownPosition() {
    return lastKnownPos;
}

static String getSensorStatus(uint8_t raw) {
    if (raw == 0x00) return "ALL_WHITE";
    if (raw == SENSOR_MASK) return "ALL_BLACK";

    LineEdge edge;
    if (!findLineEdge(raw, edgeFollowMode, 0.0f, edge)) {
        return hasManyBlackSensors(raw) ? "CROSS" : "NO_EDGE";
    }

    char edgeBuf[10];
    snprintf(edgeBuf, sizeof(edgeBuf), "E:%+.1f", edge.position);
    return String(edgeBuf);
}

String getSensorTelemetryCompact() {
    uint8_t directPins = readDirectGpioPins();
    uint8_t raw = decodeSensorPins(directPins);

    char directBuf[NUM_SENSORS + 1];
    char lineBuf[NUM_SENSORS + 1];
    for (int i = 0; i < NUM_SENSORS; i++) {
        directBuf[i] = (directPins & (1 << i)) ? '1' : '0';
        lineBuf[i]   = (raw & (1 << i)) ? '#' : '.';
    }
    directBuf[NUM_SENSORS] = '\0';
    lineBuf[NUM_SENSORS]   = '\0';

    return "R:" + String(directBuf) + " [" + String(lineBuf) + "] " + getSensorStatus(raw) + "\n";
}

String getSensorTelemetryDetailed() {
    uint8_t directPins = readDirectGpioPins();
    uint8_t raw = decodeSensorPins(directPins);

    String s = "\n=== " + String(NUM_SENSORS) + "-SENSOR DIAGNOSTIC ===\n";
    s += "Pins:";
    for (int i = 0; i < NUM_SENSORS; i++) {
        s += " S" + String(i + 1) + "(" + String(sensorPins[i]) + ")";
    }
    s += "\n";
    s += "GPIO:  ";
    for (int i = 0; i < NUM_SENSORS; i++) {
        s += String((directPins & (1 << i)) ? "1" : "0") + "     ";
    }
    s += "\nState: ";
    for (int i = 0; i < NUM_SENSORS; i++) {
        s += String((raw & (1 << i)) ? "#" : ".") + "     ";
    }
    s += "\n(# = Vach den, . = Nen trang)\n";
    s += "Logic : Vach den = " + String((lineActiveState == HIGH) ? "HIGH (1)" : "LOW (0)") + " [Bam 'I' de dao]\n";
#if defined(PIN_LINE_IR_EN) && (PIN_LINE_IR_EN >= 0)
    s += "IR LED: " + String(isSensorIrEmitterEnabled() ? "BAT (GPIO " : "TAT (GPIO ") + String(PIN_LINE_IR_EN) + (isSensorIrEmitterEnabled() ? " = HIGH)" : " = LOW)") + " [Bam 'E' de bat/tat]\n";
#else
    s += "IR LED: KHONG DUNG\n";
#endif
    s += "Pullup: " + String(sensorPullupEnabled ? "BAT (INPUT_PULLUP)" : "TAT (INPUT)") + " [Bam 'P' de dao]\n";
    s += "Vien  : " + String(getEdgeFollowModeName()) + " [Bam 'V' de doi]\n";
    s += "Status: " + getSensorStatus(raw) + "\n";
    s += "Lenh  : 1=Start/Stop, V=Doi vien, +/-=Speed, I=Dao logic, E=IR, ?=Help\n";
    s += "===========================\n";
    return s;
}

void printSensorDebug() {
    uint8_t directPins = readDirectGpioPins();
    uint8_t raw = decodeSensorPins(directPins);

    char pinBuf[12];
    char lineBuf[12];
    for (int i = 0; i < NUM_SENSORS; i++) {
        pinBuf[i]  = (directPins & (1 << i)) ? '1' : '0';
        lineBuf[i] = (raw & (1 << i)) ? '#' : '.';
    }
    pinBuf[NUM_SENSORS]  = '\0';
    lineBuf[NUM_SENSORS] = '\0';

    String status = getSensorStatus(raw);
    Serial.printf("[Sensor] GPIO: %s | Line: [%s] | Active=%s | Pullup=%s | Status: %s\n",
                  pinBuf, lineBuf, (lineActiveState == HIGH) ? "HIGH" : "LOW",
                  sensorPullupEnabled ? "PULLUP" : "FLOAT",
                  status.c_str());
}
