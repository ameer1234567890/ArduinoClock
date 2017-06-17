#include <SevSeg.h>
#include <DS1307RTC.h>
#include <Time.h>
#include <TimeLib.h>
#include <Wire.h>

// Create an instance of the object
SevSeg sevseg;
bool militaryTime = false; // true for 24 hour clock

void setup() {
  byte numDigits = 4;
  byte digitPins[] = {2, 3, 4, 5};
  byte segmentPins[] = {6, 7, 8, 9, 10, 11, 12, 13};
  bool resistorsOnSegments = false; // 'false' means resistors are on digit pins
  byte hardwareConfig = COMMON_ANODE; // See README.md for options
  bool updateWithDelays = false; // Default. Recommended
  bool leadingZeros = true; // Use 'true' if you'd like to keep the leading zeros
  
  sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins, resistorsOnSegments, updateWithDelays, leadingZeros);
  sevseg.setBrightness(90);
}

void loop() {
  tmElements_t tm;
  int time;
  int dot;

  if (RTC.read(tm)) {
    time = tm.Hour * 100;
    if (time > 1200 && militaryTime == false) {
      time = time - 1200;
    }
    if (time == 0 && militaryTime == false) {
      time = 1200;
    }
    time += tm.Minute;
  }

  if ((tm.Second % 2) == 0) {
    dot = 4;
  } else {
    dot = 2;
  }

  //Produce an output on the display
  sevseg.refreshDisplay();
  sevseg.setNumber(time, dot);
}
