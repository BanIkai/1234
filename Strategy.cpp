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
//    - เจอศัตรูไกล  → TRACK  (เลี้ยวหา)
//    - ไม่เจอเลย    → SEARCH (หมุนค้นหา สลับทิศตาม timer)
// ============================================================


// ── enum แทน magic number ────────────────────────────────────
enum EscapeState {
  IDLE        = 0,   // ไม่ได้หนีเส้น
  ESCAPE_BACK = 1,   // กำลังถอยหลัง
  ESCAPE_TURN = 2,   // กำลังหมุนกลับ
};

enum TargetDir {
  DIR_NONE  = 0,
  DIR_LEFT  = -1,
  DIR_RIGHT =  1,
};


// ── state ภายใน (private ต่อ file นี้) ──────────────────────
namespace {

  EscapeState escapeState = IDLE;
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

  void doTrack(TargetDir dir) {
    // หมุนหาศัตรู: ด้านที่เจอวิ่งเร็ว ด้านตรงข้ามช้า
    if (dir == DIR_LEFT)
      Motors::move(TRACK_SLOW, TRACK_FAST);
    else
      Motors::move(TRACK_FAST, TRACK_SLOW);
  }

  void doSearch() {
    // สลับทิศค้นหาทุก SEARCH_SWAP_MS
    if (millis() - searchTimer >= SEARCH_SWAP_MS) {
      searchDir   = !searchDir;
      searchTimer = millis();
    }

    if (searchDir)
      Motors::move(-SEARCH_SPEED, SEARCH_SPEED);   // หมุนขวา
    else
      Motors::move(SEARCH_SPEED, -SEARCH_SPEED);   // หมุนซ้าย
  }

} // namespace (anonymous)


// ── public API ───────────────────────────────────────────────

void AI::reset() {
  escapeState = IDLE;
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
  if (line.left || line.right) {
    escapeState = ESCAPE_BACK;
    escapeLeft  = line.left;     // จำว่าเจอซ้ายหรือขวา
    escapeTimer = millis();
  }

  // ===========================================================
  //  STEP 2 — ถอยหลังหนีเส้น
  // ===========================================================
  if (escapeState == ESCAPE_BACK) {
    Motors::move(-ESCAPE_BACK, -ESCAPE_BACK);

    if (millis() - escapeTimer >= ESCAPE_BACK_MS) {
      escapeState = ESCAPE_TURN;
      escapeTimer = millis();
    }
    return;   // ไม่ทำอย่างอื่นระหว่างถอย
  }

  // ===========================================================
  //  STEP 3 — หมุนกลับเข้าสนาม
  //           เจอเส้นซ้าย → หมุนขวา (และกลับกัน)
  // ===========================================================
  if (escapeState == ESCAPE_TURN) {
    if (escapeLeft)
      Motors::move( ESCAPE_TURN, -ESCAPE_TURN);   // หมุนขวา
    else
      Motors::move(-ESCAPE_TURN,  ESCAPE_TURN);   // หมุนซ้าย

    if (millis() - escapeTimer >= ESCAPE_TURN_MS) {
      escapeState = IDLE;
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
    doAttack();           // ใกล้พอ → พุ่ง!
  } else {
    doTrack(lastTarget);  // ไกลอยู่ → เลี้ยวหา
  }
}
