#include <Arduino.h>
#include "Strategy.h"
#include "Motors.h"
#include "Config.h"

// ============================================================
//  Strategy.cpp — กลยุทธ์หลักของหุ่นยนต์ซูโม่
//
//  State Machine มี 4 สถานะ:
//
//    ESCAPE_BACK  ← เจอเส้น → ถอยหลัง ESCAPE_BACK_MS
//         ↓
//    ESCAPE_TURN  ← หมุนกลับเข้าสนาม ESCAPE_TURN_MS
//         ↓
//    ATTACK / TRACK / SEARCH  ← AI ปกติ
//
//  ภายใน AI ปกติ (ไม่ได้หนีเส้น):
//    - เจอศัตรูใกล้  → ATTACK (พุ่งเต็มสปีด)
//    - เจอศัตรูไกล  → TRACK  (proportional: เลี้ยวตาม diff ของ fl/fr)
//    - ไม่เจอเลย    → SEARCH (spiral: เดินหน้า → หมุน → สลับทิศ)
// ============================================================


// ── enum แทน magic number ────────────────────────────────────
enum EscapeState {
  ESC_IDLE = 0,   // ไม่ได้หนีเส้น
  ESC_BACK = 1,   // กำลังถอยหลัง
  ESC_TURN = 2,   // กำลังหมุนกลับ
};

enum TargetDir {
  DIR_NONE  = 0,
  DIR_LEFT  = -1,
  DIR_RIGHT =  1,
};


// ── state ภายใน (private ต่อ file นี้) ──────────────────────
namespace {

  EscapeState escapeState = ESC_IDLE;
  bool        escapeLeft  = false;          // เจอเส้นซ้าย? (ใช้ตอนหมุนกลับ)
  unsigned long escapeTimer = 0;

  TargetDir     lastTarget   = DIR_NONE;    // ทิศที่เจอศัตรูครั้งล่าสุด
  unsigned long targetTimer  = 0;           // millis() ที่ lock ได้

  bool          searchDir    = false;       // false=ซ้าย, true=ขวา
  unsigned long searchTimer  = 0;           // ใช้สลับทิศค้นหา


  // ── helper: เช็คว่า sensor ด้านหน้าเห็นศัตรูไหม ──────────
  bool frontSeesTarget(const Dist& d) {
    return (d.fl < TRACK_DIST) ||
           (d.fc < TRACK_DIST) ||
           (d.fr < TRACK_DIST);
  }

  // ── helper: ตัดสินใจทิศจาก dist (คืน DIR_LEFT/RIGHT/NONE) ─
  TargetDir detectTarget(const Dist& d) {
    // ตรวจหน้ากลางก่อน (priority สูงสุด)
    if (d.fc < TRACK_DIST) {
      if      (d.fl <= d.fr) return DIR_LEFT;
      else                   return DIR_RIGHT;
    }
    if (d.fl < TRACK_DIST) return DIR_LEFT;
    if (d.fr < TRACK_DIST) return DIR_RIGHT;

    // ตรวจ sensor ข้าง
    if (d.sl < SIDE_DIST)  return DIR_LEFT;
    if (d.sr < SIDE_DIST)  return DIR_RIGHT;

    return DIR_NONE;
  }

  // ── helper: ระยะที่ใกล้ที่สุดในฝั่งที่ lock ──────────────
  int closestFront(const Dist& d) {
    return min(min(d.fl, d.fc), d.fr);
  }


  // ── พฤติกรรมหลัก 3 แบบ ───────────────────────────────────

  void doAttack() {
    Motors::move(ATTACK_SPEED, ATTACK_SPEED);
  }

  // ── TRACK แบบ proportional ───────────────────────────────
  //  แทนที่จะใช้ความเร็วตายตัว TRACK_FAST/TRACK_SLOW
  //  คำนวณจากความต่างของ fl กับ fr
  //
  //  ตัวอย่าง:
  //    fl=100, fr=300 → diff=200 → เลี้ยวซ้ายมาก
  //    fl=200, fr=220 → diff=20  → ศัตรูเกือบตรงหน้า → วิ่งตรง
  //
  void doTrack(TargetDir dir, const Dist& d) {
    // ถ้า fc เห็นศัตรู ใช้ fl กับ fr เทียบกัน
    // ถ้า fc ไม่เห็น (sensor ข้าง) ใช้ความเร็วตายตัวเหมือนเดิม
    if (d.fc < TRACK_DIST) {
      int diff = d.fl - d.fr;   // บวก = ศัตรูอยู่ขวา, ลบ = ซ้าย

      if (abs(diff) < TRACK_CENTER_ZONE) {
        // ศัตรูอยู่ตรงหน้าพอ → วิ่งตรงเลย
        Motors::move(TRACK_FAST, TRACK_FAST);
        return;
      }

      // scale diff ให้เป็นปริมาณเลี้ยว (0–90)
      // diff สูงสุดประมาณ TRACK_DIST = 350 → normalize
      int turn = map(abs(diff), TRACK_CENTER_ZONE, TRACK_DIST, 20, 90);
      turn = constrain(turn, 20, 90);

      int fast = TRACK_FAST;
      int slow = constrain(TRACK_FAST - turn, 0, 255);   // ลดล้อฝั่งตรงข้ามตาม diff

      if (diff > 0)   // ศัตรูอยู่ขวา → ล้อขวาช้า
        Motors::move(fast, slow);
      else            // ศัตรูอยู่ซ้าย → ล้อซ้ายช้า
        Motors::move(slow, fast);

    } else {
      // sensor ข้างเจอ → เลี้ยวตายตัวเหมือนเดิม
      if (dir == DIR_LEFT)
        Motors::move(TRACK_SLOW, TRACK_FAST);
      else
        Motors::move(TRACK_FAST, TRACK_SLOW);
    }
  }


  // ── SEARCH แบบ spiral ────────────────────────────────────
  //  เดิม: หมุนอยู่กับที่ สลับซ้าย-ขวา
  //  ใหม่: เดินหน้าสั้นๆ (SEARCH_FWD_MS) แล้วค่อยหมุน
  //         ทำให้ครอบคลุมพื้นที่สนามได้มากขึ้น
  //
  //  pattern:
  //    [เดินหน้า 200ms] → [หมุน 600ms] → [เดินหน้า 200ms] → ...
  //
  void doSearch() {
    unsigned long elapsed = millis() - searchTimer;
    unsigned long cycle   = SEARCH_FWD_MS + SEARCH_SWAP_MS;  // 800ms ต่อรอบ

    if (elapsed >= cycle) {
      // ครบ 1 รอบ → สลับทิศแล้วเริ่มใหม่
      searchDir   = !searchDir;
      searchTimer = millis();
      elapsed     = 0;
    }

    if (elapsed < SEARCH_FWD_MS) {
      // ช่วงแรกของรอบ: เดินหน้า
      Motors::move(SEARCH_SPEED, SEARCH_SPEED);
    } else {
      // ช่วงหลัง: หมุนค้นหา
      if (searchDir)
        Motors::move(-SEARCH_SPEED, SEARCH_SPEED);   // หมุนขวา
      else
        Motors::move(SEARCH_SPEED, -SEARCH_SPEED);   // หมุนซ้าย
    }
  }

} // namespace (anonymous)


// ── public API ───────────────────────────────────────────────

void AI::reset() {
  escapeState = ESC_IDLE;
  escapeLeft  = false;
  escapeTimer = 0;
  lastTarget  = DIR_NONE;
  targetTimer = 0;
  searchDir   = false;
  searchTimer = 0;
}


void AI::run(Dist dist, Line line) {

  // ===========================================================
  //  STEP 1 — ตรวจเส้น (ความสำคัญสูงสุด)
  //           ถ้าเจอเส้นตอนไหนก็ได้ → เริ่ม escape ทันที
  // ===========================================================
  // แก้: เริ่ม escape ใหม่ได้เฉพาะตอน IDLE เท่านั้น
  // ถ้าไม่ check escapeState == IDLE → escapeTimer จะ reset ซ้ำทุก loop
  // ตราบใดที่ sensor ยังเห็นเส้น → หนีไม่สำเร็จ
  if ((line.left || line.right) && escapeState == ESC_IDLE) {
    escapeState = ESC_BACK;
    escapeLeft  = line.left;     // จำว่าเจอซ้ายหรือขวา
    escapeTimer = millis();
  }

  // ===========================================================
  //  STEP 2 — ถอยหลังหนีเส้น
  // ===========================================================
  if (escapeState == ESC_BACK) {
    Motors::move(-ESCAPE_BACK, -ESCAPE_BACK);

    if (millis() - escapeTimer >= ESCAPE_BACK_MS) {
      escapeState = ESC_TURN;
      escapeTimer = millis();
    }
    return;   // ไม่ทำอย่างอื่นระหว่างถอย
  }

  // ===========================================================
  //  STEP 3 — หมุนกลับเข้าสนาม
  //           เจอเส้นซ้าย → หมุนขวา (และกลับกัน)
  // ===========================================================
  if (escapeState == ESC_TURN) {
    if (escapeLeft)
      Motors::move( ESCAPE_TURN, -ESCAPE_TURN);   // หมุนขวา
    else
      Motors::move(-ESCAPE_TURN,  ESCAPE_TURN);   // หมุนซ้าย

    if (millis() - escapeTimer >= ESCAPE_TURN_MS) {
      escapeState = ESC_IDLE;
    }
    return;   // ไม่ทำอย่างอื่นระหว่างหมุน
  }

  // ===========================================================
  //  STEP 4 — AI ปกติ (ไม่ได้หนีเส้น)
  // ===========================================================

  TargetDir detected = detectTarget(dist);

  if (detected != DIR_NONE) {
    // เจอศัตรู → อัปเดต lock
    lastTarget  = detected;
    targetTimer = millis();
  }

  bool targetLocked = (lastTarget != DIR_NONE) &&
                      (millis() - targetTimer < LOCK_TIME);

  if (!targetLocked) {
    // ──── SEARCH: ไม่มีข้อมูลศัตรูเลย ────
    doSearch();
    return;
  }

  // ──── มีเป้าหมาย: โจมตีหรือติดตาม ────
  if (closestFront(dist) < RAM_DIST) {
    doAttack();                    // ใกล้พอ → พุ่ง!
  } else {
    doTrack(lastTarget, dist);     // ไกลอยู่ → เลี้ยวหา (proportional)
  }
}
