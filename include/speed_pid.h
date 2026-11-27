#pragma once

#include <stdint.h>

// ------------------------------------------------------------------------------
// VONG TRONG: SPEED PI (PID) TUNG BANH
// ------------------------------------------------------------------------------
// Vong ngoai (Position PD tren 8 cam bien) ra TOI DA van toc do chuan hoa cho tung
// banh. Vong trong nay do bang encoder va sua duty de moi banh chay dung toc do
// yeu cau, sau do moi xuat PWM ra ENA/ENB.
//
// Luu y ve do phan giai: 1 xung = PI * 66 / 20 = 10.367 mm. O toc do hanh thong
// ~350 mm/s chi co ~34 xung/s, tuc ~0.17 xung moi mau 5 ms. Do do PI phai chay
// tren CUA SO THOI GIAN (SPEED_PID_WINDOW_MS), khong phai tren tung mau cam bien.
//
// PI chay theo chu ky rieng (SPEED_PID_PERIOD_MS) bang millis(), khong dung
// delay() trong vong dieu khien.

struct SpeedPidState {
    float integral = 0.0f;
    float correction = 0.0f;
    float lastMeasuredMmPerSecond = 0.0f;
    bool  hasMeasurement = false;
    bool  stalled = false;
    unsigned long lastUpdateAt = 0;
    int   emptyWindows = 0;
};

// Van toc do tham chieu cho lenh goc (dung khi lenh khac 0).
// Ty le voi duty mo: duty 165/255 -> SPEED_PID_REFERENCE_MM_S.
float speedPidReferenceMmPerSecond(int commandedSpeed);

// Dat lai toan bo trang thai PI (khi bat dau / doi che do do line).
void speedPidReset(SpeedPidState& state);

// Tinh lech so giua van toc do yeu cau va van toc do do duoc, roi ap dung
// vao lenh van toc do chuan hoa cua tung banh.
//
//   targetMmPerSecond  : van toc do tham chieu (tu duty mo)
//   measuredMmPerSecond: van toc do do duoc tu encoder
//   commandedSpeed     : lenh van toc do chuan hoa hien tai (-1000..+1000)
//   now                : millis()
//
// Tra ve lenh van toc do chuan hoa da chinh, hoac -1 neu mat tin hieu encoder.
int speedPidCorrect(SpeedPidState& state,
                    float targetMmPerSecond,
                    float measuredMmPerSecond,
                    int commandedSpeed,
                    unsigned long now);
