[![Build Status](https://travis-ci.org/ameer1234567890/ArduinoClock.svg?branch=master)](https://travis-ci.org/ameer1234567890/ArduinoClock) [![Build status](https://ci.appveyor.com/api/projects/status/m8t5kg7jovf9r3ih/branch/master?svg=true)](https://ci.appveyor.com/project/ameer1234567890/arduinoclock/branch/master)

# ArduinoClock
A clock in Arduino with a Seven Segment, an RTC, ticking sound and NTP updates

#### Pinout for Seven Segment
```
74HC595 (SR1 & SR2)  -  Seven Segment
-------------------------------------
SR2 Q0               -  Digit 1
SR2 Q1               -  Digit 2
SR2 Q2               -  Digit 3
SR2 Q3               -  Digit 4
SR1 Q0               -  Segment A
SR1 Q1               -  Segment B
SR1 Q2               -  Segment C
SR1 Q3               -  Segment D
SR1 Q4               -  Segment E
SR1 Q5               -  Segment F
SR1 Q6               -  Segment G
SR1 Q7               -  Decimal Point
```

#### Seven Segment's pins are as below:
```
Top Row:    1 A F  2 3 B
Bottom Row: E D DP C G 4
```

#### Pinout for 74HC595 shift register SR1
```
Wemos D1 mini   -  74HC595
--------------------------
5V              -  VCC
5V              -  MR
GND             -  GND
GND             -  OE
D6              -  DS
D7              -  ST_CP
D8              -  SH_CP
DS pin of SR2   -  Q7'
```

#### Pinout for 74HC595 shift register SR2
```
Wemos D1 mini   -  74HC595
--------------------------
5V              -  VCC
5V              -  MR
GND             -  GND
GND             -  OE
Q7' pin of SR1  -  DS
D7              -  ST_CP
D8              -  SH_CP
```

#### Note: Add a 0.1uF ceramic capacitor for each shift register, between VCC and GND.

#### Pinout for RTC
```
Wemos D1 mini  -  RTC
---------------------
5V             -  VCC
GND            -  GND
SCL / D1       -  SCL
SDA / D2       -  SDA
```

#### Pinout for Buzzer (Ticking sound)
```
Wemos D1 mini  -  Buzzer
------------------------
D5             -  VCC
GND            -  GND
```

#### Pinout for Button (NTP sync)
```
Wemos D1 mini  -  Button
----------------------------
D0             -  Terminal 1
GND            -  Terminal 2
```

#### Libraries used
* RTClib: https://github.com/adafruit/RTClib
* ShiftDisplay: https://miguelpynto.github.io/ShiftDisplay/

#### Project Progress
* Initial Clock: https://web.facebook.com/ameer1234567890/posts/10210875516896895
* Ticking Sound: https://web.facebook.com/ameer1234567890/posts/10211233068995474
* Clock Hanging at Wall: https://web.facebook.com/ameer1234567890/posts/10211710798218406
