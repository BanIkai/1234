#include "Strategy.h"
#include "Config.h"
#include "Motors.h"

// =============================================================================
//  Strategy.cpp — ตรรกะการต่อสู้ของหุ่นยนต์ซูโม่
//
//  ลำดับความสำคัญของ state (สูงสุด → ต่ำสุด):
//   1. LINE_ESCAPE  — หนีเส้นขาวขอบสนาม (เร่งด่วนที่สุด)
//   2. ANTI_FLANK   — หมุนสู้ภัยที่โจมตีจากด้านข้าง
//   3. RAM          — พุ่งชนเมื่อคู่ต่อสู้อยู่ใกล้มาก
//   4. TRACK        — ไล่ตามและปรับทิศเมื่อเห็นคู่ต่อสู้
//   5. SEARCH       — หมุนค้นหาเมื่อยังไม่เห็น
// =============================================================================

namespace {

  // ---------------------------------------------------------------------------
  //  ตัวแปรสถานะ — คงอยู่ข้าม step() ทุก loop
  // ---------------------------------------------------------------------------

  // ทิศที่เห็นคู่ต่อสู้ครั้งล่าสุด (-1=ซ้าย, 0=กลาง, +1=ขวา)
  int8_t lastSeenDirection = 0;

  // -- ลำดับขั้นตอนหนีเส้น --
  enum LinePhase : uint8_t { LP_NONE, LP_REVERSE, LP_TURN };
  LinePhase linePhase        = LP_NONE;
  uint32_t  linePhaseEndTime = 0;
  int8_t    lineTurnDir      = 0;  // +1=หมุนขวา, -1=หมุนซ้าย, 0=หมุน 180°

  // -- สถานะการป้องกันด้านข้าง (anti-flank) --
  uint32_t flankEndTime     = 0;
  int8_t   flankTurnDir     = 0;
  uint32_t flankLeftSince   = 0;  // เวลาที่เซ็นเซอร์ซ้ายเริ่มเห็น flank
  uint32_t flankRightSince  = 0;

  // -- สถานะตรวจจับการติดขัด (stall) ขณะพุ่งชน --
  bool     isStallPushing   = false;
  uint16_t stallRefDistance = 0;
  uint32_t stallRefTime     = 0;
  uint32_t releaseFarSince  = 0;

  // ---------------------------------------------------------------------------
  //  ฟังก์ชันช่วย
  // ---------------------------------------------------------------------------

  uint16_t minOf3(uint16_t a, uint16_t b, uint16_t c) {
    return min(min(a, b), c);
  }

  // หมุนทั้งตัว: dir=+1 ตามเข็ม (ขวา), dir=-1 ทวนเข็ม (ซ้าย)
  void spinInPlace(int8_t dir, uint8_t pwm) {
    if (dir > 0) Motors::drive(+pwm, -pwm);  // ล้อซ้ายหน้า ล้อขวาถอย
    else         Motors::drive(-pwm, +pwm);
  }

  // ---------------------------------------------------------------------------
  //  1. หนีเส้น (LINE_ESCAPE)
  //
  //  เมื่อเซ็นเซอร์ตรวจเส้นเห็นขาว:
  //    ระยะที่ 1 — ถอยหลัง LINE_REVERSE_MS ms
  //    ระยะที่ 2 — หมุนหนีจากขอบ LINE_TURN_MS ms (หรือ 180° ถ้าเห็นทั้งสอง)
  //
  //  คืน true = กำลังหนีเส้นอยู่ (ฟังก์ชันอื่นต้องหยุดรอ)
  // ---------------------------------------------------------------------------
  bool handleLineEscape(const LineReadings& line) {
    uint32_t now = millis();

    // ---- กำลังอยู่ในลำดับหนีเส้น ----
    if (linePhase == LP_REVERSE) {
      Motors::drive(-RAM_PWM, -RAM_PWM);
      if ((int32_t)(linePhaseEndTime - now) <= 0) {
        // ถอยเสร็จ → เปลี่ยนเป็นระยะหมุน
        linePhase        = LP_TURN;
        uint16_t turnDuration = (lineTurnDir == 0) ? LINE_180_MS : LINE_TURN_MS;
        linePhaseEndTime = now + turnDuration;
      }
      return true;
    }

    if (linePhase == LP_TURN) {
      spinInPlace(lineTurnDir, TURN_PWM);
      if ((int32_t)(linePhaseEndTime - now) <= 0) {
        linePhase = LP_NONE;  // หมุนเสร็จ — กลับโหมดปกติ
      }
      return true;
    }

    // ---- ตรวจว่ามีเส้นใหม่หรือไม่ ----

    // เส้นทั้งสองข้าง → ถอยและหมุน 180°
    if (line.left_white && line.right_white) {
      linePhase        = LP_REVERSE;
      linePhaseEndTime = now + LINE_REVERSE_MS;
      lineTurnDir      = 0;  // sentinel: หมุน 180°
      isStallPushing   = false;
      flankEndTime     = 0;
      Motors::drive(-RAM_PWM, -RAM_PWM);
      return true;
    }

    // เส้นซ้าย → หมุนขวาหนี
    if (line.left_white) {
      linePhase        = LP_REVERSE;
      linePhaseEndTime = now + LINE_REVERSE_MS;
      lineTurnDir      = +1;  // หมุนขวา = หนีจากขอบซ้าย
      isStallPushing   = false;
      flankEndTime     = 0;
      Motors::drive(-RAM_PWM, -RAM_PWM);
      return true;
    }

    // เส้นขวา → หมุนซ้ายหนี
    if (line.right_white) {
      linePhase        = LP_REVERSE;
      linePhaseEndTime = now + LINE_REVERSE_MS;
      lineTurnDir      = -1;  // หมุนซ้าย = หนีจากขอบขวา
      isStallPushing   = false;
      flankEndTime     = 0;
      Motors::drive(-RAM_PWM, -RAM_PWM);
      return true;
    }

    return false;  // ไม่มีเส้น
  }

  // ---------------------------------------------------------------------------
  //  ตรวจว่าสัญญาณด้านข้างเป็นภัยจริง หรือแค่แบนเนอร์หลอก
  //
  //  เชื่อว่า "จริง" ก็ต่อเมื่อ:
  //   - ด้านข้างใกล้กว่า FLANK_CLOSE_MM
  //   - ด้านข้างใกล้กว่าเซ็นเซอร์หน้าทุกตัว อีก FLANK_LEAD_MM
  //     (ถ้าตัวจริงอยู่ข้างหน้า หน้าจะเห็นใกล้ ≈ เท่ากัน → ไม่ผ่าน)
  // ---------------------------------------------------------------------------
  bool isRealFlankThreat(uint16_t side_dist, const ToFReadings& tof) {
    if (side_dist >= FLANK_CLOSE_MM) return false;
    uint16_t front_min = minOf3(tof.fl, tof.fc, tof.fr);
    return (uint32_t)side_dist + FLANK_LEAD_MM < (uint32_t)front_min;
  }

  // ---------------------------------------------------------------------------
  //  2. รับมือภัยด้านข้าง (ANTI_FLANK)
  //
  //  เมื่อเห็น flank จริงต่อเนื่องนาน FLANK_PERSIST_MS:
  //    → หมุนเข้าหาคู่ต่อสู้ ANTI_FLANK_MS ms (หรือจนกว่า FC จะเห็น)
  //
  //  คืน true = กำลังหมุนสู้ flank อยู่
  // ---------------------------------------------------------------------------
  bool handleAntiFlank(const ToFReadings& tof) {
    uint32_t now = millis();

    // ---- กำลังหมุนอยู่ในช่วง anti-flank ----
    if (flankEndTime != 0 && (int32_t)(flankEndTime - now) > 0) {
      if (tof.fc < ENGAGE_DISTANCE) {
        // FC เห็นคู่ต่อสู้แล้ว — ออกจาก anti-flank ทันที
        flankEndTime = 0;
        return false;
      }
      spinInPlace(flankTurnDir, TURN_PWM);
      return true;
    }
    flankEndTime = 0;

    // ---- อัปเดตเวลาที่เห็น flank แต่ละข้าง (สำหรับ persistence check) ----
    bool leftIsRealFlank  = isRealFlankThreat(tof.sl, tof);
    bool rightIsRealFlank = isRealFlankThreat(tof.sr, tof);

    if (leftIsRealFlank)  { if (flankLeftSince  == 0) flankLeftSince  = now; }
    else                  { flankLeftSince  = 0; }

    if (rightIsRealFlank) { if (flankRightSince == 0) flankRightSince = now; }
    else                  { flankRightSince = 0; }

    // ---- ตรวจว่า flank ใดถือนานพอ ----
    bool leftTriggered  = flankLeftSince  != 0 && (now - flankLeftSince)  >= FLANK_PERSIST_MS;
    bool rightTriggered = flankRightSince != 0 && (now - flankRightSince) >= FLANK_PERSIST_MS;

    // ถ้าทั้งสองข้าง trigger พร้อมกัน → หมุนหาข้างที่ใกล้กว่า
    if (leftTriggered && (!rightTriggered || tof.sl <= tof.sr)) {
      flankTurnDir    = -1;  // หมุนซ้ายเข้าหา
      flankEndTime    = now + ANTI_FLANK_MS;
      isStallPushing  = false;
      flankLeftSince  = 0;
      spinInPlace(flankTurnDir, TURN_PWM);
      return true;
    }
    if (rightTriggered) {
      flankTurnDir    = +1;  // หมุนขวาเข้าหา
      flankEndTime    = now + ANTI_FLANK_MS;
      isStallPushing  = false;
      flankRightSince = 0;
      spinInPlace(flankTurnDir, TURN_PWM);
      return true;
    }

    return false;  // ไม่มี flank
  }

  // ---------------------------------------------------------------------------
  //  ตรวจจับการติดขัด (stall latch) ขณะกดคู่ต่อสู้
  //
  //  ถ้า FC ใกล้มากแต่ระยะไม่ลดลงใน STALL_WINDOW_MS → latch = pushing
  //  ออกจาก latch เมื่อ FC ไกลขึ้นอย่างต่อเนื่อง
  //
  //  คืน true = กำลัง stall-push อยู่ (ให้ใช้ RAM_PWM ต่อไป)
  // ---------------------------------------------------------------------------
  bool updateStallLatch(uint16_t front_center_dist) {
    uint32_t now = millis();

    if (!isStallPushing) {
      // รอดู: ถ้า FC ใกล้มาก แต่ระยะไม่ลดเลย ถือว่าติดขัด
      if (front_center_dist < STALL_NEAR_MM) {
        if (stallRefTime == 0) {
          stallRefDistance = front_center_dist;
          stallRefTime     = now;
        } else if ((int32_t)(now - stallRefTime) >= STALL_WINDOW_MS) {
          int distChange = (int)stallRefDistance - (int)front_center_dist;
          bool notMoving = abs(distChange) < (int)STALL_DELTA_MM;
          if (notMoving) {
            isStallPushing  = true;
            releaseFarSince = 0;
          }
          // เริ่ม window ใหม่
          stallRefDistance = front_center_dist;
          stallRefTime     = now;
        }
      } else {
        stallRefTime = 0;  // ไม่ใกล้พอ — reset
      }
      return false;
    }

    // isStallPushing == true: รอให้ FC ไกลออกอย่างต่อเนื่องก่อนปลด latch
    if (front_center_dist > STALL_RELEASE_MM) {
      if (releaseFarSince == 0) releaseFarSince = now;
      else if ((int32_t)(now - releaseFarSince) >= STALL_RELEASE_MS) {
        isStallPushing  = false;
        stallRefTime    = 0;
        releaseFarSince = 0;
      }
    } else {
      releaseFarSince = 0;  // กลับใกล้แล้ว — รอต่อ
    }

    return isStallPushing;
  }

}  // namespace (private)

// =============================================================================
//  Public API
// =============================================================================

void Strategy::reset() {
  lastSeenDirection = 0;
  linePhase         = LP_NONE;
  linePhaseEndTime  = 0;
  lineTurnDir       = 0;
  flankEndTime      = 0;
  flankTurnDir      = 0;
  flankLeftSince    = 0;
  flankRightSince   = 0;
  isStallPushing    = false;
  stallRefDistance  = 0;
  stallRefTime      = 0;
  releaseFarSince   = 0;
}

SumoState Strategy::step(const ToFReadings& tof, const LineReadings& line) {

  // ======== 1. หนีเส้นขอบ (ความสำคัญสูงสุด) ========
  if (handleLineEscape(line)) return STATE_LINE_ESCAPE;

  // ======== 2. รับมือภัยด้านข้าง ========
  if (handleAntiFlank(tof)) return STATE_ANTI_FLANK;

  // ======== 3. พุ่งชน — เมื่อ FC เห็นใกล้มาก หรือกำลัง stall-push ========
  bool isStalling = updateStallLatch(tof.fc);
  if (isStalling || tof.fc < RAM_DISTANCE) {
    Motors::drive(+RAM_PWM, +RAM_PWM);
    lastSeenDirection = 0;
    return STATE_RAM;
  }

  // ======== 4. ไล่ตาม — เมื่อ FC เห็นคู่ต่อสู้ในระยะ ========
  if (tof.fc < ENGAGE_DISTANCE) {
    // FC เห็นชัด → ขับตรง แต่ nudge เล็กน้อยถ้า FL/FR ใกล้กว่า FC มาก
    bool leftLeading  = (uint32_t)tof.fl + FC_BIAS_MM < (uint32_t)tof.fc;
    bool rightLeading = (uint32_t)tof.fr + FC_BIAS_MM < (uint32_t)tof.fc;

    if (leftLeading && !rightLeading) {
      // คู่ต่อสู้เอียงซ้ายเล็กน้อย → เลี้ยวซ้ายนิด (ซ้ายช้า, ขวาเร็ว)
      Motors::drive(+CURVE_SLOW_PWM, +CURVE_FAST_PWM);
      lastSeenDirection = -1;
    } else if (rightLeading && !leftLeading) {
      // คู่ต่อสู้เอียงขวาเล็กน้อย → เลี้ยวขวานิด (ซ้ายเร็ว, ขวาช้า)
      Motors::drive(+CURVE_FAST_PWM, +CURVE_SLOW_PWM);
      lastSeenDirection = +1;
    } else {
      // FC ตรงกลาง → ขับตรงเต็มสปีด
      Motors::drive(+FORWARD_PWM, +FORWARD_PWM);
      lastSeenDirection = 0;
    }
    return STATE_TRACK;
  }

  // FC ไม่เห็น แต่ FL หรือ FR เห็น → คู่ต่อสู้เยื้องออกไป เลี้ยวหา
  if (tof.fl < ENGAGE_DISTANCE || tof.fr < ENGAGE_DISTANCE) {
    if (tof.fl <= tof.fr) {
      // ซ้ายใกล้กว่า → เลี้ยวซ้าย
      Motors::drive(+CURVE_SLOW_PWM, +CURVE_FAST_PWM);
      lastSeenDirection = -1;
    } else {
      // ขวาใกล้กว่า → เลี้ยวขวา
      Motors::drive(+CURVE_FAST_PWM, +CURVE_SLOW_PWM);
      lastSeenDirection = +1;
    }
    return STATE_TRACK;
  }

  // ======== 5. อัปเดตทิศจากเซ็นเซอร์ด้านข้าง (ใช้ช่วย SEARCH เท่านั้น) ========
  // ไม่สั่งมอเตอร์ที่นี่ — แค่จำว่าเห็นจากทิศไหน
  if (tof.sl < ENGAGE_DISTANCE && tof.sl <= tof.sr) {
    lastSeenDirection = -1;
  } else if (tof.sr < ENGAGE_DISTANCE) {
    lastSeenDirection = +1;
  }

  // ======== 6. ค้นหา — หมุนไปทางที่เคยเห็นล่าสุด ========
  int8_t searchDir = (lastSeenDirection > 0) ? +1 : -1;
  spinInPlace(searchDir, SEARCH_PWM);
  return STATE_SEARCH;
}
