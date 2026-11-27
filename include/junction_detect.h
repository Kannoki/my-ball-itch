#ifndef JUNCTION_DETECT_H
#define JUNCTION_DETECT_H

#include <stdint.h>

// Loai giao lo nhanh, phan loai tu mau 8 mat cam bien.
enum JunctionKind {
    JUNCTION_NONE = 0,
    JUNCTION_LEFT,       // Duong chinh DA MAT, chi con nhanh ben TRAI -> quay trai 90
    JUNCTION_RIGHT,      // Duong chinh DA MAT, chi con nhanh ben PHAI -> quay phai 90
    JUNCTION_TEE_LEFT,   // Nga 3 (chu T): duong chinh CON duoi xe, nhanh ben TRAI
    JUNCTION_TEE_RIGHT,  // Nga 3 (chu T): duong chinh CON duoi xe, nhanh ben PHAI
    JUNCTION_CROSS       // Vach ngang cat qua toan bo thanh cam bien (+ hoac tram servo)
};

// So cam bien bi den trong mau (bit 0 = S1).
int countBlackSensors(uint8_t raw);

// Phan loai giao lo theo HINH HOC, khong so sanh mask chinh xac.
//   - Nga 3 (T): duong chinh con nam duoi xe (S4 + S5 den) va co cum ben
//     trai hoac ben phai: "#####..." (0x1F) hoac "...#####" (0xF8).
//   - Nga re 90 do: duong chinh da mat (S5 hoac S4 trang) va cum 3 mat ben
//     trai/phai: "###....." (0x07), "####...." (0x0F), ".....###" (0xE0),
//     "....####" (0xF0).
//   - Vach ngang: it phu 4 mat giua S3..S6 hoac ca 8 mat den: giao lo (+) va
//     tram servo dung chung mot dau hieu, phan biet bang thu tu chay.
JunctionKind detectJunction(uint8_t raw);

// True neu la giao lo ngang (vach ngang 5-7 cam bien den),
// duoc dung de phat hien tram servo ma khong can ca 8 cam bien den.
bool isCrossPattern(uint8_t raw);

// True neu giao lo yeu cau quay 90 do sang ben TRAI.
bool isLeftTurnJunction(JunctionKind kind);

// True neu giao lo yeu cau quay 90 do sang ben PHAI.
bool isRightTurnJunction(JunctionKind kind);

const char* getJunctionName(JunctionKind kind);

#endif // JUNCTION_DETECT_H
