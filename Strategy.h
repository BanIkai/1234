#pragma once
#include <Arduino.h>
#include "Sensors.h"

// =============================================================================
//  Strategy.h — สถานะและ API ของตรรกะการต่อสู้
// =============================================================================

// สถานะปัจจุบันของหุ่น (ใช้ใน debug และ telemetry)
enum SumoState : uint8_t {
  STATE_LINE_ESCAPE,   // หนีเส้นขอบ
  STATE_ANTI_FLANK,    // หมุนรับมือภัยด้านข้าง
  STATE_RAM,           // พุ่งชน
  STATE_TRACK,         // ไล่ตาม (ปรับทิศ)
  STATE_SEARCH,        // หมุนค้นหาคู่ต่อสู้
};

namespace Strategy {

  // รีเซ็ตตัวแปร state ทั้งหมด — เรียกก่อน loop เริ่ม
  void reset();

  // ฟังก์ชันหลัก: ตัดสินใจและสั่งมอเตอร์ในแต่ละ loop
  // รับค่าจากเซ็นเซอร์ → คืน SumoState ปัจจุบัน (ไว้ดูใน debug)
  SumoState step(const ToFReadings& tof, const LineReadings& line);

}
