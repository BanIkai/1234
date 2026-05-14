#pragma once

// ============================================================
//  Config.h — ค่าคงที่ทั้งหมดของหุ่นยนต์ซูโม่
//  แก้ค่าที่นี่จุดเดียวเพื่อปรับพฤติกรรมหุ่น
// ============================================================


// ── ขา Motor ────────────────────────────────────────────────
#define PIN_L1  11   // มอเตอร์ซ้าย  ไปข้างหน้า
#define PIN_L2  10   // มอเตอร์ซ้าย  ถอยหลัง
#define PIN_R1   5   // มอเตอร์ขวา  ไปข้างหน้า
#define PIN_R2   6   // มอเตอร์ขวา  ถอยหลัง


// ── ขา Sensor เส้น (Line Sensor) ────────────────────────────
#define PIN_LINE_L  A2   // sensor เส้นซ้าย
#define PIN_LINE_R  A7   // sensor เส้นขวา

#define LINE_L_TH  500   // ค่า threshold ซ้าย  (> = เจอเส้นขาว)
#define LINE_R_TH  500   // ค่า threshold ขวา  (> = เจอเส้นขาว)


// ── ขา XSHUT ของ ToF แต่ละตัว ───────────────────────────────
//   SL = Side Left, FL = Front Left, FC = Front Center
//   FR = Front Right, SR = Side Right
#define PIN_XSHUT_SL  13
#define PIN_XSHUT_FL  12
#define PIN_XSHUT_FC   4
#define PIN_XSHUT_FR   2
#define PIN_XSHUT_SR   3


// ── I2C Address ของ ToF แต่ละตัว ────────────────────────────
#define ADDR_SL  0x30
#define ADDR_FL  0x31
#define ADDR_FC  0x32
#define ADDR_FR  0x33
#define ADDR_SR  0x34


// ── ระยะที่ใช้ตัดสินใจ (หน่วย mm) ──────────────────────────
#define RAM_DIST    180   // ระยะพุ่งชน   — ถ้าศัตรูใกล้กว่านี้ให้พุ่ง
#define TRACK_DIST  350   // ระยะติดตาม   — เห็นศัตรูแต่ยังไม่ถึงพุ่ง
#define SIDE_DIST   250   // ระยะ sensor ข้าง ที่ถือว่าเห็นศัตรู


// ── ความเร็ว (0–255) ────────────────────────────────────────
#define MAX_SPEED     255

#define ATTACK_SPEED  255   // พุ่งชนเต็มสปีด
#define TRACK_FAST    220   // ติดตาม — ด้านที่หันหาศัตรู
#define TRACK_SLOW    130   // ติดตาม — ด้านตรงข้าม (เลี้ยว)

#define SEARCH_SPEED  170   // หมุนค้นหาศัตรู (ช้า)
#define SEARCH_FAST   220   // หมุนค้นหาหลังจาก lock ตัวได้ (เร็ว)

#define ESCAPE_BACK   255   // ถอยหลังหนีเส้น
#define ESCAPE_TURN   200   // หมุนกลับเข้าสนาม


// ── เวลา (หน่วย ms) ─────────────────────────────────────────
#define ESCAPE_BACK_MS   180   // ระยะเวลาถอยหลัง
#define ESCAPE_TURN_MS   220   // ระยะเวลาหมุนกลับ

#define LOCK_TIME        500   // ล็อก target ไว้นานแค่ไหน ก่อนสูญเสีย (เพิ่มจาก 350)
#define SEARCH_SWAP_MS   600   // สลับทิศค้นหาทุกกี่ ms
#define SEARCH_FWD_MS    200   // เดินหน้าระหว่าง search ก่อนสลับทิศ (spiral)

// ── TRACK proportional ──────────────────────────────────────
#define TRACK_CENTER_ZONE  40  // ถ้า |fl-fr| < ค่านี้ ถือว่าศัตรูอยู่ตรงหน้า → วิ่งตรง


// ── ToF ─────────────────────────────────────────────────────
#define TOF_TIMEOUT   20    // timeout ของ sensor ToF (ms)
#define NO_TARGET    999    // ค่าที่แทน "ไม่มีเป้าหมาย"
