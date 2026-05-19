#pragma once

// ============================================================
//  Config.h — ค่าคงที่ทั้งหมดของหุ่นยนต์ซูโม่
//
//  วิธีแก้ไข: เปลี่ยนค่าในไฟล์นี้เพียงที่เดียว
//  แบ่งเป็นหมวดหมู่ เพื่อหาและแก้ไขได้ง่ายขึ้น
//
//  หมวด:
//    1. PIN มอเตอร์
//    2. PIN และ Threshold ของ Sensor เส้น
//    3. PIN ของ Sharp GP2Y0A41SK0F
//    4. ค่าแปลงสัญญาณ Sharp GP2Y
//    5. ระยะตัดสินใจ (หน่วย mm)
//    6. ความเร็วมอเตอร์ (0–255)
//    7. เวลา (หน่วย ms)
//    8. ค่าพิเศษ
// ============================================================


// ── 1. PIN มอเตอร์ ──────────────────────────────────────────
//   PIN_x1 = ไปข้างหน้า (PWM)
//   PIN_x2 = ถอยหลัง   (PWM)
#define PIN_L1  11   // มอเตอร์ซ้าย  — ไปข้างหน้า
#define PIN_L2  10   // มอเตอร์ซ้าย  — ถอยหลัง
#define PIN_R1   5   // มอเตอร์ขวา  — ไปข้างหน้า
#define PIN_R2   6   // มอเตอร์ขวา  — ถอยหลัง


// ── 2. PIN และ Threshold ของ Sensor เส้น ────────────────────
//   analogRead > THRESHOLD → เจอเส้นขาว (ออกนอกสนาม)
#define PIN_LINE_L   A2    // sensor เส้นซ้าย
#define PIN_LINE_R   A7    // sensor เส้นขวา

#define LINE_L_TH   920   // threshold ซ้าย
#define LINE_R_TH   500   // threshold ขวา


// ── 3. PIN ของ Sharp GP2Y0A41SK0F ───────────────────────────
//   หลีกเลี่ยง A2 / A7 (ใช้ sensor เส้นแล้ว)
//   SL = Side Left, FL = Front Left, FC = Front Center
//   FR = Front Right, SR = Side Right
#define PIN_SHARP_SL  A0
#define PIN_SHARP_FL  A1
#define PIN_SHARP_FC  A3
#define PIN_SHARP_FR  A4
#define PIN_SHARP_SR  A5


// ── 4. ค่าแปลงสัญญาณ Sharp GP2Y ─────────────────────────────
//   ช่วงวัดได้จริงของ GP2Y0A41SK0F: ~40–300 mm
//   library SharpIR จัดการ curve จาก datasheet ให้อัตโนมัติ
//   เปลี่ยน SAMPLES เพื่อลด noise (เพิ่ม = เสถียรขึ้น แต่ช้าลง)
#define SHARP_MAX_MM   300   // ระยะสูงสุดที่เชื่อถือได้ (mm)
#define SHARP_MIN_MM    40   // ระยะต่ำสุดที่เชื่อถือได้ (mm)
#define SHARP_SAMPLES    3   // จำนวนครั้ง oversample ต่อการอ่าน 1 ครั้ง


// ── 5. ระยะตัดสินใจ (หน่วย mm) ─────────────────────────────
//   RAM_DIST   — ใกล้กว่านี้ → พุ่งชน
//   TRACK_DIST — เห็นศัตรูแต่ยังไกล → ติดตาม
//   SIDE_DIST  — sensor ข้างเจอศัตรูที่ระยะนี้ → เริ่มหัน
#define RAM_DIST    180
#define TRACK_DIST  280
#define SIDE_DIST   200


// ── 6. ความเร็วมอเตอร์ (0–255) ──────────────────────────────
#define ATTACK_SPEED  255   // พุ่งชน — เต็มสปีด

#define TRACK_FAST    220   // ติดตาม — ด้านที่หันหาศัตรู
#define TRACK_SLOW    130   // ติดตาม — ด้านตรงข้าม (ใช้เฉพาะ sensor ข้าง)

#define SEARCH_SPEED  100   // หมุนค้นหา — ช้า (ประหยัดพื้นที่)
#define SEARCH_FAST   120   // หมุนหลัง lock ได้ — เร็วขึ้น (ไม่ได้ใช้ในปัจจุบัน)

#define ESCAPE_BACK   200   // ถอยหนีเส้น
#define ESCAPE_TURN   200   // หมุนกลับเข้าสนาม


// ── 7. เวลา (หน่วย ms) ──────────────────────────────────────
#define ESCAPE_BACK_MS   180   // ระยะเวลาถอยหนีเส้น
#define ESCAPE_TURN_MS   220   // ระยะเวลาหมุนกลับ

#define LOCK_TIME        500   // lock เป้าหมายไว้นานแค่ไหน ก่อนถือว่าหาย
#define SEARCH_SWAP_MS   600   // ระยะเวลาหมุนค้นหาในแต่ละทิศ
#define SEARCH_FWD_MS    200   // ระยะเวลาเดินหน้าก่อนหมุน (spiral search)


// ── 8. ค่าพิเศษ ─────────────────────────────────────────────
//   TRACK_CENTER_ZONE — ถ้า |fl-fr| < ค่านี้ ถือว่าศัตรูอยู่ตรงหน้า → วิ่งตรง
//   NO_TARGET         — ค่าที่แทน "ไม่มีเป้าหมาย"
#define TRACK_CENTER_ZONE   40
#define NO_TARGET          999
