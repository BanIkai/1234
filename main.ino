#include "Motors.h"
#include "Sensors.h"
#include "Strategy.h"

// ============================================================
//  main.ino — จุดเริ่มต้นโปรแกรม
// ============================================================

void setup() {
  Motors::begin();    // เริ่มต้น pin มอเตอร์
  Sensors::begin();   // เริ่มต้น ToF × 5 + sensor เส้น
  AI::reset();        // reset สถานะ AI
}

void loop() {
  Dist dist = Sensors::readDist();   // อ่านระยะจาก ToF ทั้ง 5 ตัว
  Line line = Sensors::readLine();   // อ่าน sensor เส้น
  AI::run(dist, line);               // ตัดสินใจและสั่งมอเตอร์
}
