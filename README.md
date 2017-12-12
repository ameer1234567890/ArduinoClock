# ArduinoClock
A clock in Arduino with a Seven Segment and an RTC and ticking sound

#### Pinout for Seven Segment
```
Arduino Uno  -  Seven Segment
-----------------------------
D2           -  Digit 1
D3           -  Digit 2
D4           -  Digit 3
D5           -  Digit 4
D6           -  Segment A
D7           -  Segment B
D8           -  Segment C
D9           -  Segment D
D10          -  Segment E
D11          -  Segment F
D12          -  Segment G
D13          -  Decimal Point
```

#### Pinout for RTC
```
Arduino Uno  -  RTC
-------------------
5V           -  VCC
GND          -  GND
SCL / A5     -  SCL
SDA / A4     -  SDA
```

#### Pinout for Buzzer (Ticking sound)
```
Arduino Uno  -  Buzzer
-------------------
A0           -  VCC
GND          -  GND
```

#### Libraries used
* Time: https://github.com/PaulStoffregen/Time
* DS1307RTC: https://github.com/PaulStoffregen/DS1307RTC
* SevSeg: https://github.com/DeanIsMe/SevSeg

#### Project Progress
* Initial Clock: https://web.facebook.com/ameer1234567890/posts/10210875516896895
* Ticking Sound: https://web.facebook.com/ameer1234567890/posts/10211233068995474
* Clock Hanging at Wall: https://web.facebook.com/ameer1234567890/posts/10211710798218406
