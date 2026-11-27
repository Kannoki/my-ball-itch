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

#ifndef SENSOR_PHOTORESISTOR
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
#endif

void toggleLineActiveState() {
    lineActiveState = (lineActiveState == LOW) ? HIGH : LOW;
#ifdef SENSOR_PHOTORESISTOR
    Serial.printf("[Sensor] Vach den: ADC %s nguong\n", (lineActiveState == HIGH) ? ">=" : "<");
#else
    Serial.printf("[Sensor] DA DAO MUC LOGIC SANG: %s (Vach den = muc %d)\n",
                  (lineActiveState == HIGH) ? "HIGH (1)" : "LOW (0)", lineActiveState);
#endif
}

static uint8_t readSensorLevels(uint16_t* adcValues = nullptr) {
#ifndef SENSOR_PHOTORESISTOR
    (void)adcValues;
#endif
    uint8_t levels = 0;
    for (int i = 0; i < NUM_SENSORS; i++) {
#ifdef SENSOR_PHOTORESISTOR
        const uint16_t value = analogRead(sensorPins[i]);
        if (adcValues) adcValues[i] = value;
        const bool high = value >= PHOTORES_ADC_THRESHOLDS[i];
#else
        const bool high = digitalRead(sensorPins[i]) == HIGH;
#endif
        if (high) {
            levels |= (1U << i);
        }
    }
    return levels;
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
#ifdef SENSOR_PHOTORESISTOR
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    // Khoi tao cac kenh ADC truoc khi BLE co the yeu cau doc cam bien.
    readSensorLevels();
    Serial.printf("[Sensor] Khoi tao %d mat ADC 12-bit | Vach den: ADC %s nguong\n",
                  NUM_SENSORS, (lineActiveState == HIGH) ? ">=" : "<");
#else
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
#endif
}

static uint8_t decodeSensorLevels(uint8_t levels) {
    return ((lineActiveState == HIGH) ? levels : static_cast<uint8_t>(~levels)) & SENSOR_MASK;
}

uint8_t readRawSensors() {
    return decodeSensorLevels(readSensorLevels());
}

#ifdef SENSOR_PHOTORESISTOR
uint8_t readPhotoresSensors(uint16_t (&adcValues)[NUM_SENSORS]) {
    return decodeSensorLevels(readSensorLevels(adcValues));
}
#endif

LineAction getLineAction(uint8_t raw) {
    if ((raw & SENSOR_MASK) == SENSOR_MASK) return LINE_CROSS;

    const bool left = (raw & LINE_LEFT_MASK) != 0;
    const bool center = (raw & LINE_CENTER_MASK) != 0;
    const bool right = (raw & LINE_RIGHT_MASK) != 0;

    // S3 cua ban 5 mat hoac ca hai ben cho phep tiep tuc bam vach.
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
    const uint8_t levels = readSensorLevels();
    const uint8_t raw = decodeSensorLevels(levels);

    char directBuf[NUM_SENSORS + 1];
    char lineBuf[NUM_SENSORS + 1];
    for (int i = 0; i < NUM_SENSORS; i++) {
        directBuf[i] = (levels & (1 << i)) ? '1' : '0';
        lineBuf[i]   = (raw & (1 << i)) ? '#' : '.';
    }
    directBuf[NUM_SENSORS] = '\0';
    lineBuf[NUM_SENSORS]   = '\0';

    return "R:" + String(directBuf) + " [" + String(lineBuf) + "] U:" + getTrackingPattern(raw) + " " + getSensorStatus(raw) + "\n";
}

String getSensorTelemetryDetailed() {
#ifdef SENSOR_PHOTORESISTOR
    uint16_t adcValues[NUM_SENSORS];
    const uint8_t raw = readPhotoresSensors(adcValues);
#else
    const uint8_t levels = readSensorLevels();
    const uint8_t raw = decodeSensorLevels(levels);
#endif

    String s = "\n=== " + String(NUM_SENSORS) + "-SENSOR DIAGNOSTIC ===\n";
    s += "Pins:";
    for (int i = 0; i < NUM_SENSORS; i++) {
        s += " S" + String(i + 1) + "(" + String(sensorPins[i]) + ")";
    }
#ifdef SENSOR_PHOTORESISTOR
    s += "\nADC/nguong:";
    for (int i = 0; i < NUM_SENSORS; i++) {
        s += " " + String(adcValues[i]) + "/" + String(PHOTORES_ADC_THRESHOLDS[i]);
    }
#else
    s += "\nGPIO:  ";
    for (int i = 0; i < NUM_SENSORS; i++) {
        s += String((levels & (1 << i)) ? "1" : "0") + "     ";
    }
#endif
    s += "\nState: ";
    for (int i = 0; i < NUM_SENSORS; i++) {
        s += String((raw & (1 << i)) ? "#" : ".") + "     ";
    }
    s += "\nUse: " + getTrackingPattern(raw) + "\n";
    s += "(#=active, .=inactive, -=skip)\n";
#ifdef SENSOR_PHOTORESISTOR
    s += "Vach den: ADC " + String((lineActiveState == HIGH) ? ">=" : "<") + " nguong ['I'=dao]\n";
#else
    s += "Logic : Active = " + String((lineActiveState == HIGH) ? "HIGH (1)" : "LOW (0)") + " ['I'=dao]\n";
#if defined(PIN_LINE_IR_EN) && (PIN_LINE_IR_EN >= 0)
    s += "IR LED: " + String(isSensorIrEmitterEnabled() ? "BAT (GPIO " : "TAT (GPIO ") + String(PIN_LINE_IR_EN) + (isSensorIrEmitterEnabled() ? " = HIGH)" : " = LOW)") + " [Bam 'E' de bat/tat]\n";
#else
    s += "IR LED: KHONG DUNG\n";
#endif
    s += "Pullup: " + String(sensorPullupEnabled ? "BAT (INPUT_PULLUP)" : "TAT (INPUT)") + " [Bam 'P' de dao]\n";
#endif
#if NUM_SENSORS == 8
    s += "Mode: PD THEO TAM VACH (DUONG CHINH)\n";
#else
    s += "Mode: DO LINE CO BAN (if/else)\n";
#endif
    s += "Status: " + getSensorStatus(raw) + "\n";
#ifdef SENSOR_PHOTORESISTOR
    s += "Cmd: 1,V,+/-,I,T,?\n";
#else
    s += "Cmd: 1,V,+/-,I,E,P,T,?\n";
#endif
    s += "===========================\n";
    return s;
}

void printSensorDebug() {
#ifdef SENSOR_PHOTORESISTOR
    uint16_t adcValues[NUM_SENSORS];
    const uint8_t blackMask = readPhotoresSensors(adcValues);
    char line[NUM_SENSORS + 1];
    for (int i = 0; i < NUM_SENSORS; ++i) {
        line[i] = (blackMask & (1U << i)) ? '#' : '.';
    }
    line[NUM_SENSORS] = '\0';
    Serial.printf("[%lu ms] LINE=%s ADC S1=%d S2=%d S3=%d S4=%d S5=%d S6=%d S7=%d S8=%d\r\n",
                  millis(), line, adcValues[0], adcValues[1], adcValues[2], adcValues[3],
                  adcValues[4], adcValues[5], adcValues[6], adcValues[7]);
#else
    const uint8_t levels = readSensorLevels();
    const uint8_t raw = decodeSensorLevels(levels);

    char pinBuf[12];
    char lineBuf[12];
    for (int i = 0; i < NUM_SENSORS; i++) {
        pinBuf[i]  = (levels & (1 << i)) ? '1' : '0';
        lineBuf[i] = (raw & (1 << i)) ? '#' : '.';
    }
    pinBuf[NUM_SENSORS]  = '\0';
    lineBuf[NUM_SENSORS] = '\0';

    String status = getSensorStatus(raw);
    Serial.printf("[Sensor] GPIO: %s | Raw: [%s] | Use: [%s] | Active=%s | Pullup=%s | Status: %s\n",
                  pinBuf, lineBuf, getTrackingPattern(raw).c_str(), (lineActiveState == HIGH) ? "HIGH" : "LOW",
                  sensorPullupEnabled ? "PULLUP" : "FLOAT", status.c_str());
#endif
}
