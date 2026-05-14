#pragma once

// ============================================================
//  Sensors.h — โครงสร้างข้อมูลและ API อ่านค่า Sensor
// ============================================================

// ── ข้อมูลระยะจาก ToF ทั้ง 5 ตัว (หน่วย mm) ────────────────
//   NO_TARGET (999) = ไม่เจอเป้า หรือ timeout
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

  // เริ่มต้น I2C + ToF + pin เส้น (เรียกใน setup() ครั้งเดียว)
  void begin();

  // อ่านระยะจาก ToF ทั้ง 5 ตัว
  Dist readDist();

  // อ่าน sensor เส้นทั้งสองข้าง
  Line readLine();
}
