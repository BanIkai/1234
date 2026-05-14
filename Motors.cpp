#include "Motors.h"
#include "Config.h"

// =============================================================================
//  Motors.cpp — ฟังก์ชันควบคุมมอเตอร์ภายใน
// =============================================================================

namespace {

  // ขับมอเตอร์หนึ่งช่องผ่าน half-bridge A4950 แบบ slow-decay sign-magnitude
  //
  //   speed > 0  →  IN1 = PWM, IN2 = LOW   (หน้า)
  //   speed < 0  →  IN1 = LOW, IN2 = PWM   (หลัง)
  //   speed = 0  →  IN1 = HIGH, IN2 = HIGH  (เบรก)
  //
  void driveChannel(uint8_t pin_in1, uint8_t pin_in2, int speed) {
    // จำกัดขอบเขต
    speed = constrain(speed, -255, 255);

    if (speed > 0) {
      analogWrite(pin_in1, speed);
      digitalWrite(pin_in2, LOW);
    } else if (speed < 0) {
      digitalWrite(pin_in1, LOW);
      analogWrite(pin_in2, -speed);  // PWM ต้องเป็นค่าบวก
    } else {
      // เบรก: ทั้งสองขา HIGH พร้อมกัน
      digitalWrite(pin_in1, HIGH);
      digitalWrite(pin_in2, HIGH);
    }
  }

}  // namespace (private)

// -----------------------------------------------------------------------------

void Motors::begin() {
  pinMode(PIN_LEFT_IN1,  OUTPUT);
  pinMode(PIN_LEFT_IN2,  OUTPUT);
  pinMode(PIN_RIGHT_IN1, OUTPUT);
  pinMode(PIN_RIGHT_IN2, OUTPUT);
  coast();  // ปล่อยล้อตั้งต้น — ห้ามเคลื่อนที่ก่อน setup เสร็จ
}

void Motors::drive(int left, int right) {
  driveChannel(PIN_LEFT_IN1,  PIN_LEFT_IN2,  left);
  driveChannel(PIN_RIGHT_IN1, PIN_RIGHT_IN2, right);
}

void Motors::brake() {
  // HIGH ทุกขา = เบรกแข็งทันที
  digitalWrite(PIN_LEFT_IN1,  HIGH);
  digitalWrite(PIN_LEFT_IN2,  HIGH);
  digitalWrite(PIN_RIGHT_IN1, HIGH);
  digitalWrite(PIN_RIGHT_IN2, HIGH);
}

void Motors::coast() {
  // LOW ทุกขา = ปล่อยล้อหมุนฟรี (ใช้หลัง Kill)
  digitalWrite(PIN_LEFT_IN1,  LOW);
  digitalWrite(PIN_LEFT_IN2,  LOW);
  digitalWrite(PIN_RIGHT_IN1, LOW);
  digitalWrite(PIN_RIGHT_IN2, LOW);
}
