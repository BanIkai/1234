// =============================================================================
//  sumo.ino — ไฟล์หลัก Arduino สำหรับหุ่นยนต์ซูโม่
//
//  โครงสร้างโปรเจกต์:
//   Config.h    — ค่า pin, ระยะ, timing ทุกตัว (แก้ที่นี่เพื่อ tune หุ่น)
//   Motors      — สั่งมอเตอร์ซ้าย-ขวา
//   Sensors     — อ่านระยะ VL53L0X และเซ็นเซอร์เส้น
//   Strategy    — ตรรกะการต่อสู้ (LINE_ESCAPE → ANTI_FLANK → RAM → TRACK → SEARCH)
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include "Config.h"
#include "Sensors.h"
#include "Motors.h"
#include "Strategy.h"

// สถานะของหุ่นในระดับ main loop
enum RunMode : uint8_t {
  MODE_IDLE,     // รอสัญญาณ Start
  MODE_ARMED,    // กด Start แล้ว กำลังนับถอยหลัง
  MODE_RUNNING,  // กำลังต่อสู้
  MODE_KILLED,   // ถูก Kill — หยุดถาวร
};

static RunMode      currentMode  = MODE_IDLE;
static volatile bool killFlag    = false;  // set โดย ISR — volatile!

// =============================================================================
//  ISR: Kill Switch — เรียกเมื่อขา KILL ขึ้น HIGH
//  ต้องทำงานเร็วที่สุด: ดึงมอเตอร์ลง LOW ทันที ห้าม PWM/Wire ใน ISR
// =============================================================================
void IRAM_ATTR onKillSignal() {
  digitalWrite(PIN_LEFT_IN1,  LOW);
  digitalWrite(PIN_LEFT_IN2,  LOW);
  digitalWrite(PIN_RIGHT_IN1, LOW);
  digitalWrite(PIN_RIGHT_IN2, LOW);
  killFlag = true;
}

// =============================================================================
//  ฟังก์ชันช่วยสำหรับ setup()
// =============================================================================

// ตรวจปุ่ม Start (active-low + debounce)
static bool isStartButtonPressed() {
  if (digitalRead(PIN_START_BTN) != LOW) return false;
  delay(BTN_DEBOUNCE_MS);
  return digitalRead(PIN_START_BTN) == LOW;
}

// ตรวจสัญญาณ Start ภายนอก
static bool isExternalStartHigh() {
  return digitalRead(PIN_START_EXT) == HIGH;
}

// กระพริบ LED รอ Start — ออกเมื่อกดปุ่มหรือรับสัญญาณภายนอก
static void waitForStartSignal() {
  pinMode(LED_BUILTIN, OUTPUT);
  uint32_t lastToggle = 0;
  bool     ledState   = false;

  while (true) {
    if (isExternalStartHigh() || isStartButtonPressed()) return;

    // กระพริบทุก 500 ms (heart beat)
    uint32_t now = millis();
    if (now - lastToggle >= 500) {
      ledState = !ledState;
      digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
      lastToggle = now;
    }
  }
}

// ไฟ LED ติดค้าง + รอ START_DELAY_MS ก่อนออกวิ่ง (มาตรฐานซูโม่ = 5 วินาที)
static void countdownBeforeRun() {
  digitalWrite(LED_BUILTIN, HIGH);
  uint32_t startTime = millis();
  while ((int32_t)(millis() - startTime) < START_DELAY_MS) {
    if (killFlag) return;
    delay(10);
  }
}

// =============================================================================
//  setup() — ทำงานครั้งเดียวตอนเปิดเครื่อง
// =============================================================================
void setup() {
#if SUMO_DEBUG
  Serial.begin(115200);
#endif

  // ตั้งค่าขา Input
  pinMode(PIN_START_EXT, INPUT);
  pinMode(PIN_START_BTN, INPUT_PULLUP);
  pinMode(PIN_KILL,      INPUT);   // ต้องมีตัวต้านทาน pull-down ภายนอกที่ D3
  pinMode(PIN_LINE_L,    INPUT);
  pinMode(PIN_LINE_R,    INPUT);

  // เริ่มต้นมอเตอร์ก่อน → รับประกันว่าหยุดอยู่ก่อน init อื่น
  Motors::begin();
  Motors::coast();

  // เริ่ม I2C และเซ็นเซอร์วัดระยะ
  Wire.begin();
  Wire.setClock(I2C_CLOCK_HZ);
  Sensors::initToF();

  // ผูก Kill interrupt หลัง pin มอเตอร์พร้อมแล้ว
  // (เพื่อให้ ISR เรียก digitalWrite ได้ทันที)
  attachInterrupt(digitalPinToInterrupt(PIN_KILL), onKillSignal, RISING);

#if DEBUG_MOTOR_TEST
  // ทดสอบสายมอเตอร์ — กระตุกสั้น ๆ แล้วหยุด
  Motors::drive(120, 0);    delay(250);
  Motors::drive(0, 120);    delay(250);
  Motors::drive(-120, -120); delay(250);
  Motors::drive(0, 0);
#endif

  Strategy::reset();

  // รอสัญญาณ Start
  waitForStartSignal();
  if (killFlag) { currentMode = MODE_KILLED; return; }
  currentMode = MODE_ARMED;

  // นับถอยหลังตามกติกา
  countdownBeforeRun();
  if (killFlag) { currentMode = MODE_KILLED; return; }
  currentMode = MODE_RUNNING;
}

// =============================================================================
//  loop() — ทำงานซ้ำทุก cycle
// =============================================================================
void loop() {

  // ถูก Kill → หยุดและไม่ทำอะไรอีกเลย
  if (killFlag) {
    Motors::coast();
    currentMode = MODE_KILLED;
#if SUMO_DEBUG
    static bool killPrinted = false;
    if (!killPrinted) {
      Serial.println(F("KILLED"));
      killPrinted = true;
    }
#endif
    return;
  }

  if (currentMode != MODE_RUNNING) return;

  // อ่านเซ็นเซอร์
  ToFReadings  tof  = Sensors::readToF();
  LineReadings line = Sensors::readLine();

  // ตัดสินใจและสั่งมอเตอร์
  SumoState state = Strategy::step(tof, line);

  // ---- Debug: พิมพ์ทุก 100 ms ----
#if SUMO_DEBUG
  static uint32_t lastPrint = 0;
  uint32_t now = millis();
  if (now - lastPrint >= 100) {
    lastPrint = now;
    Serial.print(F("state="));  Serial.print((int)state);
    Serial.print(F(" SL="));    Serial.print(tof.sl);
    Serial.print(F(" FL="));    Serial.print(tof.fl);
    Serial.print(F(" FC="));    Serial.print(tof.fc);
    Serial.print(F(" FR="));    Serial.print(tof.fr);
    Serial.print(F(" SR="));    Serial.print(tof.sr);
    Serial.print(F(" lineL=")); Serial.print(line.left_raw);
    Serial.print(line.left_white  ? F("(W)") : F("(b)"));
    Serial.print(F(" lineR=")); Serial.print(line.right_raw);
    Serial.println(line.right_white ? F("(W)") : F("(b)"));
  }
#endif
}
