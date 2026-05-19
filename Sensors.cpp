#include "Sensors.h"
#include "Config.h"
#include <SharpIR.h>

// ============================================================
//  Sensors.cpp — จัดการ Sharp GP2Y0A41SK0F × 5 + sensor เส้น × 2
//  [แบบใช้ Library SharpIR]
//
//  ติดตั้ง library ก่อน:
//    Arduino IDE → Library Manager → ค้นหา "SharpIR" → Install
//
//  library จะจัดการแปลงค่า Analog → cm ให้อัตโนมัติ
//  โดยใช้ curve จาก datasheet ของ GP2Y0A41SK0F
// ============================================================

namespace {

  // GP2Y0A41SK0F คือ model ที่ระบุใน SharpIR library
  SharpIR sensorSL(SharpIR::GP2Y0A41SK0F, PIN_SHARP_SL);
  SharpIR sensorFL(SharpIR::GP2Y0A41SK0F, PIN_SHARP_FL);
  SharpIR sensorFC(SharpIR::GP2Y0A41SK0F, PIN_SHARP_FC);
  SharpIR sensorFR(SharpIR::GP2Y0A41SK0F, PIN_SHARP_FR);
  SharpIR sensorSR(SharpIR::GP2Y0A41SK0F, PIN_SHARP_SR);

  // ── แปลง cm → mm และกรองค่านอกช่วง ────────────────────────
  int toMM(int cm) {
    int mm = cm * 10;
    if (mm < SHARP_MIN_MM || mm > SHARP_MAX_MM) return NO_TARGET;
    return mm;
  }

} // namespace (anonymous)


// ── public API ───────────────────────────────────────────────

void Sensors::begin() {
  Serial.begin(115200);

  // Sharp GP2Y เป็น Analog — ตั้ง pin เป็น INPUT
  pinMode(PIN_SHARP_SL, INPUT);
  pinMode(PIN_SHARP_FL, INPUT);
  pinMode(PIN_SHARP_FC, INPUT);
  pinMode(PIN_SHARP_FR, INPUT);
  pinMode(PIN_SHARP_SR, INPUT);

  // pin sensor เส้น
  pinMode(PIN_LINE_L, INPUT);
  pinMode(PIN_LINE_R, INPUT);

  Serial.println(F("[Sensors] SharpIR library x5 ready"));
}


Dist Sensors::readDist() {
  Dist d;
  // distance() คืนค่า cm → แปลงเป็น mm ด้วย toMM()
  d.sl = toMM(sensorSL.distance());
  d.fl = toMM(sensorFL.distance());
  d.fc = toMM(sensorFC.distance());
  d.fr = toMM(sensorFR.distance());
  d.sr = toMM(sensorSR.distance());
  return d;
}


Line Sensors::readLine() {
  Line l;
  l.leftRaw  = analogRead(PIN_LINE_L);
  l.rightRaw = analogRead(PIN_LINE_R);
  l.left     = l.leftRaw  > LINE_L_TH;
  l.right    = l.rightRaw > LINE_R_TH;
  return l;
}
