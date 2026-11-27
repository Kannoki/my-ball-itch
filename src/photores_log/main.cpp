#include <Arduino.h>
#include "config.h"
#include "line_sensor.h"

static_assert(NUM_SENSORS == 8 && PIN_LINE_IR_EN < 0, "Requires the 8-channel photoresistor configuration");

static constexpr unsigned long LOG_INTERVAL_MS = 500;
static unsigned long lastLoggedAt = 0;

void setup() {
    // Giu hai chan Enable cua L298N o muc tat trong luc do cam bien.
    digitalWrite(PIN_MOTOR_LEFT_ENB, LOW);
    pinMode(PIN_MOTOR_LEFT_ENB, OUTPUT);
    digitalWrite(PIN_MOTOR_RIGHT_ENA, LOW);
    pinMode(PIN_MOTOR_RIGHT_ENA, OUTPUT);

    Serial.begin(115200);
    delay(1000);
    lineSensorInit();
    Serial.printf("PHOTORES ADC LOG | 12-bit | %lu ms | S1..S8\n", LOG_INTERVAL_MS);
    lastLoggedAt = millis();
    printSensorDebug();
}

void loop() {
    const unsigned long now = millis();
    if (now - lastLoggedAt < LOG_INTERVAL_MS) {
        delay(1);
        return;
    }
    lastLoggedAt = now;
    printSensorDebug();
}
