#include "Sensors.h"
#include "Config.h"
#include <SharpIR.h>

// ============================================================
//  Sensors.cpp — อ่านค่า Sharp GP2Y0A41SK0F × 5 และ sensor เส้น × 2
//
//  ต้องติดตั้ง library ก่อน:
//    Arduino IDE → Library Manager → ค้นหา "SharpIR" → Install
//
//  library จะแปลงค่า Analog ของ sensor ให้เป็นระยะ (cm) อัตโนมัติ
//  เราแค่ต้องแปลงต่อจาก cm → mm และกรองค่าที่ไม่น่าเชื่อถือออก
// ============================================================


// ── สร้าง object sensor ทั้ง 5 ตัว ─────────────────────────
//
//  SharpIR(รุ่นเซนเซอร์, ขา analog)
//  รุ่น GP2Y0A41SK0F คือรหัสที่ library ใช้เพื่อเลือก curve ที่ถูกต้อง
//
//  วางใน namespace {} เพื่อซ่อน ไม่ให้ไฟล์อื่นเรียกใช้โดยตรง

namespace {

  SharpIR sensorSL(SharpIR::GP2Y0A41SK0F, PIN_SHARP_SL);
  SharpIR sensorFL(SharpIR::GP2Y0A41SK0F, PIN_SHARP_FL);
  SharpIR sensorFC(SharpIR::GP2Y0A41SK0F, PIN_SHARP_FC);
  SharpIR sensorFR(SharpIR::GP2Y0A41SK0F, PIN_SHARP_FR);
  SharpIR sensorSR(SharpIR::GP2Y0A41SK0F, PIN_SHARP_SR);


  // อ่านค่าจาก sensor หลายครั้งแล้วเฉลี่ย เพื่อลด noise
  // SHARP_SAMPLES กำหนดใน Config.h (ค่าเริ่มต้น = 3)
  int readAverage(SharpIR& sensor) {
    long sum = 0;
    for (int i = 0; i < SHARP_SAMPLES; i++) {
      sum += sensor.distance();
    }
    return (int)(sum / SHARP_SAMPLES);
  }

  // แปลงค่าจาก cm → mm และกรองค่านอกช่วงที่เชื่อถือได้ออก
  //
  // ทำไมต้องกรอง? เพราะ sensor นี้วัดแม่นแค่ 40–300 mm
  // ถ้าใกล้กว่า 40mm ค่าจะบิดเบี้ยว, ไกลกว่า 300mm วัดไม่ถึง
  int toMM(int cm) {
    int mm = cm * 10;

    if (mm < SHARP_MIN_MM || mm > SHARP_MAX_MM) {
      return NO_TARGET;  // ค่าไม่น่าเชื่อถือ → แทนด้วย "ไม่เจอ"
    }

    return mm;
  }

} // ปิด namespace (anonymous)


// ── Public API ───────────────────────────────────────────────

void Sensors::begin() {
  Serial.begin(115200);  // เปิด Serial สำหรับ debug ผ่าน Serial Monitor

  // Sharp GP2Y เป็น Analog sensor → ตั้งขาเป็น INPUT
  pinMode(PIN_SHARP_SL, INPUT);
  pinMode(PIN_SHARP_FL, INPUT);
  pinMode(PIN_SHARP_FC, INPUT);
  pinMode(PIN_SHARP_FR, INPUT);
  pinMode(PIN_SHARP_SR, INPUT);

  // sensor เส้นก็เป็น Analog เช่นกัน
  pinMode(PIN_LINE_L, INPUT);
  pinMode(PIN_LINE_R, INPUT);

  Serial.println(F("[Sensors] พร้อมใช้งาน"));
  // F("...") = เก็บข้อความใน Flash memory แทน RAM ประหยัด memory ได้
}


// อ่านค่าระยะจาก sensor ทั้ง 5 ตัว
// .distance() คืนค่าเป็น cm จากนั้นเรา toMM() แปลงเป็น mm
Dist Sensors::readDist() {
  Dist d;
  d.sl = toMM(readAverage(sensorSL));
  d.fl = toMM(readAverage(sensorFL));
  d.fc = toMM(readAverage(sensorFC));
  d.fr = toMM(readAverage(sensorFR));
  d.sr = toMM(readAverage(sensorSR));
  return d;
}


// อ่านค่า sensor เส้นทั้งสองข้าง
Line Sensors::readLine() {
  Line l;

  // analogRead() คืนค่า 0–1023
  l.leftRaw  = analogRead(PIN_LINE_L);
  l.rightRaw = analogRead(PIN_LINE_R);

  // เปรียบกับ Threshold → ถ้าเกิน = เจอเส้นขาว
  l.left  = l.leftRaw  > LINE_L_TH;
  l.right = l.rightRaw > LINE_R_TH;

  return l;
}
