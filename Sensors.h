#pragma once

// ============================================================
//  Sensors.h — โครงสร้างข้อมูลและ API อ่านค่า Sensor
//
//  ไฟล์นี้กำหนด "รูปแบบข้อมูล" ที่ sensor จะส่งคืนมา
//  โค้ดจริงอยู่ใน Sensors.cpp
// ============================================================


// ── ข้อมูลระยะจาก Sensor รอบตัว ─────────────────────────────
//
//  หน่วยเป็น mm (มิลลิเมตร)
//  ถ้าเป็น NO_TARGET (999) = ไม่เจอวัตถุ หรือ sensor อ่านไม่ได้
//
//  ตำแหน่งของ sensor:
//
//         [sl]  [fl][fc][fr]  [sr]
//          ◄─       ▲▲▲       ─►
//        ข้าง     หน้าหุ่น     ข้าง
//        ซ้าย                  ขวา

struct Dist {
  int sl;   // Side Left  — sensor ข้างซ้าย
  int fl;   // Front Left — sensor หน้าซ้าย
  int fc;   // Front Center — sensor หน้ากลาง
  int fr;   // Front Right — sensor หน้าขวา
  int sr;   // Side Right — sensor ข้างขวา
};


// ── ข้อมูล Sensor เส้น ───────────────────────────────────────
//
//  left/right = true  → "เจอเส้นขาว" → หุ่นกำลังจะออกนอกสนาม!
//  leftRaw/rightRaw   → ค่าดิบจาก analogRead (0–1023) ไว้ debug

struct Line {
  bool left;      // เจอเส้นขาวซ้าย?
  bool right;     // เจอเส้นขาวขวา?
  int  leftRaw;   // ค่าดิบ sensor เส้นซ้าย (ไว้ดูตอน calibrate)
  int  rightRaw;  // ค่าดิบ sensor เส้นขวา
};


// ── API ──────────────────────────────────────────────────────

namespace Sensors {

  // เตรียม sensor ทั้งหมด — เรียกใน setup() ครั้งเดียว
  void begin();

  // อ่านระยะจาก sensor รอบตัวทั้ง 5 ตัว คืนค่าเป็น Dist
  Dist readDist();

  // อ่านค่า sensor เส้นทั้งสองข้าง คืนค่าเป็น Line
  Line readLine();

}
