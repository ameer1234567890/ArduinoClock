#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <ShiftDisplay2.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <ESP8266httpUpdate.h>
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
#define DATA_PIN D6
#define LATCH_PIN D7
#define CLOCK_PIN D8
#define BUTTON_PIN D0
#define DISPLAY_SIZE 4 // number of digits
#define DISPLAY_TYPE COMMON_ANODE // either COMMON_ANODE or COMMON_CATHODE
const int ALARM_BEEPS = 10; // number of beeps during alarm
const int SERVER_PORT = 80; // Port number for server to initiate sync remotely
const String TIMEZONE = "+5"; // local timezone
const int INIT_SYNC = 1000 * 30; // NTP sync on startup, after 30 seconds
const bool AUTO_SYNC = true; // true to update clock via NTP automatically
const int NTP_TIMEOUT = 5000; // NTP request timeout interval
const int WIFI_TIMEOUT = 3000; // Wifi connect timeout interval
const bool HOURLY_CHIME = false; // true for a chime at the start of every hour
const bool LEADING_ZEROS = true; // true for leading zeros
const bool MILITARY_TIME = false; // true for 24 hour clock
const String OTA_URL = "http://192.168.100.44/ArduinoClock/ArduinoClock.ino.bin"; // HTTP OTA update URL

/* Do not change unless you know what you are doing */
String logMsg;
String timeString;
int wifiCount = 0;
int chimeCount = 0;
uint eepromAddr = 0;
bool synced = false;
bool chimed = false;
bool alarmed = false;
const int numChimes = 4;
bool dismissAlarm = false;
bool stopCountdown = false;
unsigned long lastTime = 0;
const int ntpPacketSize = 48;
unsigned long previousMillis = 0;
byte packetBuffer[ntpPacketSize];
unsigned long lastTimeInitSync = 0;
const unsigned int localPort = 2390;
const long intervalBetweenChimes = 200;
String logTime = "00/00/0000 00:00:00";
const char* ntpServerName = "pool.ntp.org";
typedef struct { 
  uint set = 0;
  uint hour = 0;
  uint minute = 0;
} eepromData;
eepromData alarmData;
eepromData alarmDataOld;

WiFiUDP udp;
RTC_DS1307 rtc;
WiFiClient wClient;
IPAddress timeServerIP;
ESP8266WebServer server(SERVER_PORT);
ShiftDisplay2 display(LATCH_PIN, CLOCK_PIN, DATA_PIN, DISPLAY_TYPE, DISPLAY_SIZE);


void setup() {
  log("I/system: startup");
  pinMode(TICK_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  if (digitalRead(BUTTON_PIN) == LOW) {
    runHTTPUpdate();
  }
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
    server.send(200, "text/html", \
      "<a href=\"/log\">/log</a><br>"\
      "<a href=\"/sync\">/sync</a><br>"\
      "<a href=\"/reboot\">/reboot</a><br>"\
      "<a href=\"/countdown?secs=10\">/countdown?secs=10</a><br>"\
      "<a href=\"/stopcountdown\">/stopcountdown</a><br>"\
      "<a href=\"/setalarm?hour=14&minute=44\">/setalarm?hour=14&minute=44</a><br>"\
      "<a href=\"/showalarm\">/showalarm</a><br>"\
      "<a href=\"/cancelalarm\">/cancelalarm</a><br>"\
      "<a href=\"/dismissalarm\">/dismissalarm</a><br>"\
      "<a href=\"/otaupdate\">/otaupdate</a><br>"\
      "<br><p><small>"\
      "Powered by: <a href=\"https://github.com/ameer1234567890/ArduinoClock\">ArduinoClock</a> | "\
      "Chip ID: " + String(ESP.getChipId()) + \
      "</small></p>"\
    );
    log("I/server: served / to " + server.client().remoteIP().toString());
  });
  server.on("/log", []() {
    log("I/server: served /log to " + server.client().remoteIP().toString());
    server.send(200, "text/plain", logMsg);
  });
  server.on("/sync", []() {
    server.send(200, "text/plain", "Sync started");
    log("I/system: running clock sync routine upon request from " + server.client().remoteIP().toString());
    syncntp();
  });
  server.on("/reboot", []() {
    server.send(200, "text/plain", "Rebooting clock");
    log("I/system: rebooting upon request from " + server.client().remoteIP().toString());
    delay(1000);
    ESP.restart();
  });
  server.on("/countdown", countdown);
  server.on("/stopcountdown", []() {
    server.send(200, "text/plain", "Stopping countdown");
    log("I/system: stopping countdown upon request from " + server.client().remoteIP().toString());
    stopCountdown = true;
  });
  server.on("/setalarm", setAlarm);
  server.on("/showalarm", showAlarm);
  server.on("/cancelalarm", cancelAlarm);
  server.on("/dismissalarm", []() {
    server.send(200, "text/plain", "Dismissing alarm");
    log("I/system: dismissing alarm upon request from " + server.client().remoteIP().toString());
    dismissAlarm = true;
  });
  server.on("/otaupdate", []() {
    server.send(200, "text/plain", "HTTP OTA update started");
    log("I/system: ota update started upon request from " + server.client().remoteIP().toString());
    runHTTPUpdate();
  });
  server.begin();

  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.begin();
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    display.set(String(progress / (total / 100)), ALIGN_RIGHT);
    display.show();
  });
  ArduinoOTA.onError([](ota_error_t error) {
    log("E/ota   : ota failed: " + String(error));
  });

  EEPROM.begin(512);
  EEPROM.get(eepromAddr, alarmData);
}


void loop() {
  DateTime now = rtc.now();
  int day = now.day();
  int month = now.month();
  int year = now.year();
  int hour = now.hour();
  int minute = now.minute();
  int second = now.second();

  if (alarmData.set == 1 && hour == alarmData.hour && minute == alarmData.minute) {
    if (!alarmed) {
      doAlarm();
      alarmed = true;
    }
  } else {
    alarmed = false;
    dismissAlarm = false; // this is to avoid dismissal before the alarm starts
  }

  if (day < 10) {
    logTime = "0" + String(day) + "/";
  } else {
    logTime = String(day) + "/";
  }
  if (month < 10) {
    logTime += "0" + String(month) + "/";
  } else {
    logTime += String(month) + "/";
  }
  logTime += String(year) + " ";
  if (hour < 10) {
    logTime += "0" + String(hour) + ":";
  } else {
    logTime += String(hour) + ":";
  }
  if (minute < 10) {
    logTime += "0" + String(minute) + ":";
  } else {
    logTime += String(minute) + ":";
  }
  if (second < 10) {
    logTime += "0" + String(second);
  } else {
    logTime += String(second);
  }

  if (hour > 12 && !MILITARY_TIME) {
    hour = hour - 12;
  }
  if (hour == 0 && !MILITARY_TIME) {
    hour = 12;
  }

  timeString = getTimeString(hour, minute);

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

  if (millis() > INIT_SYNC && !synced && AUTO_SYNC && millis() > (lastTimeInitSync + INIT_SYNC)) {
    lastTimeInitSync = millis();
    log("I/system: running auto sync via NTP");
    syncntp();
  }

  display.set(timeString);
  if ((now.second() % 2) == 0) {
    display.setDot(1, true);
  }
  display.show();

  if (millis() - lastTime >= 1000 || !lastTime) {
    lastTime = millis();
    tone(TICK_PIN, 500, 2);
  }

  if (digitalRead(BUTTON_PIN) == LOW) {
    syncntp();
    debounce();
  }

  server.handleClient();

  ArduinoOTA.handle();

  stopCountdown = false; // this is to avoid countdown being unable to start
}


void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    wifiCount++;
    if (wifiCount > WIFI_TIMEOUT) {
      wifiCount = 0;
      log("E/ntp   : wifi timeout");
      display.set("ERR4");
      display.show(3000);
      return;
    } else {
      display.set("WIFI");
      display.show();
    }
  }
  log("I/wifi  : WiFi connected. IP address: " + WiFi.localIP().toString());
}


void log(String msg) {
  logMsg = logMsg + "[" + logTime + "] ";
  logMsg = logMsg + msg + "\n";
}


void syncntp() {
  setupWifi();
  log("I/ntp   : clock update started");
  udp.begin(localPort);
  WiFi.hostByName(ntpServerName, timeServerIP);
  sendNTPpacket(timeServerIP);
  display.set("SYNC");
  display.show(3000);
  int cb = udp.parsePacket();
  if (!cb) {
    log("E/ntp   : packet empty");
    display.set("ERR5");
    display.show(3000);
  } else {
    udp.read(packetBuffer, ntpPacketSize);
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    unsigned long unixEpoch = secsSince1900 - 2208988800;
    unsigned long localTime;
    if (TIMEZONE.startsWith("+")) {
      localTime = unixEpoch + (TIMEZONE.toInt() * 60 * 60);
    } else {
      localTime = unixEpoch - (TIMEZONE.toInt() * 60 * 60);
    }
    rtc.adjust(DateTime(localTime));
    synced = true;
    log("I/ntp   : clock updated");
    display.set("DONE");
    display.show(3000);
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


String getTimeString(uint hour, uint minute) {
  if (hour < 10 && LEADING_ZEROS) {
    timeString = "0" + String(hour);
  } else if (hour < 10 && !LEADING_ZEROS) {
    timeString = " " + String(hour);
  } else {
    timeString = String(hour);
  }
  if (minute < 10 && LEADING_ZEROS) {
    timeString += "0" + String(minute);
  } else if (minute < 10 && !LEADING_ZEROS) {
    timeString += " " + String(minute);
  } else {
    timeString += String(minute);
  }
  return timeString;
}


void debounce() {
  // de-bouce by re-setting the pin
  pinMode(BUTTON_PIN, OUTPUT);
  digitalWrite(BUTTON_PIN, HIGH);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}


void runHTTPUpdate() {
  log("I/updatr: HTTP OTA update started");
  display.set("OTA ");
  display.show(3000);
  setupWifi();
  ESPhttpUpdate.rebootOnUpdate(false);
  display.set("UPDT");
  display.show(1000);
  HTTPUpdateResult ret = ESPhttpUpdate.update(wClient, OTA_URL);
  switch(ret) {
    case HTTP_UPDATE_FAILED:
      log("E/updatr: HTTP OTA update failed");
      display.set("ERR6");
      display.show(3000);
      break;
    case HTTP_UPDATE_OK:
      log("I/updatr: HTTP OTA update completed. reboot required!");
      display.set("DONE");
      display.show(3000);
      break;
  }
  debounce();
}


void countdown() {
  if (server.arg("secs") == "" && server.arg("mins") == "") {
    server.send(400, "text/plain", "Countdown period not specified!");
    log("I/ctdown: no period specified. request from " + server.client().remoteIP().toString());
  } else {
    log("I/ctdown: started countdown upon request from " + server.client().remoteIP().toString());
    server.send(200, "text/plain", "Countdown started");
    int secs = (server.arg("mins").toInt() * 60) + server.arg("secs").toInt();
    for (secs; secs > 0; secs--) {
      if (secs <= 60) {
        display.set(secs, ALIGN_RIGHT);
      } else {
        display.set(getTimeString(abs(secs / 60), secs % 60));
        display.setDot(1, true);
      }
      tone(TICK_PIN, 1000, 100);
      display.show(1000);
      if (digitalRead(BUTTON_PIN) == LOW || stopCountdown) {
        stopCountdown = true;
        log("I/ctdown: countdown stopped");
        break;
      }
      server.handleClient();
      ArduinoOTA.handle();
    }
    if (stopCountdown == true) {
      stopCountdown = false;
      display.set("STOP");
      display.show(2000);
      debounce();
    } else {
      display.set("GO", ALIGN_RIGHT);
      tone(TICK_PIN, 1000, 1000);
      display.show(1000);
      display.set("");
      display.show(500);
      display.set("GO", ALIGN_RIGHT);
      tone(TICK_PIN, 1000, 1000);
      display.show(1000);
      log("I/ctdown: completed for " + String(secs) + " seconds");
    }
  }
}


void setAlarm() {
  log("I/alarm : started setAlarm upon request from " + server.client().remoteIP().toString());
  if (server.arg("hour") == "" || server.arg("minute") == "") {
    server.send(400, "text/plain", "Alarm time not specified!");
  } else {
    int alarmHour = server.arg("hour").toInt();
    int alarmMinute = server.arg("minute").toInt();
    alarmData.set = 1;
    alarmData.hour = alarmHour;
    alarmData.minute = alarmMinute;
    EEPROM.get(eepromAddr, alarmDataOld);
    if (alarmDataOld.set == alarmData.set && alarmDataOld.hour == alarmData.hour && alarmDataOld.minute == alarmData.minute) {
      server.send(200, "text/plain", "Alarm already set for " + String(alarmData.hour) + ":" + String(alarmData.minute));
    } else {
      EEPROM.put(eepromAddr, alarmData);
      EEPROM.commit();
      server.send(200, "text/plain", "Alarm set for " + String(alarmData.hour) + ":" + String(alarmData.minute));
    }
    timeString = getTimeString(alarmData.hour, alarmData.minute);
    display.set("ALRM");
    display.show(1000);
    display.set(timeString);
    display.setDot(1, true);
    display.show(3000);
    display.set("DONE");
    display.show(1000);
  }
  log("I/alarm : completed setAlarm");
}


void showAlarm() {
  log("I/alarm : started showAlarm upon request from " + server.client().remoteIP().toString());
  if (alarmData.set == 0) {
    server.send(200, "text/plain", "No alarm set");
    display.set("ALRM");
    display.show(1000);
    display.set("NONE");
    display.show(3000);
  } else {
    EEPROM.get(eepromAddr, alarmData);
    server.send(200, "text/plain", "Alarm is set for " + String(alarmData.hour) + ":" + String(alarmData.minute));
    timeString = getTimeString(alarmData.hour, alarmData.minute);
    display.set("ALRM");
    display.show(1000);
    display.set(timeString);
    display.setDot(1, true);
    display.show(3000);
    display.set("DONE");
    display.show(1000);
  }
  log("I/alarm : completed showAlarm");
}


void cancelAlarm() {
  log("I/alarm : started cancelAlarm upon request from " + server.client().remoteIP().toString());
  if (EEPROM.read(eepromAddr) == 0) {
    server.send(200, "text/plain", "No alarm set");
    display.set("ALRM");
    display.show(1000);
    display.set("NONE");
    display.show(3000);
  } else {
    EEPROM.write(0, 0);
    EEPROM.commit();
    alarmData.set = 0;
    server.send(200, "text/plain", "Alarm cancelled");
    display.set("ALRM");
    display.show(1000);
    display.set("CXLD");
    display.show(3000);
  }
  log("I/alarm : completed cancelAlarm");
}


void doAlarm() {
  log("I/alarm : started doAlarm");
  for (int i = 0; i < ALARM_BEEPS; i++) {
    if (digitalRead(BUTTON_PIN) == LOW || dismissAlarm) {
      dismissAlarm = false;
      log("I/alarm : alarm dismissed");
      break;
    }
    server.handleClient();
    ArduinoOTA.handle();
    display.set("ALRM");
    display.show(1000);
    display.set(timeString);
    display.setDot(1, true);
    tone(TICK_PIN, 1000, 1000);
    display.show(1000);
  }
  debounce();
  delay(3000);
  log("I/alarm : completed doAlarm");
}
