#include <Arduino.h>
#include <math.h>

// ---------------- CONFIG ----------------
#define EMG_PIN      A1
#define EOG_PIN      A0
#define SAMPLE_RATE  500  // Hz
#define ENVELOPE_WINDOW_MS 100
#define ENVELOPE_WINDOW_SIZE ((ENVELOPE_WINDOW_MS * SAMPLE_RATE) / 1000)

// Blink thresholds
const float BlinkTriggerNegative = -15;  
const float BlinkResetNegative   = -15;  
const unsigned long BLINK_DEBOUNCE_MS = 200;
const unsigned long BLINK_SEQUENCE_TIMEOUT_MS = 1000;

// ---------------- GLOBAL ----------------
float eogEnvBuffer[ENVELOPE_WINDOW_SIZE] = {0};
int eogEnvIndex = 0;
float eogEnvSum = 0;

float emgEnvBuffer[ENVELOPE_WINDOW_SIZE] = {0};
int emgEnvIndex = 0;
float emgEnvSum = 0;

unsigned long lastBlinkTime = 0;
unsigned long firstBlinkTime = 0;
int blinkSequenceCount = 0;

float eogBaseline = 0;
bool eogCalibrated = false;
unsigned long calibrationStart = 0;

// ---------------- FILTERS ----------------
struct Biquad {
  float b0, b1, b2;
  float a1, a2;
  float s1 = 0, s2 = 0;
};

// EOG filter
Biquad eogBiquad1 = { 0.9780, -1.9560, 0.9780, -1.9556, 0.9565 };
Biquad eogBiquad2 = { 0.9565, -1.9130, 0.9565, -1.9119, 0.9169 };

// EMG filter
Biquad emgBiquad1 = { 0.9780, -1.9560, 0.9780, -1.9556, 0.9565 };
Biquad emgBiquad2 = { 0.9565, -1.9130, 0.9565, -1.9119, 0.9169 };

float applyBiquad(Biquad &b, float x) {
  float y = b.b0 * x + b.s1;
  b.s1 = b.b1 * x - b.a1 * y + b.s2;
  b.s2 = b.b2 * x - b.a2 * y;
  return y;
}

float HPFilter4th(Biquad &b1, Biquad &b2, float x) {
  float y = applyBiquad(b1, x);
  return applyBiquad(b2, y);
}

// ---------------- ENVELOPE ----------------
float updateEnvelope(float *buffer, int &index, float &sum, float sample) {
  sum -= buffer[index];
  sum += sample;
  buffer[index] = sample;
  index = (index + 1) % ENVELOPE_WINDOW_SIZE;
  return sum / ENVELOPE_WINDOW_SIZE;
}

// ---------------- BASELINE ----------------
float calibrate(float env) {
  if (!eogCalibrated) {
    static float sum = 0; static int count = 0;
    if (calibrationStart == 0) calibrationStart = millis();
    sum += env; count++;
    if (millis() - calibrationStart > 1000) {
      eogBaseline = sum / count;
      eogCalibrated = true;
      Serial.println("EOG Calibration complete.");
    }
  }
  return env - eogBaseline;
}

// ---------------- BLINK DETECTION ----------------
bool detectBlink(float env) {
  static bool waitingForReset = false;
  unsigned long nowMs = millis();

  if (!waitingForReset && env < BlinkTriggerNegative &&
      (nowMs - lastBlinkTime) >= BLINK_DEBOUNCE_MS) {
    waitingForReset = true;
    lastBlinkTime = nowMs;
    return true;
  }
  if (waitingForReset && env > BlinkResetNegative) {
    waitingForReset = false;
  }
  return false;
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  pinMode(EMG_PIN, INPUT);
  pinMode(EOG_PIN, INPUT);
  Serial.println("EMG + EOG 4th-Order Butterworth Ready!");
}

// ---------------- LOOP ----------------
void loop() {
  // --- EOG Processing ---
  float eogRaw = (float)analogRead(EOG_PIN);
  float eogFiltered = HPFilter4th(eogBiquad1, eogBiquad2, eogRaw);
  float eogEnv = updateEnvelope(eogEnvBuffer, eogEnvIndex, eogEnvSum, eogFiltered);
  float eogAdjusted = calibrate(eogEnv);

  // --- EMG Processing ---
  float emgRaw = (float)analogRead(EMG_PIN);
  float emgFiltered = HPFilter4th(emgBiquad1, emgBiquad2, emgRaw);
  float emgEnv = updateEnvelope(emgEnvBuffer, emgEnvIndex, emgEnvSum, emgFiltered);
  float emgNormalized = emgEnv / 1023.0;  // normalize to 0-1

  // Blink detection
  if (eogCalibrated && detectBlink(eogAdjusted)) {
    if (blinkSequenceCount == 0) firstBlinkTime = millis();
    blinkSequenceCount++;
  }

  if (blinkSequenceCount > 0 && (millis() - firstBlinkTime) >= BLINK_SEQUENCE_TIMEOUT_MS) {
    int blinksInSequence = blinkSequenceCount;
    blinkSequenceCount = 0;

    // Only send 1 or 2 blinks
    if (blinksInSequence == 2 || blinksInSequence == 3 || blinksInSequence == 4) {
      Serial.print(blinksInSequence - 1);  // blink code
      Serial.print(",");
      Serial.println(emgNormalized, 3);    // normalized EMG
    }
  }

  delay(2);  // ~500 Hz
}
