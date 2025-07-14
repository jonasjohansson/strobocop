#include <DmxSimple.h>

const uint8_t dmxPin = 4;

const uint8_t dimmerChannel = 1;
const uint8_t strobeChannel = 2;
const uint8_t redChannel = 3;
const uint8_t greenChannel = 4;
const uint8_t whiteChannel = 6;

bool first = true;

void setDmx(uint8_t channel, uint8_t value) {
  DmxSimple.write(channel, value);
  Serial.print("Channel ");
  Serial.print(channel);
  Serial.print(" set to ");
  Serial.println(value);
}

void setup() {
  Serial.begin(9600);
  DmxSimple.usePin(dmxPin);
  DmxSimple.maxChannel(12);

  // Ensure all DMX channels start at 0
}

void loop() {

  if (first) {
    delay(2000);
    first = false;
    setDmx(1, 255);
    setDmx(2, 64);
    setDmx(3, 255);
    setDmx(4, 0);
    setDmx(5, 0);
  }
}
