#pragma once

// ============================================================
//  Config.h — ค่าคงที่ทั้งหมดของหุ่นยนต์ซูโม่
//  แก้ค่าที่นี่จุดเดียวเพื่อปรับพฤติกรรมหุ่น
//
//  [แก้ไข] เปลี่ยน ToF (VL53L0X / I2C) → Sharp GP2Y0A41SK0F (Analog)
//          ระยะวัดได้จริง ~40–300 mm (เซนเซอร์ตัวนี้)
// ============================================================


// ── ขา Motor ────────────────────────────────────────────────
#define PIN_L1  11   // มอเตอร์ซ้าย  ไปข้างหน้า
#define PIN_L2  10   // มอเตอร์ซ้าย  ถอยหลัง
#define PIN_R1   5   // มอเตอร์ขวา  ไปข้างหน้า
#define PIN_R2   6   // มอเตอร์ขวา  ถอยหลัง


// ── ขา Sensor เส้น (Line Sensor) ────────────────────────────
#define PIN_LINE_L  A2   // sensor เส้นซ้าย
#define PIN_LINE_R  A7   // sensor เส้นขวา

#define LINE_L_TH  920   // ค่า threshold ซ้าย  (analogRead > ค่านี้ = เจอเส้นขาว)
#define LINE_R_TH  500   // ค่า threshold ขวา  (analogRead > ค่านี้ = เจอเส้นขาว)


// ── ขา Analog ของ Sharp GP2Y0A41SK0F แต่ละตัว ──────────────
//   SL = Side Left, FL = Front Left, FC = Front Center
//   FR = Front Right, SR = Side Right
//
//   ต่อสาย Vout ของแต่ละเซนเซอร์เข้า pin ด้านล่างนี้
//   (เลือก A0–A5 ตามที่เหลือบอร์ด หลีกเลี่ยง A2 / A7 ที่ใช้เส้นแล้ว)
#define PIN_SHARP_SL  A0
#define PIN_SHARP_FL  A1
#define PIN_SHARP_FC  A3
#define PIN_SHARP_FR  A4
#define PIN_SHARP_SR  A5


// ── ค่าคงที่ Sharp GP2Y ──────────────────────────────────────
//   สูตรแปลง:  mm = K / (Vout_raw - OFFSET)
//   ค่าได้จาก datasheet + fitting บนสนาม จริง ปรับ K / OFFSET ได้
#define SHARP_K       12000.0f   // ค่าคงที่ fitting (หน่วย: raw × mm)
#define SHARP_OFFSET  0          // offset ADC ถ้าต้องการชดเชย (ปกติ = 0)

//   ระยะสูงสุดที่เชื่อถือได้ของ GP2Y0A41SK0F ≈ 300 mm
//   ระยะต่ำสุดที่เชื่อถือได้ ≈ 40 mm (ใกล้กว่านี้ค่าบิด)
#define SHARP_MAX_MM  300
#define SHARP_MIN_MM   40

//   จำนวนครั้ง oversample ต่อการอ่าน 1 ครั้ง (เฉลี่ยลด noise)
#define SHARP_SAMPLES   3


// ── ระยะที่ใช้ตัดสินใจ (หน่วย mm) ──────────────────────────
#define RAM_DIST    180   // ระยะพุ่งชน   — ถ้าศัตรูใกล้กว่านี้ให้พุ่ง
#define TRACK_DIST  280   // ระยะติดตาม   — เห็นศัตรูแต่ยังไม่ถึงพุ่ง (ลดจาก 300 ให้อยู่ในช่วง sensor)
#define SIDE_DIST   200   // ระยะ sensor ข้าง ที่ถือว่าเห็นศัตรู


// ── ความเร็ว (0–255) ────────────────────────────────────────
#define MAX_SPEED     255

#define ATTACK_SPEED  255   // พุ่งชนเต็มสปีด
#define TRACK_FAST    220   // ติดตาม — ด้านที่หันหาศัตรู
#define TRACK_SLOW    130   // ติดตาม — ด้านตรงข้าม (เลี้ยว)

#define SEARCH_SPEED  100   // หมุนค้นหาศัตรู (ช้า)
#define SEARCH_FAST   120   // หมุนค้นหาหลังจาก lock ตัวได้ (เร็ว)

#define ESCAPE_BACK   200   // ถอยหลังหนีเส้น
#define ESCAPE_TURN   200   // หมุนกลับเข้าสนาม


// ── เวลา (หน่วย ms) ─────────────────────────────────────────
#define ESCAPE_BACK_MS   180   // ระยะเวลาถอยหลัง
#define ESCAPE_TURN_MS   220   // ระยะเวลาหมุนกลับ

#define LOCK_TIME        500   // ล็อก target ไว้นานแค่ไหน ก่อนสูญเสีย
#define SEARCH_SWAP_MS   600   // สลับทิศค้นหาทุกกี่ ms
#define SEARCH_FWD_MS    200   // เดินหน้าระหว่าง search ก่อนสลับทิศ (spiral)

// ── TRACK proportional ──────────────────────────────────────
#define TRACK_CENTER_ZONE  40  // ถ้า |fl-fr| < ค่านี้ ถือว่าศัตรูอยู่ตรงหน้า → วิ่งตรง


// ── ค่า "ไม่มีเป้า" ─────────────────────────────────────────
#define NO_TARGET  999   // ค่าที่แทน "ไม่มีเป้าหมาย" (เหมือนเดิม)
