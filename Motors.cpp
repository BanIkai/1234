#include "Motors.h"
#include "Config.h"
#include <Arduino.h>

// ============================================================
//  Motors.cpp — โค้ดควบคุมมอเตอร์ผ่าน H-Bridge
//
//  H-Bridge คือวงจรที่ทำให้มอเตอร์หมุนได้ทั้ง 2 ทิศ
//  โดยใช้ 2 ขาต่อมอเตอร์ 1 ตัว ดังนี้:
//
//    speed > 0  →  PIN_x1 = PWM (speed),  PIN_x2 = LOW   (เดินหน้า)
//    speed < 0  →  PIN_x1 = LOW,          PIN_x2 = PWM   (ถอยหลัง)
//    speed = 0  →  PIN_x1 = LOW,          PIN_x2 = LOW   (หยุด)
//
//  PWM คือสัญญาณ 0-255 ที่บอก "ความเร็ว" ของมอเตอร์
// ============================================================


// ── ฟังก์ชัน private (ใช้ภายใน file นี้เท่านั้น) ────────────
namespace {

  // ขับมอเตอร์ 1 ตัว
  // pin1 = ขาเดินหน้า, pin2 = ขาถอยหลัง, speed = ความเร็ว (-255 ถึง 255)
  void driveMotor(int pin1, int pin2, int speed) {

    // ป้องกันค่าเกินขอบเขต (เผื่อโค้ดส่งค่า 300 มาโดยบังเอิญ)
    speed = constrain(speed, -255, 255);

    if (speed > 0) {
      // เดินหน้า: ปล่อย PWM ที่ pin1, ดับ pin2
      analogWrite(pin1, speed);
      digitalWrite(pin2, LOW);
    }
    else if (speed < 0) {
      // ถอยหลัง: ดับ pin1, ปล่อย PWM ที่ pin2 (speed เป็นลบ จึงต้อง -speed)
      digitalWrite(pin1, LOW);
      analogWrite(pin2, -speed);
    }
    else {
      // หยุด: ดับทั้งสองขา
      digitalWrite(pin1, LOW);
      digitalWrite(pin2, LOW);
    }
  }

} // ปิด namespace (anonymous)


// ── Public API ───────────────────────────────────────────────

// เตรียมขาทั้งหมดให้เป็น OUTPUT
void Motors::begin() {
  pinMode(PIN_L1, OUTPUT);
  pinMode(PIN_L2, OUTPUT);
  pinMode(PIN_R1, OUTPUT);
  pinMode(PIN_R2, OUTPUT);
}

// สั่งมอเตอร์ซ้ายและขวาพร้อมกัน
void Motors::move(int leftSpeed, int rightSpeed) {
  driveMotor(PIN_L1, PIN_L2, leftSpeed);
  driveMotor(PIN_R1, PIN_R2, rightSpeed);
}

// หยุดมอเตอร์ทั้งคู่
void Motors::stop() {
  move(0, 0);
}
