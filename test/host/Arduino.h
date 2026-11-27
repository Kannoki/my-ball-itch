#pragma once

#include <stdint.h>

constexpr uint8_t LOW = 0;
constexpr uint8_t HIGH = 1;
constexpr uint8_t INPUT = 0;
constexpr uint8_t INPUT_PULLUP = 2;
constexpr uint8_t OUTPUT = 3;

class String {
public:
    String(const char*) {}
    String(int) {}
    const char* c_str() const { return ""; }
    String& operator+=(const String&) { return *this; }
};

inline String operator+(String left, const String& right) { return left += right; }

struct HostSerial {
    void printf(const char*, ...) {}
    void println(const char*) {}
};

extern HostSerial Serial;
unsigned long millis();
int digitalRead(uint8_t pin);
uint16_t analogRead(uint8_t pin);
constexpr uint8_t ADC_11db = 3;
inline void analogReadResolution(uint8_t) {}
inline void analogSetAttenuation(uint8_t) {}
inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t, uint8_t) {}
