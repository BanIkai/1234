#include <SharpIR.h>

// ── PIN ──────────────────────────────────────────────────────
#define PIN_SHARP_SL  A0
#define PIN_SHARP_FL  A1
#define PIN_SHARP_FC  A3
#define PIN_SHARP_FR  A4
#define PIN_SHARP_SR  A5
#define PIN_LINE_L    A2
#define PIN_LINE_R    A7

#define LINE_L_TH    920
#define LINE_R_TH    500

// ── sensor objects ───────────────────────────────────────────
SharpIR sensorSL(SharpIR::GP2Y0A41SK0F, PIN_SHARP_SL);
SharpIR sensorFL(SharpIR::GP2Y0A41SK0F, PIN_SHARP_FL);
SharpIR sensorFC(SharpIR::GP2Y0A41SK0F, PIN_SHARP_FC);
SharpIR sensorFR(SharpIR::GP2Y0A41SK0F, PIN_SHARP_FR);
SharpIR sensorSR(SharpIR::GP2Y0A41SK0F, PIN_SHARP_SR);

int readMM(SharpIR& s) {
  int mm = s.distance() * 10;
  return (mm < 40 || mm > 300) ? 999 : mm;
}

void printDist(const char* label, int mm) {
  Serial.print(label);
  if (mm == 999) {
    Serial.print(F("  ---  "));
  } else {
    Serial.print(F("  "));
    Serial.print(mm);
    Serial.print(F("mm"));
    if (mm < 180) Serial.print(F(" RAM!"));
  }
}

void setup() {
  Serial.begin(115200);
}

void loop() {
  // sensor ระยะ
  Serial.println(F("--- DIST ---"));
  printDist("[SL]", readMM(sensorSL)); Serial.println();
  printDist("[FL]", readMM(sensorFL)); Serial.println();
  printDist("[FC]", readMM(sensorFC)); Serial.println();
  printDist("[FR]", readMM(sensorFR)); Serial.println();
  printDist("[SR]", readMM(sensorSR)); Serial.println();

  // sensor เส้น
  int lL = analogRead(PIN_LINE_L);
  int lR = analogRead(PIN_LINE_R);
  Serial.println(F("--- LINE ---"));
  Serial.print(F("L: ")); Serial.print(lL);
  Serial.print(F("R: ")); Serial.print(lR);

  Serial.println();
  delay(200);
}
