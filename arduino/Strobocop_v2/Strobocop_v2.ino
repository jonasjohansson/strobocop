#include <DmxSimple.h>

const uint8_t dmxPin = 4;
const uint8_t micPin = A0;

const uint8_t dimmerChannel = 1;
const uint8_t shutterChannel = 2;
const uint8_t redChannel = 3;
const uint8_t greenChannel = 4;
const uint8_t blueChannel = 5;
const uint8_t whiteChannel = 6;
const uint8_t focusChannel = 7;
const uint8_t colorTempChannel = 9;
const uint8_t smokeChannel = 64;

// Thresholds for micDiff
const int micThreshold = 16;
const int micMin = 10;
const int micMax = 60;

const unsigned long smokeDuration = 2000;
const unsigned long activeDuration = 5000;
const unsigned long busyDuration = 15000;

enum State { READY, ACTIVE, BUSY };
State currentState = READY;

unsigned long stateStartTime = 0;
unsigned long smokeStartTime = 0;
unsigned long lastAutoSmokeTime = 0;
unsigned long autoSmokeEndTime = 0;

bool smokeOn = false;

int micBaseline = 0;

const bool DEBUG = true;

void setDmx(uint8_t channel, uint8_t value) {
  DmxSimple.write(channel, value);
}

void setStatusReady() {
  setDmx(dimmerChannel, 255);
  setDmx(whiteChannel, 0);
  setDmx(shutterChannel, 32);
  setDmx(greenChannel, 128);
  setDmx(redChannel, 70);
  setDmx(colorTempChannel, 0);  // highest kelvin (coolest)
}

void setStatusBusy() {
  setDmx(dimmerChannel, 255);
  setDmx(whiteChannel, 0);
  setDmx(shutterChannel, 32);
  setDmx(colorTempChannel, 16);
}

void setup() {
  Serial.begin(9600);
  DmxSimple.usePin(dmxPin);
  DmxSimple.maxChannel(64);

  // Init DMX
  setDmx(smokeChannel, 0);
  setDmx(dimmerChannel, 0);
  setDmx(whiteChannel, 0);
  setDmx(shutterChannel, 0);
  setDmx(colorTempChannel, 255);
  setDmx(focusChannel, 255);  // set focus once

  // Initial state
  setStatusReady();
  currentState = READY;
  stateStartTime = millis();
  lastAutoSmokeTime = millis();
  micBaseline = analogRead(micPin);
}

void loop() {
  unsigned long currentTime = millis();

  // Smooth baseline
  int micValue = analogRead(micPin);
  int micDiff = abs(micValue - micBaseline);

  if (DEBUG) {
    Serial.println("micValue: " + String(micValue) +
                   " | micBaseline: " + String(micBaseline) +
                   " | micDiff: " + String(micDiff));
  }

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

    case ACTIVE: {
      // Stop manual smoke
      if (smokeOn && (currentTime - smokeStartTime >= smokeDuration)) {
        setDmx(smokeChannel, 0);
        smokeOn = false;
      }

      static uint8_t currentDimmerValue = 0;
      if (micDiff > micMin) {
        int safeDiff = constrain(micDiff, micMin, micMax);
        currentDimmerValue = map(safeDiff, micMin, micMax, 10, 255);

        // Color temp: screaming = low kelvin = low value
        uint8_t tempValue = map(safeDiff, micMin, micMax, 255, 16);
        setDmx(colorTempChannel, tempValue);
      } else if (currentDimmerValue > 0) {
        currentDimmerValue = max(0, currentDimmerValue - 5);
      }

      currentDimmerValue = constrain(currentDimmerValue, 0, 255);
      setDmx(dimmerChannel, currentDimmerValue);
      setDmx(whiteChannel, currentDimmerValue > 0 ? 255 : 0);
      setDmx(shutterChannel, currentDimmerValue > 0 ? 95 : 0);  // Pulsate if active

      if (currentTime - stateStartTime >= activeDuration) {
        Serial.println("ACTIVE over. Entering BUSY.");
        currentState = BUSY;
        stateStartTime = currentTime;

        // Turn off visual effects
        setDmx(shutterChannel, 0);
        setDmx(whiteChannel, 0);
        setDmx(dimmerChannel, 0);
      }
      break;
    }

    case BUSY:
      setStatusBusy();
      if (currentTime - stateStartTime >= busyDuration) {
        Serial.println("BUSY over. Back to READY.");
        currentState = READY;
        stateStartTime = currentTime;
      }
      break;
  }

  delay(10);
}
