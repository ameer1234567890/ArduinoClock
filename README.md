# ArduinoClock
A clock in Arduino with a Seven Segment and an RTC

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

#### Libraries used
* Time: https://github.com/PaulStoffregen/Time
* DS1307RTC: https://github.com/PaulStoffregen/DS1307RTC
* SevSeg: https://github.com/DeanIsMe/SevSeg
