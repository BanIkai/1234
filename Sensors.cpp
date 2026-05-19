#include "Sensors.h"
#include "Config.h"

// ============================================================
//  Sensors.cpp — จัดการ Sharp GP2Y0A41SK0F × 5 + sensor เส้น × 2
//
//  การแปลงค่า Analog → mm:
//    Vout ของ GP2Y0A41SK0F สัมพันธ์กับระยะแบบไม่เชิงเส้น
//    ใช้สูตร:  mm = SHARP_K / analogRead(pin)
//    (ดัดแปลงจาก datasheet curve และ fitting จริง)
//
//    ข้อควรระวัง:
//      - ระยะ < 40 mm  → sensor ให้ค่าบิดเบน → ตัดทิ้ง (NO_TARGET)
//      - ระยะ > 300 mm → สัญญาณอ่อนมาก    → ตัดทิ้ง (NO_TARGET)
//      - ค่า raw = 0   → ป้องกัน divide-by-zero
//
//  ไม่ต้องการ:
//    - Wire / I2C
//    - XSHUT pin
//    - Library พิเศษ (ใช้แค่ analogRead() มาตรฐาน)
// ============================================================

namespace {

  // ── แปลง analogRead → mm ────────────────────────────────
  //   รับ pin, คืน mm หรือ NO_TARGET ถ้านอกช่วง
  int sharpReadMM(int pin) {

    // Oversample: อ่านหลายครั้งแล้วเฉลี่ย ลด noise จาก ADC
    long sum = 0;
    for (int i = 0; i < SHARP_SAMPLES; i++) {
      sum += analogRead(pin);
      delayMicroseconds(200);   // รอให้ S&H ของ ADC settle ระหว่างรอบ
    }
    int raw = (int)(sum / SHARP_SAMPLES);

    // ป้องกัน divide-by-zero และ raw ต่ำมาก (sensor ถูกบล็อก / ไฟดับ)
    if (raw <= 0) return NO_TARGET;

    // แปลงเป็น mm ด้วยสูตร inverse
    int mm = (int)(SHARP_K / (float)(raw + SHARP_OFFSET));

    // กรองค่านอกช่วงที่เชื่อถือได้ของ GP2Y0A41SK0F
    if (mm < SHARP_MIN_MM || mm > SHARP_MAX_MM) return NO_TARGET;

    return mm;
  }

} // namespace (anonymous)


// ── public API ───────────────────────────────────────────────

void Sensors::begin() {
  Serial.begin(115200);   // เปิด Serial สำหรับ debug

  // Sharp GP2Y เป็น Analog OUTPUT — ตั้งเป็น INPUT เพื่อ analogRead
  // (Arduino Uno/Nano: A0–A5 เป็น INPUT by default แต่ตั้งชัดเจนไว้ดีกว่า)
  pinMode(PIN_SHARP_SL, INPUT);
  pinMode(PIN_SHARP_FL, INPUT);
  pinMode(PIN_SHARP_FC, INPUT);
  pinMode(PIN_SHARP_FR, INPUT);
  pinMode(PIN_SHARP_SR, INPUT);

  // pin sensor เส้น
  pinMode(PIN_LINE_L, INPUT);
  pinMode(PIN_LINE_R, INPUT);

  Serial.println(F("[Sensors] Sharp GP2Y0A41SK0F x5 ready (Analog mode)"));
}


Dist Sensors::readDist() {
  Dist d;
  d.sl = sharpReadMM(PIN_SHARP_SL);
  d.fl = sharpReadMM(PIN_SHARP_FL);
  d.fc = sharpReadMM(PIN_SHARP_FC);
  d.fr = sharpReadMM(PIN_SHARP_FR);
  d.sr = sharpReadMM(PIN_SHARP_SR);
  return d;
}


Line Sensors::readLine() {
  Line l;
  l.leftRaw  = analogRead(PIN_LINE_L);
  l.rightRaw = analogRead(PIN_LINE_R);
  l.left     = l.leftRaw  > LINE_L_TH;   // ค่าสูง = เจอเส้นขาว
  l.right    = l.rightRaw > LINE_R_TH;   // ค่าสูง = เจอเส้นขาว
  return l;
}
