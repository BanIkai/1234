#include <Arduino.h>
#include "Strategy.h"
#include "Motors.h"
#include "Config.h"

// ============================================================
//  Strategy.cpp — AI หุ่นยนต์ซูโม่
//
//  ภาพรวมการทำงาน:
//
//    loop() เรียก AI::run() ทุกรอบ
//         │
//         ▼
//    เจอเส้น? ──Yes──► [ESCAPE_BACK] ถอยหลัง 180ms
//         │                    │
//         No                   ▼
//         │             [ESCAPE_TURN] หมุนกลับ 220ms
//         ▼                    │
//    เห็นศัตรู? ──No──► [SEARCH] หมุนหา (spiral)
//         │
//         Yes
//         │
//    ศัตรูใกล้? ──Yes──► [ATTACK] พุ่งชน
//         │
//         No
//         │
//         └────────────► [TRACK] เลี้ยวตาม
// ============================================================


// ── ชนิดข้อมูล (Enum) ────────────────────────────────────────
//
//  แทนที่จะใช้ตัวเลข 0, 1, 2 ตรงๆ
//  เราใช้ชื่อที่อ่านเข้าใจ เช่น ESC_IDLE, ESC_BACK, ESC_TURN

enum EscapeState {
  ESC_IDLE = 0,   // ปกติ ไม่ได้หนีเส้น
  ESC_BACK = 1,   // กำลังถอยหลังหนีเส้น
  ESC_TURN = 2,   // กำลังหมุนกลับเข้าสนาม
};

enum TargetDir {
  DIR_NONE  =  0,   // ไม่เห็นศัตรู
  DIR_LEFT  = -1,   // ศัตรูอยู่ทางซ้าย
  DIR_RIGHT =  1,   // ศัตรูอยู่ทางขวา
};


// ── ตัวแปร "จำ state" (ใช้ภายใน file นี้เท่านั้น) ────────────
namespace {

  // -- จำ state การหนีเส้น --
  EscapeState   escapeState = ESC_IDLE;
  bool          escapeLeft  = false;    // เจอเส้นซ้าย? (ใช้ตอนตัดสินใจหมุน)
  unsigned long escapeTimer = 0;        // millis() ตอนที่เริ่ม escape

  // -- จำ target ล่าสุด --
  TargetDir     lastTarget  = DIR_NONE;
  unsigned long targetTimer = 0;        // millis() ที่เห็น target ครั้งล่าสุด

  // -- จำ state การค้นหา --
  bool          searchDir   = false;    // ทิศค้นหาปัจจุบัน (false=ซ้าย, true=ขวา)
  unsigned long searchTimer = 0;        // millis() ที่เริ่มรอบค้นหานี้


  // ================================================================
  //  ฟังก์ชันช่วย (Helper) — private ใช้ภายใน file นี้เท่านั้น
  // ================================================================

  // ตรวจว่า sensor หน้าตัวใดตัวหนึ่งเห็นศัตรูในระยะ TRACK_DIST ไหม
  bool frontSeesTarget(const Dist& d) {
    return (d.fl < TRACK_DIST) ||
           (d.fc < TRACK_DIST) ||
           (d.fr < TRACK_DIST);
  }

  // วิเคราะห์ข้อมูล sensor ทั้งหมด แล้วบอกว่าศัตรูอยู่ทางไหน
  //
  //  ลำดับตรวจ: หน้ากลาง → หน้าซ้าย → หน้าขวา → ข้างซ้าย → ข้างขวา
  //  (หน้ากลางสำคัญสุด เพราะบอกแนวพุ่งได้ตรงที่สุด)
  TargetDir detectTarget(const Dist& d) {
    if (d.fc < TRACK_DIST) {
      // เห็นหน้ากลาง → เปรียบ fl กับ fr ว่าศัตรูเอียงไปทางไหน
      return (d.fl <= d.fr) ? DIR_LEFT : DIR_RIGHT;
    }
    if (d.fl < TRACK_DIST) return DIR_LEFT;
    if (d.fr < TRACK_DIST) return DIR_RIGHT;
    if (d.sl < SIDE_DIST)  return DIR_LEFT;
    if (d.sr < SIDE_DIST)  return DIR_RIGHT;
    return DIR_NONE;
  }

  // คืนระยะที่ใกล้ที่สุดจาก sensor หน้า 3 ตัว
  // ใช้ตัดสินใจว่า "ใกล้พอจะพุ่งหรือยัง"
  int closestFront(const Dist& d) {
    return min(min(d.fl, d.fc), d.fr);
  }


  // ================================================================
  //  พฤติกรรมหลัก 3 แบบ
  // ================================================================

  // ATTACK — เห็นศัตรูอยู่ใกล้ → พุ่งเต็มสปีดทั้งสองล้อ
  void doAttack() {
    Motors::move(ATTACK_SPEED, ATTACK_SPEED);
  }


  // TRACK — เห็นศัตรูแต่ยังไกล → เลี้ยวตามแบบ proportional
  //
  //  กรณี 1: sensor หน้ากลาง (fc) เห็น → คำนวณจาก fl และ fr
  //    diff = fl - fr
  //      ถ้า diff บวก = ศัตรูอยู่ขวา → ลดความเร็วล้อขวา
  //      ถ้า diff ลบ  = ศัตรูอยู่ซ้าย → ลดความเร็วล้อซ้าย
  //      ยิ่ง diff มาก = เลี้ยวแรงขึ้น
  //
  //  กรณี 2: sensor ข้างเจอ (fc ไม่เห็น) → ใช้ความเร็วตายตัว
  //
  void doTrack(TargetDir dir, const Dist& d) {

    if (d.fc < TRACK_DIST) {
      // === Proportional Steering ===
      int diff = d.fl - d.fr;   // บวก = ศัตรูเอียงขวา, ลบ = เอียงซ้าย

      if (abs(diff) < TRACK_CENTER_ZONE) {
        // ศัตรูอยู่ตรงหน้าพอดีในโซน → วิ่งตรงไปเลย
        Motors::move(TRACK_FAST, TRACK_FAST);
        return;
      }

      // คำนวณว่าต้องเลี้ยวแค่ไหน (ยิ่ง diff มาก = เลี้ยวแรงขึ้น)
      // map() แปลงค่าจากช่วงหนึ่งไปอีกช่วง เช่น diff 40→280 กลายเป็น turn 20→90
      int turn = map(abs(diff), TRACK_CENTER_ZONE, TRACK_DIST, 20, 90);
      turn = constrain(turn, 20, 90);  // ป้องกันค่าเกินขอบเขต

      int fast = TRACK_FAST;
      int slow = constrain(TRACK_FAST - turn, 0, 255);  // ล้อที่ "ช้าลง"

      if (diff > 0)
        Motors::move(fast, slow);   // ศัตรูอยู่ขวา → ล้อขวาช้า = เลี้ยวขวา
      else
        Motors::move(slow, fast);   // ศัตรูอยู่ซ้าย → ล้อซ้ายช้า = เลี้ยวซ้าย

    } else {
      // === Fixed Speed (sensor ข้างเจอ) ===
      if (dir == DIR_LEFT)
        Motors::move(TRACK_SLOW, TRACK_FAST);  // ล้อซ้ายช้า = เลี้ยวซ้าย
      else
        Motors::move(TRACK_FAST, TRACK_SLOW);  // ล้อขวาช้า = เลี้ยวขวา
    }
  }


  // SEARCH — ไม่เห็นศัตรู → หมุนค้นหาแบบ spiral
  //
  //  แต่ละรอบ (= SEARCH_FWD_MS + SEARCH_SWAP_MS = 800ms):
  //    ช่วงต้น (200ms): เดินหน้า
  //    ช่วงหลัง (600ms): หมุนค้นหา
  //    พอครบรอบ: สลับทิศ แล้วเริ่มรอบใหม่
  //
  //  ทำไมต้องเดินหน้าก่อน? เพื่อให้ครอบคลุมพื้นที่สนามมากขึ้น
  //  แทนที่จะหมุนอยู่กับที่ตลอด
  //
  void doSearch() {
    const unsigned long CYCLE = SEARCH_FWD_MS + SEARCH_SWAP_MS;  // 800ms ต่อรอบ

    unsigned long elapsed = millis() - searchTimer;  // ผ่านไปกี่ ms แล้ว

    // ครบ 1 รอบ → สลับทิศ แล้วเริ่มนับใหม่
    if (elapsed >= CYCLE) {
      searchDir   = !searchDir;  // ! คือ "กลับค่า" false→true หรือ true→false
      searchTimer = millis();
      elapsed     = 0;
    }

    if (elapsed < SEARCH_FWD_MS) {
      // ช่วงแรกของรอบ: เดินหน้า
      Motors::move(SEARCH_SPEED, SEARCH_SPEED);
    } else {
      // ช่วงหลัง: หมุน
      if (searchDir)
        Motors::move(-SEARCH_SPEED, SEARCH_SPEED);   // หมุนขวา (ล้อซ้ายถอย ล้อขวาหน้า)
      else
        Motors::move(SEARCH_SPEED, -SEARCH_SPEED);   // หมุนซ้าย
    }
  }

} // ปิด namespace (anonymous)


// ================================================================
//  Public API — ฟังก์ชันที่ไฟล์อื่นเรียกได้
// ================================================================

// รีเซ็ตตัวแปรทั้งหมดให้เริ่มต้นใหม่
void AI::reset() {
  escapeState = ESC_IDLE;
  escapeLeft  = false;
  escapeTimer = 0;
  lastTarget  = DIR_NONE;
  targetTimer = 0;
  searchDir   = false;
  searchTimer = 0;
}


// AI หลัก — เรียกทุก loop()
void AI::run(Dist dist, Line line) {

  // ============================================================
  //  ขั้น 1: ตรวจเส้น (สำคัญที่สุด ตรวจก่อนเสมอ)
  // ============================================================
  //
  //  ทำไมต้องเช็ค escapeState == ESC_IDLE ด้วย?
  //  เพราะถ้าหุ่นกำลังถอยอยู่ sensor เส้นอาจยังเห็นเส้นอยู่
  //  ถ้าไม่เช็ค → escapeTimer จะ reset ทุก loop → ถอยไม่จบสักที
  //
  if ((line.left || line.right) && escapeState == ESC_IDLE) {
    escapeState = ESC_BACK;
    escapeLeft  = line.left;   // จำว่าเจอเส้นซ้าย (ใช้ตอนตัดสินใจหมุน)
    escapeTimer = millis();    // เริ่มจับเวลา
  }

  // ============================================================
  //  ขั้น 2: ถอยหลังหนีเส้น
  // ============================================================
  if (escapeState == ESC_BACK) {
    Motors::move(-ESCAPE_BACK, -ESCAPE_BACK);   // ค่าลบ = ถอยหลัง

    // ถอยครบเวลา ESCAPE_BACK_MS แล้ว → เข้าสู่ขั้นหมุนกลับ
    if (millis() - escapeTimer >= ESCAPE_BACK_MS) {
      escapeState = ESC_TURN;
      escapeTimer = millis();   // รีเซ็ต timer สำหรับรอบถัดไป
    }
    return;   // return = หยุดทำส่วนที่เหลือของ loop นี้
  }

  // ============================================================
  //  ขั้น 3: หมุนกลับเข้าสนาม
  // ============================================================
  //
  //  เจอเส้นซ้าย → แปลว่าหุ่นหันหน้าไปซ้าย → หมุนขวาเพื่อกลับ
  //  เจอเส้นขวา → หมุนซ้าย
  //
  if (escapeState == ESC_TURN) {
    if (escapeLeft)
      Motors::move( ESCAPE_TURN, -ESCAPE_TURN);   // หมุนขวา
    else
      Motors::move(-ESCAPE_TURN,  ESCAPE_TURN);   // หมุนซ้าย

    if (millis() - escapeTimer >= ESCAPE_TURN_MS) {
      escapeState = ESC_IDLE;   // หมุนครบแล้ว → กลับสู่ปกติ
    }
    return;
  }

  // ============================================================
  //  ขั้น 4: AI ปกติ — ไม่ได้หนีเส้น
  // ============================================================

  // ตรวจว่า sensor เห็นศัตรูทิศไหน
  TargetDir detected = detectTarget(dist);

  if (detected != DIR_NONE) {
    // เห็นศัตรู → อัปเดต lock ใหม่
    lastTarget  = detected;
    targetTimer = millis();
  }

  // "targetLocked" = true ถ้าเพิ่งเห็นศัตรูใน LOCK_TIME ms ที่ผ่านมา
  // ทำให้หุ่นยังพุ่งหาศัตรูต่อได้ แม้มองไม่เห็นชั่วคราว
  bool targetLocked = (lastTarget != DIR_NONE) &&
                      (millis() - targetTimer < LOCK_TIME);

  if (!targetLocked) {
    // ไม่มีข้อมูลศัตรู → ออกค้นหา
    doSearch();
    return;
  }

  // มีเป้าหมาย → ตัดสินใจว่าจะพุ่งหรือตาม
  if (closestFront(dist) < RAM_DIST)
    doAttack();              // ใกล้พอแล้ว → พุ่งชน!
  else
    doTrack(lastTarget, dist);  // ยังไกล → เลี้ยวตาม
}
