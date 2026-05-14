#include "Sensors.h"
#include "Config.h"
#include <Wire.h>
#include <VL53L0X.h>

// =============================================================================
//  Sensors.cpp — การอ่านเซ็นเซอร์ VL53L0X (วัดระยะ) และตรวจเส้น
// =============================================================================

namespace {

  // อ็อบเจกต์เซ็นเซอร์วัดระยะ ทั้ง 5 ตัว
  VL53L0X sensor_SL, sensor_FL, sensor_FC, sensor_FR, sensor_SR;

  // -------------------------------------------------------------------------
  // กระพริบ LED แสดงรหัสเซ็นเซอร์ที่ค้าง (เรียกเมื่อ init ล้มเหลว)
  // รูปแบบ: กระพริบ id ครั้ง → หยุด 1.2 วินาที → ซ้ำ
  //   id=1 = SL, 2 = FL, 3 = FC, 4 = FR, 5 = SR
  // -------------------------------------------------------------------------
  void blinkErrorForever(uint8_t sensor_id) {
    pinMode(LED_BUILTIN, OUTPUT);
    while (true) {
      for (uint8_t i = 0; i < sensor_id; i++) {
        digitalWrite(LED_BUILTIN, HIGH); delay(150);
        digitalWrite(LED_BUILTIN, LOW);  delay(200);
      }
      delay(1200);  // หยุดระหว่างรอบ ให้อ่านรหัสทัน
    }
  }

  // -------------------------------------------------------------------------
  // ปลุกเซ็นเซอร์หนึ่งตัวแล้วตั้ง I2C address ใหม่
  // ต้องทำทีละตัว เพราะทุกตัวเริ่มต้นที่ address เดียวกัน (0x29)
  // -------------------------------------------------------------------------
  void bringUpSensor(VL53L0X& sensor, uint8_t xshut_pin, uint8_t new_i2c_addr,
                     uint8_t sensor_id, const __FlashStringHelper* sensor_name) {

#if VL53L0X_DEBUG
    Serial.print(F("[ToF] ปลุก ")); Serial.println(sensor_name);
#endif

    // ปล่อยขา XSHUT → เซ็นเซอร์ตื่นผ่าน internal pull-up
    pinMode(xshut_pin, INPUT);
    delay(10);

    sensor.setTimeout(50);
    if (!sensor.init()) {
      // init ล้มเหลว — กระพริบ LED แล้วหยุดตลอดกาล
#if VL53L0X_DEBUG
      Serial.print(F("[ToF] ERROR ค้างที่ ")); Serial.println(sensor_name);
      Serial.flush();
#endif
      blinkErrorForever(sensor_id);
    }

    // เปลี่ยน I2C address ก่อนปลุกตัวถัดไป
    sensor.setAddress(new_i2c_addr);

    // ตั้ง timing budget และเริ่มวัดต่อเนื่อง (continuous mode)
    sensor.setMeasurementTimingBudget(TOF_TIMING_BUDGET_US);
    sensor.startContinuous(0);  // 0 = วัดถี่สุดเท่าที่ทำได้

#if VL53L0X_DEBUG
    Serial.print(F("[ToF] OK ")); Serial.println(sensor_name);
#endif
  }

  // -------------------------------------------------------------------------
  // อ่านระยะจากเซ็นเซอร์หนึ่งตัว
  // ถ้า timeout หรือค่าผิดปกติ → คืน TOF_NO_TARGET
  // -------------------------------------------------------------------------
  uint16_t readDistance(VL53L0X& sensor) {
    uint16_t distance_mm = sensor.readRangeContinuousMillimeters();

    bool is_invalid = sensor.timeoutOccurred()
                   || distance_mm == 65535          // ค่า error ของ library
                   || distance_mm > TOF_NO_TARGET;

    return is_invalid ? TOF_NO_TARGET : distance_mm;
  }

}  // namespace (private)

// =============================================================================

void Sensors::initToF() {
#if VL53L0X_DEBUG
  if (!Serial) Serial.begin(115200);
  Serial.println(F("[ToF] เริ่ม initToF()"));
#endif

  // กด XSHUT ทุกตัวลง LOW → reset พร้อมกันก่อน
  uint8_t xshut_pins[] = {
    PIN_XSHUT_SL, PIN_XSHUT_FL, PIN_XSHUT_FC, PIN_XSHUT_FR, PIN_XSHUT_SR
  };
  for (uint8_t pin : xshut_pins) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
  delay(10);

  // ปลุกทีละตัว → ตั้ง address → ปลุกตัวถัดไป
  bringUpSensor(sensor_SL, PIN_XSHUT_SL, TOF_ADDR_SL, 1, F("SL"));
  bringUpSensor(sensor_FL, PIN_XSHUT_FL, TOF_ADDR_FL, 2, F("FL"));
  bringUpSensor(sensor_FC, PIN_XSHUT_FC, TOF_ADDR_FC, 3, F("FC"));
  bringUpSensor(sensor_FR, PIN_XSHUT_FR, TOF_ADDR_FR, 4, F("FR"));
  bringUpSensor(sensor_SR, PIN_XSHUT_SR, TOF_ADDR_SR, 5, F("SR"));

#if VL53L0X_DEBUG
  Serial.println(F("[ToF] พร้อมทุกตัว"));
#endif
}

ToFReadings Sensors::readToF() {
  ToFReadings readings;
  readings.sl = readDistance(sensor_SL);
  readings.fl = readDistance(sensor_FL);
  readings.fc = readDistance(sensor_FC);
  readings.fr = readDistance(sensor_FR);
  readings.sr = readDistance(sensor_SR);
  return readings;
}

LineReadings Sensors::readLine() {
  LineReadings readings;
  readings.left_raw    = analogRead(PIN_LINE_L);
  readings.right_raw   = analogRead(PIN_LINE_R);
  readings.left_white  = readings.left_raw  > LINE_L_THRESHOLD;
  readings.right_white = readings.right_raw > LINE_R_THRESHOLD;
  return readings;
}
