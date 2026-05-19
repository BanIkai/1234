#pragma once

// ============================================================
//  Sensors.h — โครงสร้างข้อมูลและ API อ่านค่า Sensor
//  [แก้ไข] ใช้ Sharp GP2Y0A41SK0F (Analog) แทน VL53L0X (I2C)
//          Interface ภายนอก (Dist / Line / begin / readDist / readLine)
//          เหมือนเดิมทุกอย่าง — Strategy.cpp ไม่ต้องแก้
// ============================================================

// ── ข้อมูลระยะจาก Sharp sensor ทั้ง 5 ตัว (หน่วย mm) ────────
//   NO_TARGET (999) = ไม่เจอเป้า หรือค่านอกช่วง sensor
struct Dist {
  int sl;   // Side  Left  — ด้านข้างซ้าย
  int fl;   // Front Left  — หน้าซ้าย
  int fc;   // Front Center— หน้ากลาง
  int fr;   // Front Right — หน้าขวา
  int sr;   // Side  Right — ด้านข้างขวา
};

// ── ข้อมูล sensor เส้น ──────────────────────────────────────
//   left/right = true  หมายถึง "เจอเส้นขาว" (ออกนอกสนาม)
struct Line {
  bool left;      // เจอเส้นซ้าย?
  bool right;     // เจอเส้นขวา?
  int  leftRaw;   // ค่าดิบ analogRead ซ้าย
  int  rightRaw;  // ค่าดิบ analogRead ขวา
};


namespace Sensors {

  // เริ่มต้น pin Analog + pin เส้น (เรียกใน setup() ครั้งเดียว)
  // [หมายเหตุ] ไม่ต้องเรียก Wire.begin() อีกต่อไป เพราะไม่ใช้ I2C
  void begin();

  // อ่านระยะจาก Sharp sensor ทั้ง 5 ตัว
  Dist readDist();

  // อ่าน sensor เส้นทั้งสองข้าง
  Line readLine();
}
