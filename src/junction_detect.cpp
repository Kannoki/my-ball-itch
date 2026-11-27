#include "junction_detect.h"

#include "config.h"

int countBlackSensors(uint8_t raw) {
    uint8_t mask = static_cast<uint8_t>(raw & SENSOR_MASK);
    int count = 0;
    while (mask != 0) {
        count += mask & 1;
        mask >>= 1;
    }
    return count;
}

// -----------------------------------------------------------------------------
// PHAN LOAI GIAO LO THEO HINH HOC
// -----------------------------------------------------------------------------
// MANG CAM BIEN: bit 0 = S1 (ngoai cung trai) ... bit 7 = S8 (ngoai cung phai).
//
// HINH HOC CUA CAC GIAO LO (bar cam bien song song voi duong chinh):
//
//   NGA 3 (chu T) - nhanh ben TRAI, duong chinh VAN con chay qua:
//        duong chinh ......... S4 S5 den (giua thanh)
//        nhanh trai     ... S1 S2 S3 den, S6 S7 S8 TRANG
//        -> mau 0x19 (S1,S4,S5), 0x1B (S1,S2,S4,S5), 0x1F (S1..S5)
//        Dai nay xuat hien TRONG khi xe duyet qua nhanh: dai vach ngang cua
//        nhanh quet tren thanh tu trai sang phai (S1 -> S1S2 -> S1..S5).
//        Ben PHAI luon TRANG o nga 3 vi chi co MOT nhanh.
//
//   NGA RE 90 DO - duong chinh DA MAT (S5 hoac S4 trang):
//        "###....." (0x07), "####...." (0x0F)  -> JUNCTION_LEFT
//        ".....###" (0xE0), "....####" (0xF0)  -> JUNCTION_RIGHT
//
//   GIAO LO (+) / TRAM SERVO - dai vach ngang cat qua TOAN BO thanh cam bien:
//        "..####.." (0x3C), ".######." (0x7E), "########" (0xFF)...
//        GIAO LO (+) va TRAM SERVO DUNG CHUNG MOT DAU HIEU -> khong the phan
//        biet bang hinh hoc, chi phan biet duoc bang THU TU CHAY.
//
// THU TU kiem tra quan trong: phai kiem tra NGA 3 TRUOC VACH NGANG. Khi xe
// cham vao giao lo (+), dai vach ngang cung quet qua thanh tu trai sang
// phai va DI QUA mau 0x1F - neu nhan 0x1F la nga 3 thi xe se quay sai
// huong. Vi vay 0x1F chi duoc coi la nga 3 khi CHUONG TRINH DUONG CHAY
// dang cho nga 3 (xem line_tracking.cpp).
JunctionKind detectJunction(uint8_t raw) {
    const uint8_t mask = static_cast<uint8_t>(raw & SENSOR_MASK);
    if (mask == 0) return JUNCTION_NONE;

    const bool s1 = (mask & 0x01) != 0;
    const bool s4 = (mask & 0x08) != 0;
    const bool s5 = (mask & 0x10) != 0;
    const bool s8 = (mask & 0x80) != 0;

    // Cum 3 cam bien lien tiep o bien trai (S1, S2, S3) hoac ben phai (S6, S7, S8).
    const bool leftCluster = (mask & 0x07) == 0x07;
    const bool rightCluster = (mask & 0xE0) == 0xE0;

    // 1. NGA 3 (chu T): duong chinh con nam duoi xe (S4 + S5 den) va co nhanh
    //    o MOT BEN, ben doi phai SACH HOAN TOAN (S6/S7/S8 hoac S1/S2/S3 trang).
    //    Chi can mat bien ngoai cung ben nhanh (S1 hoac S8) la den: dai vach
    //    ngang cua nhanh quet tren thanh qua nhieu mau lien tiep nen can
    //    nhieu mau de xac nhan, khong duoc chang mot mau.
    if (s4 && s5) {
        if (s1 && (mask & 0xE0) == 0) return JUNCTION_TEE_LEFT;
        if (s8 && (mask & 0x07) == 0) return JUNCTION_TEE_RIGHT;
    }

    // 2. VACH NGANG: chap nhan dai rong co mat doc trang xen giua.
    if ((mask & 0x3C) == 0x3C || countBlackSensors(mask) >= 5) return JUNCTION_CROSS;

    // 3. NGA RE 90 DO: co cum bien, cho phep con mot mat giua.
    if (leftCluster && (mask & 0xE0) == 0) return JUNCTION_LEFT;
    if (rightCluster && (mask & 0x07) == 0) return JUNCTION_RIGHT;

    return JUNCTION_NONE;
}

bool isCrossPattern(uint8_t raw) {
    const uint8_t mask = static_cast<uint8_t>(raw & SENSOR_MASK);
    if (mask == SENSOR_MASK) return true;
    // Vach NGANG (tram servo) cat ngang qua giua thanh cam bien: it phu
    // S3..S6 (4 cam bien giua) thay vi chi 2 cam bien nhu duong chinh (S4+S5).
    // Do la mau 5-7 cam bien den, nen truoc day bi getLineAction() phan loai
    // sai thanh FORWARD va chuoi quay tai cho o tram servo khong bao gio
    // duoc kich hoat.
    if ((mask & 0x3C) != 0x3C) return false;
    return countBlackSensors(mask) >= 4;
}

bool isLeftTurnJunction(JunctionKind kind) {
    return kind == JUNCTION_LEFT || kind == JUNCTION_TEE_LEFT;
}

bool isRightTurnJunction(JunctionKind kind) {
    return kind == JUNCTION_RIGHT || kind == JUNCTION_TEE_RIGHT;
}

const char* getJunctionName(JunctionKind kind) {
    switch (kind) {
        case JUNCTION_LEFT:      return "LEFT";
        case JUNCTION_RIGHT:     return "RIGHT";
        case JUNCTION_TEE_LEFT:  return "TEE-LEFT";
        case JUNCTION_TEE_RIGHT: return "TEE-RIGHT";
        case JUNCTION_CROSS:     return "CROSS";
        default:                 return "NONE";
    }
}
