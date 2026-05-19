#include <Arduino.h>
#include "Strategy.h"
#include "Motors.h"
#include "Config.h"

// ============================================================
//  Strategy.cpp — กลยุทธ์หลักของหุ่นยนต์ซูโม่
//
//  State Machine (ลำดับความสำคัญ):
//
//    1. ESCAPE_BACK  ← เจอเส้น → ถอยหลัง ESCAPE_BACK_MS
//    2. ESCAPE_TURN  ← หมุนกลับเข้าสนาม ESCAPE_TURN_MS
//    3. AI ปกติ:
//         ATTACK  — เจอศัตรูในระยะพุ่ง (< RAM_DIST)
//         TRACK   — เจอศัตรูแต่ยังไกล (proportional steering)
//         SEARCH  — ยังไม่เจอศัตรู (spiral pattern)
// ============================================================


// ── ประเภทข้อมูล (Enum) ──────────────────────────────────────

enum EscapeState {
  ESC_IDLE = 0,   // ไม่ได้หนีเส้น
  ESC_BACK = 1,   // กำลังถอยหลัง
  ESC_TURN = 2,   // กำลังหมุนกลับ
};

enum TargetDir {
  DIR_NONE  =  0,
  DIR_LEFT  = -1,
  DIR_RIGHT =  1,
};


// ── State ภายใน (เห็นได้เฉพาะ file นี้) ─────────────────────
namespace {

  // --- Escape state ---
  EscapeState   escapeState = ESC_IDLE;
  bool          escapeLeft  = false;    // เจอเส้นซ้าย? (ใช้ตอนหมุนกลับ)
  unsigned long escapeTimer = 0;

  // --- Target lock ---
  TargetDir     lastTarget  = DIR_NONE;
  unsigned long targetTimer = 0;        // millis() ที่เห็นเป้าครั้งล่าสุด

  // --- Search state ---
  bool          searchDir   = false;    // false=ซ้าย, true=ขวา
  unsigned long searchTimer = 0;


  // ================================================================
  //  Helper Functions
  // ================================================================

  // คืน true ถ้า sensor ด้านหน้าใดด้านหนึ่งเห็นศัตรูในระยะ TRACK_DIST
  bool frontSeesTarget(const Dist& d) {
    return (d.fl < TRACK_DIST) ||
           (d.fc < TRACK_DIST) ||
           (d.fr < TRACK_DIST);
  }

  // วิเคราะห์ข้อมูล sensor แล้วคืนทิศของศัตรู (หรือ DIR_NONE)
  // ลำดับความสำคัญ: หน้ากลาง → หน้าซ้าย → หน้าขวา → ข้างซ้าย → ข้างขวา
  TargetDir detectTarget(const Dist& d) {
    if (d.fc < TRACK_DIST) {
      return (d.fl <= d.fr) ? DIR_LEFT : DIR_RIGHT;
    }
    if (d.fl < TRACK_DIST) return DIR_LEFT;
    if (d.fr < TRACK_DIST) return DIR_RIGHT;
    if (d.sl < SIDE_DIST)  return DIR_LEFT;
    if (d.sr < SIDE_DIST)  return DIR_RIGHT;
    return DIR_NONE;
  }

  // คืนระยะที่ใกล้ที่สุดจาก sensor ด้านหน้า 3 ตัว
  int closestFront(const Dist& d) {
    return min(min(d.fl, d.fc), d.fr);
  }


  // ================================================================
  //  พฤติกรรมหลัก 3 แบบ
  // ================================================================

  // ATTACK — พุ่งชนเต็มสปีด
  void doAttack() {
    Motors::move(ATTACK_SPEED, ATTACK_SPEED);
  }

  // TRACK — เลี้ยวตามศัตรูแบบ proportional
  //
  //   ถ้า sensor หน้ากลาง (fc) เห็นศัตรู:
  //     คำนวณ diff = fl - fr แล้วปรับความเร็วล้อตามสัดส่วน
  //     → เลี้ยวนิดถ้าศัตรูเกือบตรงหน้า, เลี้ยวมากถ้าเอียง
  //
  //   ถ้า sensor ข้างเจอ (fc ไม่เห็น):
  //     ใช้ความเร็วตายตัว TRACK_FAST / TRACK_SLOW
  //
  void doTrack(TargetDir dir, const Dist& d) {
    if (d.fc < TRACK_DIST) {
      // Proportional steering จาก fl - fr
      int diff = d.fl - d.fr;   // บวก = ศัตรูอยู่ขวา, ลบ = ซ้าย

      if (abs(diff) < TRACK_CENTER_ZONE) {
        // ศัตรูตรงหน้าพอ → วิ่งตรง
        Motors::move(TRACK_FAST, TRACK_FAST);
        return;
      }

      // แปลง diff เป็นค่าเลี้ยว (20–90)
      int turn = map(abs(diff), TRACK_CENTER_ZONE, TRACK_DIST, 20, 90);
      turn = constrain(turn, 20, 90);

      int fast = TRACK_FAST;
      int slow = constrain(TRACK_FAST - turn, 0, 255);

      if (diff > 0)
        Motors::move(fast, slow);   // ศัตรูขวา → ล้อขวาช้า
      else
        Motors::move(slow, fast);   // ศัตรูซ้าย → ล้อซ้ายช้า

    } else {
      // Sensor ข้างเจอ → เลี้ยวตายตัว
      if (dir == DIR_LEFT)
        Motors::move(TRACK_SLOW, TRACK_FAST);
      else
        Motors::move(TRACK_FAST, TRACK_SLOW);
    }
  }

  // SEARCH — หมุนค้นหาแบบ spiral
  //
  //   แต่ละรอบ (SEARCH_FWD_MS + SEARCH_SWAP_MS):
  //     ช่วงต้น: เดินหน้า SEARCH_FWD_MS ms
  //     ช่วงหลัง: หมุนค้นหา SEARCH_SWAP_MS ms
  //     ครบรอบ: สลับทิศหมุน แล้วเริ่มรอบใหม่
  //
  void doSearch() {
    const unsigned long CYCLE = SEARCH_FWD_MS + SEARCH_SWAP_MS;  // 1 รอบ = 800ms
    unsigned long elapsed = millis() - searchTimer;

    // ครบ 1 รอบ → สลับทิศ แล้วรีเซ็ต timer
    if (elapsed >= CYCLE) {
      searchDir   = !searchDir;
      searchTimer = millis();
      elapsed     = 0;
    }

    if (elapsed < SEARCH_FWD_MS) {
      // ช่วงเดินหน้า
      Motors::move(SEARCH_SPEED, SEARCH_SPEED);
    } else {
      // ช่วงหมุน
      if (searchDir)
        Motors::move(-SEARCH_SPEED, SEARCH_SPEED);   // หมุนขวา
      else
        Motors::move(SEARCH_SPEED, -SEARCH_SPEED);   // หมุนซ้าย
    }
  }

} // namespace (anonymous)


// ================================================================
//  Public API
// ================================================================

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

  // ── Step 1: ตรวจเส้น (ความสำคัญสูงสุด) ─────────────────────
  // เริ่ม escape ใหม่ได้เฉพาะตอน IDLE
  // ถ้าไม่ตรวจ escapeState → timer จะ reset ซ้ำทุก loop จนหนีไม่ออก
  if ((line.left || line.right) && escapeState == ESC_IDLE) {
    escapeState = ESC_BACK;
    escapeLeft  = line.left;
    escapeTimer = millis();
  }

  // ── Step 2: ถอยหลังหนีเส้น ──────────────────────────────────
  if (escapeState == ESC_BACK) {
    Motors::move(-ESCAPE_BACK, -ESCAPE_BACK);
    if (millis() - escapeTimer >= ESCAPE_BACK_MS) {
      escapeState = ESC_TURN;
      escapeTimer = millis();
    }
    return;
  }

  // ── Step 3: หมุนกลับเข้าสนาม ────────────────────────────────
  // เจอเส้นซ้าย → หมุนขวา (และกลับกัน)
  if (escapeState == ESC_TURN) {
    if (escapeLeft)
      Motors::move( ESCAPE_TURN, -ESCAPE_TURN);
    else
      Motors::move(-ESCAPE_TURN,  ESCAPE_TURN);

    if (millis() - escapeTimer >= ESCAPE_TURN_MS) {
      escapeState = ESC_IDLE;
    }
    return;
  }

  // ── Step 4: AI ปกติ ──────────────────────────────────────────

  // อัปเดต target lock ถ้าเห็นศัตรู
  TargetDir detected = detectTarget(dist);
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

  // มีเป้าหมาย: ตัดสินใจ Attack หรือ Track
  if (closestFront(dist) < RAM_DIST)
    doAttack();
  else
    doTrack(lastTarget, dist);
}
