#pragma once
#include <Arduino.h>

// =============================================================================
//  Sensors.h — โครงสร้างข้อมูลและ API ของเซ็นเซอร์ทั้งหมด
// =============================================================================

// ระยะทาง (mm) จากเซ็นเซอร์วัดระยะ VL53L0X ทั้ง 5 ตัว
// ถ้าไม่มีเป้าหมาย / วัดไม่ได้ → ค่าจะเท่ากับ TOF_NO_TARGET
struct ToFReadings {
  uint16_t sl;  // Side Left  — ด้านข้างซ้าย
  uint16_t fl;  // Front Left  — หน้าซ้าย
  uint16_t fc;  // Front Center — หน้ากลาง (ตัวบ่งชี้หลัก)
  uint16_t fr;  // Front Right — หน้าขวา
  uint16_t sr;  // Side Right  — ด้านข้างขวา
};

// ผลการอ่านเซ็นเซอร์ตรวจเส้น
struct LineReadings {
  bool left_white;   // true = เซ็นเซอร์ซ้ายเห็นเส้นขาว (ขอบสนาม)
  bool right_white;  // true = เซ็นเซอร์ขวาเห็นเส้นขาว
  int  left_raw;     // ค่า analogRead ดิบ (ใช้ debug)
  int  right_raw;
};

namespace Sensors {

  // เรียกใน setup() หลัง Wire.begin() — ปลุกเซ็นเซอร์ VL53L0X ทีละตัว
  // และกำหนด I2C address ใหม่ไม่ซ้ำกัน
  void initToF();

  // อ่านระยะทางจากเซ็นเซอร์ทั้ง 5 ตัว (เรียกทุก loop)
  ToFReadings readToF();

  // อ่านเซ็นเซอร์ตรวจเส้น และเปรียบเทียบกับ threshold ใน Config.h
  LineReadings readLine();

}
