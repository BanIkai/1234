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

enum EscapeState {
  ESC_IDLE = 0,   // ปกติ ไม่ได้หนีเส้น
  ESC_BACK = 1,   // กำลังถอยหลังหนีเส้น
  ESC_TURN = 2,   // กำลังหมุนกลับเข้าสนาม
};

enum TargetDir {
  DIR_NONE   =  0,   // ไม่เห็นศัตรูเลย
  DIR_CENTER =  2,   // ศัตรูอยู่ตรงหน้าพอดี → วิ่งตรง
  DIR_LEFT   = -1,   // ศัตรูอยู่ทางซ้าย
  DIR_RIGHT  =  1,   // ศัตรูอยู่ทางขวา
};


// ── ตัวแปร "จำ state" (ใช้ภายใน file นี้เท่านั้น) ────────────
namespace {

  // -- จำ state การหนีเส้น --
  EscapeState   escapeState = ESC_IDLE;
  bool          escapeTurnRight = false;  // [FIX #2] ชื่อชัดเจน: true = หมุนขวา
  unsigned long escapeTimer = 0;

  // -- จำ target ล่าสุด --
  TargetDir     lastTarget  = DIR_NONE;
  unsigned long targetTimer = 0;

  // -- จำ state การค้นหา --
  bool          searchDir   = false;
  unsigned long searchTimer = 0;


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
  //
  //  [FIX #1] เมื่อก่อนคืน DIR_NONE เวลาศัตรูตรงหน้าพอดี
  //  ทำให้ AI ไม่อัปเดต lock → หุ่นหยุดพุ่งแล้วหันไปค้นหาแทน
  //  แก้โดยเพิ่ม DIR_CENTER แทน DIR_NONE ในกรณีนี้
  TargetDir detectTarget(const Dist& d) {
    if (d.fc < TRACK_DIST) {
      if (abs(d.fl - d.fr) < TRACK_CENTER_ZONE) return DIR_CENTER;  // ตรงหน้าพอดี
      return (d.fl < d.fr) ? DIR_LEFT : DIR_RIGHT;
    }
    if (d.fl < TRACK_DIST) return DIR_LEFT;
    if (d.fr < TRACK_DIST) return DIR_RIGHT;
    if (d.sl < SIDE_DIST)  return DIR_LEFT;
    if (d.sr < SIDE_DIST)  return DIR_RIGHT;
    return DIR_NONE;
  }

  // คืนระยะที่ใกล้ที่สุดจาก sensor หน้า 3 ตัว
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
  //      ถ้า diff บวก = fl น้อยกว่า fr → sensor ซ้ายเห็นใกล้กว่า → ศัตรูอยู่ซ้าย → เลี้ยวซ้าย
  //      ถ้า diff ลบ  = fr น้อยกว่า fl → sensor ขวาเห็นใกล้กว่า → ศัตรูอยู่ขวา  → เลี้ยวขวา
  //      ยิ่ง diff มาก = เลี้ยวแรงขึ้น
  //
  //  กรณี 2: sensor ข้างเจอ (fc ไม่เห็น) → ใช้ความเร็วตายตัว
  //
  void doTrack(TargetDir dir, const Dist& d) {

    if (d.fc < TRACK_DIST) {
      // === Proportional Steering ===
      // diff บวก = fl น้อย = sensor ซ้ายใกล้ศัตรูกว่า = ศัตรูอยู่ซ้าย
      // diff ลบ  = fr น้อย = sensor ขวาใกล้ศัตรูกว่า = ศัตรูอยู่ขวา
      int diff = d.fl - d.fr;

      if (abs(diff) < TRACK_CENTER_ZONE) {
        Motors::move(TRACK_FAST, TRACK_FAST);
        return;
      }

      int turn = map(abs(diff), TRACK_CENTER_ZONE, TRACK_DIST, 20, 90);
      turn = constrain(turn, 20, 90);

      int fast = TRACK_FAST;
      int slow = constrain(TRACK_FAST - turn, 0, 255);

      if (diff > 0)
        Motors::move(slow, fast);   // ศัตรูอยู่ซ้าย → ล้อซ้ายช้า = เลี้ยวซ้าย  [FIX #5]
      else
        Motors::move(fast, slow);   // ศัตรูอยู่ขวา  → ล้อขวาช้า = เลี้ยวขวา   [FIX #5]

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
  void doSearch() {
    const unsigned long CYCLE = SEARCH_FWD_MS + SEARCH_SWAP_MS;

    unsigned long elapsed = millis() - searchTimer;

    if (elapsed >= CYCLE) {
      searchDir   = !searchDir;
      searchTimer = millis();
      elapsed     = 0;
    }

    if (elapsed < SEARCH_FWD_MS) {
      Motors::move(SEARCH_SPEED, SEARCH_SPEED);
    } else {
      if (searchDir)
        Motors::move(-SEARCH_SPEED, SEARCH_SPEED);   // หมุนขวา
      else
        Motors::move(SEARCH_SPEED, -SEARCH_SPEED);   // หมุนซ้าย
    }
  }

} // ปิด namespace (anonymous)


// ================================================================
//  Public API
// ================================================================

void AI::reset() {
  escapeState    = ESC_IDLE;
  escapeTurnRight = false;
  escapeTimer    = 0;
  lastTarget     = DIR_NONE;
  targetTimer    = 0;
  searchDir      = false;
  searchTimer    = millis();  // [FIX #3] ใช้ millis() ไม่ใช่ 0
                              // ป้องกัน elapsed พุ่งสูงหลัง delay(5000)
                              // ทำให้รอบแรกเริ่มด้วย "เดินหน้า" เสมอ
}


void AI::run(Dist dist, Line line) {

  // ============================================================
  //  ขั้น 1: ตรวจเส้น (สำคัญที่สุด ตรวจก่อนเสมอ)
  // ============================================================
  //
  //  เช็ค ESC_IDLE ก่อน เพราะถ้ากำลังถอยอยู่ sensor อาจยังเห็นเส้น
  //  ถ้าไม่เช็ค → escapeTimer reset ทุก loop → ถอยไม่จบสักที
  //
  if ((line.left || line.right) && escapeState == ESC_IDLE) {
    escapeState = ESC_BACK;
    escapeTimer = millis();

    // ตัดสินใจทิศหมุนกลับ
    // [FIX #2] ชื่อตัวแปรชัดเจนขึ้น: escapeTurnRight = true = จะหมุนขวา
    //
    //  เจอเส้นซ้าย  → หุ่นใกล้ขอบซ้าย → หมุนขวาเพื่อกลับเข้าสนาม
    //  เจอเส้นขวา   → หุ่นใกล้ขอบขวา → หมุนซ้าย
    //  เจอทั้งสอง   → random ป้องกันติดแบบเดิม
    if (line.left && !line.right)
      escapeTurnRight = true;
    else if (line.right && !line.left)
      escapeTurnRight = false;
    else
      escapeTurnRight = (random(2) == 0);
  }

  // ============================================================
  //  ขั้น 2: ถอยหลังหนีเส้น
  // ============================================================
  if (escapeState == ESC_BACK) {
    Motors::move(-ESCAPE_BACK, -ESCAPE_BACK);

    if (millis() - escapeTimer >= ESCAPE_BACK_MS) {
      escapeState = ESC_TURN;
      escapeTimer = millis();
    }
    return;
  }

  // ============================================================
  //  ขั้น 3: หมุนกลับเข้าสนาม
  // ============================================================
  if (escapeState == ESC_TURN) {
    if (escapeTurnRight)
      Motors::move( ESCAPE_TURN, -ESCAPE_TURN);   // หมุนขวา
    else
      Motors::move(-ESCAPE_TURN,  ESCAPE_TURN);   // หมุนซ้าย

    if (millis() - escapeTimer >= ESCAPE_TURN_MS) {
      escapeState = ESC_IDLE;
      searchTimer = millis();  // [FIX #4] reset searchTimer ทันทีที่กลับเป็น IDLE
                               // ป้องกัน elapsed ค้างค่าเก่า → phase ค้นหาผิด
      return;                  // [FIX #6] return ทันที ไม่ให้ไหลเข้า AI ปกติในรอบเดียวกัน
    }
    return;
  }

  // ============================================================
  //  ขั้น 4: AI ปกติ — ไม่ได้หนีเส้น
  // ============================================================

  TargetDir detected = detectTarget(dist);

  // [FIX #1] DIR_CENTER ก็นับว่า "เห็นศัตรู" → อัปเดต lock ด้วย
  if (detected != DIR_NONE) {
    lastTarget  = detected;
    targetTimer = millis();
  }

  bool targetLocked = (lastTarget != DIR_NONE) &&
                      (millis() - targetTimer < LOCK_TIME);

  if (!targetLocked) {
    doSearch();
    return;
  }

  // มีเป้าหมาย → ตัดสินใจว่าจะพุ่งหรือตาม
  if (closestFront(dist) < RAM_DIST)
    doAttack();
  else
    doTrack(lastTarget, dist);
}
