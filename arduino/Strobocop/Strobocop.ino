#include <DmxSimple.h>

const uint8_t dmxPin = 4;
const uint8_t micPin = A0;

const uint8_t dimmerChannel = 1;
const uint8_t strobeChannel = 2;
const uint8_t redChannel = 3;
const uint8_t greenChannel = 4;
const uint8_t whiteChannel = 6;
const uint8_t smokeChannel = 64;

// Thresholds based on micDiff, not raw mic value
const int micThreshold = 12;  // to trigger smoke
const int micMin = 10;        // min difference for light response
const int micMax = 40;        // max difference mapped to full brightness

const unsigned long smokeDuration = 1500;
const unsigned long activeDuration = 10000;
const unsigned long busyDuration = 10000;

enum State { READY,
             ACTIVE,
             BUSY };
State currentState = READY;

unsigned long stateStartTime = 0;
unsigned long smokeStartTime = 0;
bool smokeOn = false;
int micBaseline = 0;

const bool DEBUG = false;

void setDmx(uint8_t channel, uint8_t value) {
  DmxSimple.write(channel, value);
}

void setStatusReady() {
  setDmx(dimmerChannel, 255);
  setDmx(redChannel, 0);
  setDmx(greenChannel, 255);
  setDmx(whiteChannel, 0);
  setDmx(strobeChannel, 32);  // soft open
}

void setStatusBusy() {
  setDmx(dimmerChannel, 255);
  setDmx(redChannel, 255);
  setDmx(greenChannel, 0);
  setDmx(whiteChannel, 0);
  setDmx(strobeChannel, 32);
}

void setup() {
  Serial.begin(9600);
  DmxSimple.usePin(dmxPin);
  DmxSimple.maxChannel(65);

  // Init DMX channels
  setDmx(smokeChannel, 0);
  setDmx(redChannel, 0);
  setDmx(greenChannel, 0);
  setDmx(whiteChannel, 0);
  setDmx(dimmerChannel, 0);
  setDmx(strobeChannel, 0);

  // Start in READY state
  setStatusReady();
  currentState = READY;
  stateStartTime = millis();

  // Set initial mic baseline
  micBaseline = analogRead(micPin);
}

void loop() {
  unsigned long currentTime = millis();

  // Smooth baseline tracking
  int micValue = analogRead(micPin);
  int micDiff = abs(micValue - micBaseline);


  Serial.println(micDiff);

  switch (currentState) {

    case READY:
      micBaseline = (micBaseline * 15 + micValue) / 16;
      setStatusReady();
      if (micDiff > micThreshold) {
        Serial.println("Triggered! Entering ACTIVE state.");

        setDmx(smokeChannel, 255);
        smokeOn = true;
        smokeStartTime = currentTime;

        currentState = ACTIVE;
        stateStartTime = currentTime;
      }
      break;

    case ACTIVE:
      {
        // Handle smoke duration
        if (smokeOn && (currentTime - smokeStartTime >= smokeDuration)) {
          setDmx(smokeChannel, 0);
          smokeOn = false;
        }

        // Handle reactive dimming
        static uint8_t currentDimmerValue = 0;
        if (micDiff > micMin) {
          int safeDiff = constrain(micDiff, micMin, micMax);
          currentDimmerValue = map(safeDiff, micMin, micMax, 10, 255);
        } else if (currentDimmerValue > 0) {
          currentDimmerValue = max(0, currentDimmerValue - 5);  // fade out
        }

        currentDimmerValue = constrain(currentDimmerValue, 0, 255);
        setDmx(dimmerChannel, currentDimmerValue);
        setDmx(whiteChannel, currentDimmerValue > 0 ? 255 : 0);
        setDmx(strobeChannel, currentDimmerValue > 0 ? 95 : 0);
        setDmx(redChannel, 0);
        setDmx(greenChannel, 0);

        // End of ACTIVE window
        if (currentTime - stateStartTime >= activeDuration) {
          Serial.println("ACTIVE window over. Entering BUSY state.");
          currentState = BUSY;
          stateStartTime = currentTime;

          // Reset lights
          setDmx(strobeChannel, 0);
          setDmx(whiteChannel, 0);
        }
        break;
      }

    case BUSY:
      setStatusBusy();
      if (currentTime - stateStartTime >= busyDuration) {
        Serial.println("BUSY time done. Back to READY.");
        currentState = READY;
        stateStartTime = currentTime;
      }
      break;
  }

  // Debug output
  static unsigned long lastPrint = 0;
  if (currentTime - lastPrint > 250) {
    lastPrint = currentTime;
    if (DEBUG) {
      Serial.println("micValue: " + String(micValue) + " | micBaseline: " + String(micBaseline) + " | micDiff: " + String(abs(micValue - micBaseline)) + " | state: " + (currentState == READY ? "READY" : currentState == ACTIVE ? "ACTIVE"
                                                                                                                                                                                                                                  : "BUSY")
                     + " | smokeOn: " + String(smokeOn));
    }
  }

  delay(10);
}
