#include <Arduino.h>
#include <math.h>

// ---------------- CONFIG ----------------
#define SAMPLE_RATE 512
#define INPUT_PIN   A0

#define ENVELOPE_WINDOW_MS 100
#define ENVELOPE_WINDOW_SIZE ((ENVELOPE_WINDOW_MS * SAMPLE_RATE) / 1000)

// Blink thresholds
const float BlinkTriggerNegative = -15;  // trigger when envelope < -15
const float BlinkResetNegative   = -15;   // reset above -5
const unsigned long BLINK_DEBOUNCE_MS = 200;
const unsigned long BLINK_SEQUENCE_TIMEOUT_MS = 600;

// ---------------- GLOBAL ----------------
float envBuffer[ENVELOPE_WINDOW_SIZE] = {0};
int envIndex = 0;
float envSum = 0;

unsigned long lastBlinkTime = 0;
unsigned long firstBlinkTime = 0;
int blinkSequenceCount = 0;

float baseline = 0;
bool calibrated = false;
unsigned long calibrationStart = 0;

// ---------------- FILTERS ----------------
// Stable Direct Form II Transposed biquad
struct Biquad {
  float b0, b1, b2;
  float a1, a2;
  float s1 = 0, s2 = 0; // state variables
};

// Example coefficients for 5 Hz cutoff, 512 Hz sampling
Biquad biquad1 = { 0.9780, -1.9560, 0.9780, -1.9556, 0.9565 };
Biquad biquad2 = { 0.9565, -1.9130, 0.9565, -1.9119, 0.9169 };

// Apply one biquad
float applyBiquad(Biquad &b, float x) {
  float y = b.b0 * x + b.s1;
  b.s1 = b.b1 * x - b.a1 * y + b.s2;
  b.s2 = b.b2 * x - b.a2 * y;
  return y;
}

// 4th-order cascade
float HPFilter4th(float x) {
  float y = applyBiquad(biquad1, x);
  y = applyBiquad(biquad2, y);
  return y;
}

// ---------------- ENVELOPE ----------------
float updateEnvelope(float sample) {
  envSum -= envBuffer[envIndex];
  envSum += sample;
  envBuffer[envIndex] = sample;
  envIndex = (envIndex + 1) % ENVELOPE_WINDOW_SIZE;
  return envSum / ENVELOPE_WINDOW_SIZE;
}

// ---------------- BASELINE ----------------
float calibrate(float env) {
  if (!calibrated) {
    static float sum = 0; static int count = 0;
    if (calibrationStart == 0) calibrationStart = millis();
    sum += env; count++;
    if (millis() - calibrationStart > 1000) {
      baseline = sum / count;
      calibrated = true;
      Serial.println("Calibration complete.");
    }
  }
  return env - baseline;
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
  pinMode(INPUT_PIN, INPUT);
  Serial.println("Stable Signed 4th-Order Butterworth EOG Blink Detector Ready!");
}

// ---------------- LOOP ----------------
void loop() {
  float raw = (float)analogRead(INPUT_PIN);

  // Apply stable 4th-order high-pass filter
  float filtered = HPFilter4th(raw);

  // Signed envelope
  float env = updateEnvelope(filtered);
  float adjusted = calibrate(env);

  // Debug output for Serial Plotter (optional)
  // Serial.print(adjusted); Serial.print(",");
  // Serial.print(BlinkTriggerNegative); Serial.print(",");
  // Serial.println(BlinkResetNegative);

  // ---------------- BLINK SEQUENCE DETECTION ----------------
  if (calibrated && detectBlink(adjusted)) {
    if (blinkSequenceCount == 0) firstBlinkTime = millis();
    blinkSequenceCount++;
  }

  // Check if blink sequence window expired
  if (blinkSequenceCount > 0 && (millis() - firstBlinkTime) >= BLINK_SEQUENCE_TIMEOUT_MS) {
    int blinksInSequence = blinkSequenceCount;
    blinkSequenceCount = 0; // reset for next sequence

    // Send number of blinks to another program
    Serial.print("Blinks: ");
    Serial.println(blinksInSequence); // 1, 2, 3...
  }

  delay(2);  // ~500 Hz sampling
}
