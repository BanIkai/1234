#include "Motors.h"
#include "Config.h"
#include <Arduino.h>

// ============================================================
//  Motors.cpp — ไดร์ฟมอเตอร์แบบ H-Bridge (2 pin ต่อมอเตอร์)
//
//  วงจร:  speed > 0  → PIN_x1 = PWM,  PIN_x2 = LOW  (หน้า)
//          speed < 0  → PIN_x1 = LOW,  PIN_x2 = PWM  (หลัง)
//          speed = 0  → ทั้งคู่ LOW                  (หยุด)
// ============================================================

namespace {

  // ── helper: ขับมอเตอร์ตัวเดียว ───────────────────────────
  void driveMotor(int pin1, int pin2, int speed) {
    speed = constrain(speed, -255, 255);

    if (speed > 0) {
      analogWrite(pin1, speed);
      digitalWrite(pin2, LOW);
    }
    else if (speed < 0) {
      digitalWrite(pin1, LOW);
      analogWrite(pin2, -speed);
    }
    else {
      digitalWrite(pin1, LOW);
      digitalWrite(pin2, LOW);
    }
  }

} // namespace (anonymous)


// ── public API ───────────────────────────────────────────────

void Motors::begin() {
  pinMode(PIN_L1, OUTPUT);
  pinMode(PIN_L2, OUTPUT);
  pinMode(PIN_R1, OUTPUT);
  pinMode(PIN_R2, OUTPUT);
}

void Motors::move(int leftSpeed, int rightSpeed) {
  driveMotor(PIN_L1, PIN_L2, leftSpeed);
  driveMotor(PIN_R1, PIN_R2, rightSpeed);
}

void Motors::stop() {
  move(0, 0);
}
