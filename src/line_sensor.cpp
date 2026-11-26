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

LineAction getLineAction(uint8_t raw) {
    if ((raw & SENSOR_MASK) == SENSOR_MASK) return LINE_CROSS;

#if NUM_SENSORS == 8
    // On the return straight the station arm extends to the robot's left.
    // It may cover S1 through S5 without reaching the far right sensor.
    if ((raw & 0x1F) == 0x1F) return LINE_CROSS;
    if (raw == 0xF8 || raw == 0xFC) return LINE_RIGHT;
#endif
    const bool left = (raw & LINE_LEFT_MASK) != 0;
    const bool center = (raw & LINE_CENTER_MASK) != 0;
    const bool right = (raw & LINE_RIGHT_MASK) != 0;

    // Thay line o giua (ban 5 mat) hoac ca hai ben thi di thang.
    if (center || (left && right)) return LINE_FORWARD;
    if (left) return LINE_LEFT;
    if (right) return LINE_RIGHT;
    return LINE_STOP;
}

const char* getLineActionName(LineAction action) {
    switch (action) {
        case LINE_FORWARD: return "FORWARD";
        case LINE_LEFT:    return "LEFT";
        case LINE_RIGHT:   return "RIGHT";
        case LINE_CROSS:   return "CROSS";
        default:           return "STOP";
    }
}

static String getSensorStatus(uint8_t raw) {
    return String(getLineActionName(getLineAction(raw)));
}

static String getTrackingPattern(uint8_t raw) {
    char pattern[NUM_SENSORS + 1];
    for (int i = 0; i < NUM_SENSORS; ++i) {
        pattern[i] = !(LINE_TRACKING_MASK & (1 << i)) ? '-' : (raw & (1 << i)) ? '#' : '.';
    }
    pattern[NUM_SENSORS] = '\0';
    return String(pattern);
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

    return "R:" + String(directBuf) + " [" + String(lineBuf) + "] U:" + getTrackingPattern(raw) + " " + getSensorStatus(raw) + "\n";
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
    s += "\nUse: " + getTrackingPattern(raw) + "\n";
    s += "(#=active, .=inactive, -=skip)\n";
    s += "Logic : Active = " + String((lineActiveState == HIGH) ? "HIGH (1)" : "LOW (0)") + " ['I'=dao]\n";
#if defined(PIN_LINE_IR_EN) && (PIN_LINE_IR_EN >= 0)
    s += "IR LED: " + String(isSensorIrEmitterEnabled() ? "BAT (GPIO " : "TAT (GPIO ") + String(PIN_LINE_IR_EN) + (isSensorIrEmitterEnabled() ? " = HIGH)" : " = LOW)") + " [Bam 'E' de bat/tat]\n";
#else
    s += "IR LED: KHONG DUNG\n";
#endif
    s += "Pullup: " + String(sensorPullupEnabled ? "BAT (INPUT_PULLUP)" : "TAT (INPUT)") + " [Bam 'P' de dao]\n";
    s += "Mode: DO LINE CO BAN (if/else)\n";
    s += "Status: " + getSensorStatus(raw) + "\n";
    s += "Cmd: 1,V,+/-,I,E,P,T,?\n";
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
    Serial.printf("[Sensor] GPIO: %s | Raw: [%s] | Use: [%s] | Active=%s | Pullup=%s | Status: %s\n",
                  pinBuf, lineBuf, getTrackingPattern(raw).c_str(), (lineActiveState == HIGH) ? "HIGH" : "LOW",
                  sensorPullupEnabled ? "PULLUP" : "FLOAT",
                  status.c_str());
}
