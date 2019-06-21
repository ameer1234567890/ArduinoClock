#include <Wire.h>
#include <RTClib.h>
#include <ShiftDisplay.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include "Secrets.h"

/*
Secrets.h file should contain data as below:
#define STASSID "your-ssid" // your network SSID (name)
#define STAPSK "your-password" // your network password
*/
#ifndef STA_SSID
#define STA_SSID "your-ssid" // your network SSID (name)
#define STA_PSK  "your-password" // your network password
#define OTA_HOSTNAME "ArduinoClock" // desired hostname for OTA
#endif

const char* ssid = STA_SSID;
const char* pass = STA_PSK;

/* Configurable variables */
#define TICK_PIN D5
#define SYNC_PIN D0
#define DATA_PIN D6
#define LATCH_PIN D7
#define CLOCK_PIN D8
#define DISPLAY_SIZE 4 // number of digits
#define DISPLAY_TYPE COMMON_ANODE // either COMMON_ANODE or COMMON_CATHODE
const String TIMEZONE = "+5"; // local timezone
const int NTP_TIMEOUT = 5000; // NTP request timeout interval
const int SERVER_PORT = 8888; // Port number for server to initiate sync remotely
const int WIFI_TIMEOUT = 3000; // Wifi connect timeout interval
const bool LEADING_ZEROS = true; // true for leading zeros
const bool MILITARY_TIME = false; // true for 24 hour clock
const bool HOURLY_CHIME = true; // true for a chime at the start of every hour

/* Do not change unless you know what you are doing */
int ntpCount = 0;
int wifiCount = 0;
int chimeCount = 0;
bool chimed = false;
const int numChimes = 4;
unsigned long lastTime = 0;
const int ntpPacketSize = 48;
unsigned long previousMillis = 0;
byte packetBuffer[ntpPacketSize];
const unsigned int localPort = 2390;
const long intervalBetweenChimes = 200;
const char* ntpServerName = "pool.ntp.org";

WiFiUDP udp;
RTC_DS1307 rtc;
IPAddress timeServerIP;
ESP8266WebServer server(SERVER_PORT);
ShiftDisplay display(LATCH_PIN, CLOCK_PIN, DATA_PIN, DISPLAY_TYPE, DISPLAY_SIZE);


void setup() {
  pinMode(TICK_PIN, OUTPUT);
  pinMode(SYNC_PIN, INPUT_PULLUP);
  if (!rtc.begin()) {
    display.set("ERR1");
    while (true) {
      display.show();
    }
  }

  if (!rtc.isrunning()) {
    display.set("ERR2");
    while (true) {
      display.show();
    }
  }

  server.on("/", []() {
    server.send(400, "text/plain", "Bad request");
  });
  server.on("/sync", []() {
    server.send(200, "text/plain", "Sync started");
    syncntp();
  });
  server.on("/reboot", []() {
    server.send(200, "text/plain", "Rebooting clock");
    delay(1000);
    ESP.restart();
  });
  server.on("/countdown", countdown);
  server.begin();

  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.begin();
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    display.set(String(progress / (total / 100)));
    display.show();
  });
}


void loop() {
  String timeString;
  DateTime now = rtc.now();

  int hour = now.hour();
  if (hour > 12 && !MILITARY_TIME) {
    hour = hour - 12;
  }
  if (hour == 0 && !MILITARY_TIME) {
    hour = 12;
  }
  if (hour < 10 && LEADING_ZEROS) {
    timeString = "0" + String(hour);
  } else if (hour < 10 && !LEADING_ZEROS) {
    timeString = " " + String(hour);
  } else {
    timeString = String(hour);
  }

  int minute = now.minute();
  if (minute < 10 && LEADING_ZEROS) {
    timeString += "0" + String(minute);
  } else if (minute < 10 && !LEADING_ZEROS) {
    timeString += " " + String(minute);
  } else {
    timeString += String(minute);
  }

  if (minute == 0) {
    if (HOURLY_CHIME && !chimed) {
      if (chimeCount < numChimes) {
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= intervalBetweenChimes) {
          previousMillis = currentMillis;
          chimeCount++;
          tone(TICK_PIN, 1000, 70);
        }
      } else {
        chimed = true;
      }
    }
  } else {
    chimed = false;
  }

  display.set(timeString);
  if ((now.second() % 2) == 0) {
    display.setDot(1, true);
  }
  display.show();

  if (millis() - lastTime >= 1000 || !lastTime) {
    lastTime = millis();
    tone(TICK_PIN, 1000, 2);
  }

  if (digitalRead(SYNC_PIN) == LOW) {
    syncntp();
    pinMode(SYNC_PIN, OUTPUT);
    digitalWrite(SYNC_PIN, HIGH);
    pinMode(SYNC_PIN, INPUT_PULLUP);
  }

  server.handleClient();

  ArduinoOTA.handle();
}


void syncntp() {
  ntpCount++;
  if (ntpCount > NTP_TIMEOUT) {
    ntpCount = 0;
    int i = 0;
    while (i < 1000) {
      i++;
      display.set("ERR3");
      display.show();
    }
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
      wifiCount++;
      if (wifiCount > WIFI_TIMEOUT) {
        wifiCount = 0;
        int i = 0;
        while (i < 1000) {
          i++;
          display.set("ERR4");
          display.show();
        }
        return;
      } else {
        display.set("WIFI");
        display.show();
      }
    }
    udp.begin(localPort);
    WiFi.hostByName(ntpServerName, timeServerIP);
    sendNTPpacket(timeServerIP);
    int i = 0;
    while (i < 1000) {
      i++;
      display.set("SYNC");
      display.show();
    }
    int cb = udp.parsePacket();
    if (!cb) {
      int i = 0;
      while (i < 1000) {
        i++;
        display.set("ERR5");
        display.show();
      }
    } else {
      udp.read(packetBuffer, ntpPacketSize);
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
      unsigned long secsSince1900 = highWord << 16 | lowWord;
      int localTime;
      if (TIMEZONE.startsWith("+")) {
        localTime = secsSince1900 + (TIMEZONE.toInt() * 60 * 60);
      } else {
        localTime = secsSince1900 - (TIMEZONE.toInt() * 60 * 60);
      }
      rtc.adjust(DateTime(localTime));
      int i = 0;
      while (i < 1000) {
        i++;
        display.set("DONE");
        display.show();
      }
    }
  }
}


unsigned long sendNTPpacket(IPAddress& address) {
  memset(packetBuffer, 0, ntpPacketSize);
  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  udp.beginPacket(address, 123);
  udp.write(packetBuffer, ntpPacketSize);
  udp.endPacket();
}


void countdown() {
  if (server.arg("secs") == "") {
    server.send(400, "text/plain", "Countdown period not specified!");
  } else {
    server.send(200, "text/plain", "Countdown started");
    for (int secs = server.arg("secs").toInt(); secs > 0; secs--) {
      display.set(secs, ALIGN_RIGHT);
      tone(TICK_PIN, 1000, 2);
      display.show(1000);
    }
    display.set("GO", ALIGN_RIGHT);
    tone(TICK_PIN, 1000, 1000);
    display.show(3000);
  }
}
