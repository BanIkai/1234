#include "Sensors.h"
#include "Config.h"
#include <Wire.h>
#include <VL53L0X.h>

// ============================================================
//  Sensors.cpp — จัดการ ToF × 5 + sensor เส้น × 2
//
//  ขั้นตอน init ToF:
//    1. ดึง XSHUT ลง LOW ทุกตัว (ปิดหมด)
//    2. เปิดทีละตัว → init → ตั้ง address เฉพาะ
//  วิธีนี้ป้องกัน address ชนกันบน I2C bus เดียว
// ============================================================

namespace {

  // ── instance ToF แต่ละตัว ────────────────────────────────
  VL53L0X tofSL, tofFL, tofFC, tofFR, tofSR;

  // ── เปิด ToF 1 ตัว แล้วตั้ง address ────────────────────
  void initOneSensor(VL53L0X& sensor, int xshutPin, int i2cAddress, const char* name) {
    digitalWrite(xshutPin, HIGH);   // ปลุก sensor ขึ้นมา
    delay(10);                      // รอ boot

    sensor.setTimeout(TOF_TIMEOUT);

    if (!sensor.init()) {
      // แจ้งชื่อ sensor ที่ fail แล้วค้างไว้ (ง่ายต่อการ debug)
      Serial.print(F("[ERROR] ToF init failed: "));
      Serial.println(name);
      while (true);
    }

    sensor.setAddress(i2cAddress);
    sensor.startContinuous();
  }

} // namespace (anonymous)


// ── public API ───────────────────────────────────────────────

void Sensors::begin() {
  Serial.begin(115200);   // เปิด Serial สำหรับ debug (ดู error ตอน init)
  Wire.begin();

  // ตั้ง pin XSHUT เป็น OUTPUT
  int xshutPins[] = {
    PIN_XSHUT_SL, PIN_XSHUT_FL, PIN_XSHUT_FC,
    PIN_XSHUT_FR, PIN_XSHUT_SR
  };
  for (int pin : xshutPins) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);   // ปิดทุกตัวก่อน
  }
  delay(10);

  // เปิด ToF ทีละตัว พร้อมกำหนด address
  initOneSensor(tofSL, PIN_XSHUT_SL, ADDR_SL, "SL");
  initOneSensor(tofFL, PIN_XSHUT_FL, ADDR_FL, "FL");
  initOneSensor(tofFC, PIN_XSHUT_FC, ADDR_FC, "FC");
  initOneSensor(tofFR, PIN_XSHUT_FR, ADDR_FR, "FR");
  initOneSensor(tofSR, PIN_XSHUT_SR, ADDR_SR, "SR");

  // pin sensor เส้น
  pinMode(PIN_LINE_L, INPUT);
  pinMode(PIN_LINE_R, INPUT);
}


Dist Sensors::readDist() {
  Dist d;
  d.sl = tofSL.readRangeContinuousMillimeters();
  d.fl = tofFL.readRangeContinuousMillimeters();
  d.fc = tofFC.readRangeContinuousMillimeters();
  d.fr = tofFR.readRangeContinuousMillimeters();
  d.sr = tofSR.readRangeContinuousMillimeters();

  // ถ้า timeout ให้ใส่ NO_TARGET แทนค่าผิดพลาด
  if (tofSL.timeoutOccurred()) d.sl = NO_TARGET;
  if (tofFL.timeoutOccurred()) d.fl = NO_TARGET;
  if (tofFC.timeoutOccurred()) d.fc = NO_TARGET;
  if (tofFR.timeoutOccurred()) d.fr = NO_TARGET;
  if (tofSR.timeoutOccurred()) d.sr = NO_TARGET;

  // กรองค่าเกินจริง (VL53L0X คืน 8190 หรือ > 1200mm เมื่อวัดไม่ได้)
  // สนามซูโม่มาตรฐาน ~770mm เส้นผ่านศูนย์กลาง ตัด > 1200 ทิ้ง
  const int MAX_VALID = 1200;
  if (d.sl > MAX_VALID) d.sl = NO_TARGET;
  if (d.fl > MAX_VALID) d.fl = NO_TARGET;
  if (d.fc > MAX_VALID) d.fc = NO_TARGET;
  if (d.fr > MAX_VALID) d.fr = NO_TARGET;
  if (d.sr > MAX_VALID) d.sr = NO_TARGET;

  return d;
}


Line Sensors::readLine() {
  Line l;
  l.leftRaw  = analogRead(PIN_LINE_L);
  l.rightRaw = analogRead(PIN_LINE_R);
  l.left     = l.leftRaw  > LINE_L_TH;   // เกิน threshold = เจอเส้น
  l.right    = l.rightRaw > LINE_R_TH;
  return l;
}
