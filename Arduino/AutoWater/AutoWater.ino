/*
  Система автополива Виктора Генадиевича
*/

const bool DEBUG = true;

// File names
const String LAST_WATERING_FILENAME = "lastwtr.txt";
const String VALUES_FILENAME        = "values.log";
const String LOG_FILENAME           = "main.log";

// Sensor limits
const int SENSOR_MIN                = 503;
const int SENSOR_MAX                = 338;
const int MOISTURE_MIN              = 10; // percents

// Libs
#include "RTClib.h"             // Библиотека часов реального времени
#include <SD.h>                 // Библиотека SD карты памяти
#include <LiquidCrystal_I2C.h>  // Библиотека ЖКД
#include <TimerOne.h>           // Прерывания по таймеру

// Modules
RTC_DS1307 rtc;                     // Часы DS1307. I2C адреса 0x50, 0x68
const int CS_SD_PIN = 10;           // Контакт Chip Select для SD
LiquidCrystal_I2C lcd(0x27, 20, 4); // ЖДК
const int SENSOR_0 = A0;            // Сенсор влажности почвы
const int SENSOR_1 = A1;            // Сенсор влажности почвы
const int STATE_SER   = 7;          // Регистр статуса
const int STATE_LATCH = 8;          // Регистр статуса
const int STATE_CLK   = 9;          // Регистр статуса
const int PUMP_PIN    = 6;          // Помпа

bool sdEnabled;

// Values
DateTime now = DateTime(__DATE__, __TIME__);
DateTime lastWateringDateTime = DateTime(__DATE__, __TIME__);
int moisture;
byte state = 0; // 00000000 000000 RTCCheck RTCEnabled

// Files
File logFile;
File valuesFile;
File lastWateringFile;


void setup() {
  setPinsModes();
  initModules();
  setLastWateringDateTime();
  showStartState();
}


void loop() {
  now = getNow();
  TimeSpan minWateringDistance = getMinWateringDistance();
  moisture = readSensor(SENSOR_0);

  if (now - minWateringDistance >= lastWateringDateTime && moisture <= MOISTURE_MIN) {
    watering();
    updateLastWateringDateTime();
  }

  showLastWateringDateTime();
  showMoisture();
  showState();
  showNow();

  if (millis() > 86400000) state = state | 2;
}


void setPinsModes() {
  pinMode(CS_SD_PIN, OUTPUT);
  pinMode(STATE_SER, OUTPUT);
  pinMode(STATE_LATCH, OUTPUT);
  pinMode(STATE_CLK, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
}


void initModules() {
  initLCD();
  initSD();
  initRTC();
  initSensors();
  initState();
  initPump();
}


void initLCD() {
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("   Auto watering    ");
  lcd.setCursor(0, 2);
  lcd.print("Initialize...");
  delay(2000);
  lcd.clear();
}


void initRTC() {
  lcd.setCursor(0, 0);
  lcd.print("Initializing RTC:   ");
  lcd.setCursor(0, 1);
  state = state | ! rtc.begin();
  if (state & 1) {
    lcd.print("BAD");
  } else {
    lcd.print("OK");
    lcd.setCursor(0, 2);
    lcd.print("RTC time: ");
    if (! rtc.isrunning()) {
      lcd.print("Stopped");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    } else {
      lcd.print("Running");
    }
    lcd.setCursor(0, 3);
    lcd.print("RTC running");
  }
  delay(2000);
  lcd.clear();
}


void initSD() {
  sdEnabled = SD.begin(CS_SD_PIN);
  if (! sdEnabled) {
    if (DEBUG) {
      Serial.println("Could not find SD-card");
    }
  } else {
    if (DEBUG) {
      Serial.println("SD-card was find and initialized");
    }
  }
}


void initSensors() {
  delay(50);  // Ожидание стабилизации генератора импульсов датчика
}


void initState() {
  digitalWrite(STATE_LATCH, LOW);
  shiftOut(STATE_SER, STATE_CLK, MSBFIRST, 0);
  digitalWrite(STATE_LATCH, HIGH);
  delay(10);
  digitalWrite(STATE_LATCH, LOW);
}


void initPump() {
  digitalWrite(PUMP_PIN, LOW);
}


DateTime getNow() {
  if (state & 1) {
    now = DateTime(__DATE__, __TIME__) + TimeSpan(0, 0, 0, millis()/1000);
  } else {
    now = rtc.now();
  }
  return now;
}


void showStartState() {
  lcd.setCursor(0, 0);
  lcd.print("   Auto watering    ");
  lcd.setCursor(0, 2);
  lcd.print("SD card: ");
  if (sdEnabled) {
    lcd.print("OK");
  } else {
    lcd.print("BAD");
  }
  lcd.setCursor(0, 3);
  lcd.print("Sensors: ");
  lcd.print(analogRead(SENSOR_0));
  lcd.print(' ');
  lcd.print(analogRead(SENSOR_1));
  delay(10000);
  lcd.setCursor(0, 2);
  lcd.print("                    ");
}


void showNow() {
  lcd.setCursor(0, 3);
  
  // Show date
  if (now.day() < 10) lcd.print('0');
  lcd.print(now.day()); lcd.print('.');
  if (now.month() < 10) lcd.print('0');
  lcd.print(now.month()); lcd.print('.');
  lcd.print(now.year());

  // Empty space
  lcd.print("  ");

  // Show time
  if (now.hour() < 10) lcd.print('0');
  lcd.print(now.hour()); lcd.print(':');
  if (now.minute() < 10) lcd.print('0');
  lcd.print(now.minute()); lcd.print(':');
  if (now.second() < 10) lcd.print('0');
  lcd.print(now.second());
}


void showLastWateringDateTime() { 
  lcd.setCursor(0, 0);
  lcd.print("Watering: ");
  
  // Show date
  if (lastWateringDateTime.day() < 10) lcd.print('0');
  lcd.print(lastWateringDateTime.day()); lcd.print('.');
  if (lastWateringDateTime.month() < 10) lcd.print('0');
  lcd.print(lastWateringDateTime.month()); lcd.print('.');
  lcd.print(lastWateringDateTime.year());

  if (DEBUG) {
    lcd.setCursor(10, 0);
    if (lastWateringDateTime.hour() < 10) lcd.print('0');
    lcd.print(lastWateringDateTime.hour()); lcd.print(':');
    if (lastWateringDateTime.minute() < 10) lcd.print('0');
    lcd.print(lastWateringDateTime.minute()); lcd.print(':');
    if (lastWateringDateTime.second() < 10) lcd.print('0');
    lcd.print(lastWateringDateTime.second()); lcd.print("  ");
  }
}


void showMoisture() {
  lcd.setCursor(0, 1);
  lcd.print("Moisture:      ");
  if (moisture < 100) lcd.print(' ');
  if (moisture < 10)  lcd.print(' ');
  lcd.print(moisture);
  lcd.print(" %");
}


void showState() {
  lcd.setCursor(0, 2);
  lcd.print("State:      ");
  lcd.print(bool(state & 128));
  lcd.print(bool(state & 64));
  lcd.print(bool(state & 32));
  lcd.print(bool(state & 16));
  lcd.print(bool(state & 8));
  lcd.print(bool(state & 4));
  lcd.print(bool(state & 2));
  lcd.print(bool(state & 1));
}


void setLastWateringDateTime() {
  if (sdEnabled) {
    lastWateringFile = SD.open(LAST_WATERING_FILENAME);
    if (lastWateringFile) {
      if (lastWateringFile.available() > 0) {
        int day = lastWateringFile.parseInt();
        int month = lastWateringFile.parseInt();
        int year = lastWateringFile.parseInt();
        int hour = lastWateringFile.parseInt();
        int minute = lastWateringFile.parseInt();
        int second = lastWateringFile.parseInt();
        if (DEBUG) {
          Serial.print("Last watering datetime: ");
          Serial.print(day); Serial.print('.');
          Serial.print(month); Serial.print('.');
          Serial.print(year); Serial.print(' ');
          Serial.print(hour); Serial.print(':');
          Serial.print(minute); Serial.print(':');
          Serial.print(second);
        }

        lastWateringDateTime = DateTime(year, month, day, hour, minute, second);
      }
    }
  }
}


void updateLastWateringDateTime() {
  lastWateringDateTime = now;
  SD.remove(LAST_WATERING_FILENAME);
  lastWateringFile = SD.open(LAST_WATERING_FILENAME, FILE_WRITE);
  if (lastWateringFile) {
    lastWateringFile.print(lastWateringDateTime.day());    lastWateringFile.print('.');
    lastWateringFile.print(lastWateringDateTime.month());  lastWateringFile.print('.');
    lastWateringFile.print(lastWateringDateTime.year());   lastWateringFile.print(' ');
    lastWateringFile.print(lastWateringDateTime.hour());   lastWateringFile.print(':');
    lastWateringFile.print(lastWateringDateTime.minute()); lastWateringFile.print(':');
    lastWateringFile.println(lastWateringDateTime.second());
    lastWateringFile.close();
  }
}


TimeSpan getMinWateringDistance() {
  TimeSpan minWateringDistance;
  if (state & 1) {
    minWateringDistance = TimeSpan(7, 0, 0, 0);
  } else {
    if (now.month() > 3 && now.month() < 11) {
      minWateringDistance = TimeSpan(7, 0, 0, 0);
    } else {
      minWateringDistance = TimeSpan(30, 0, 0, 0);
    }
    if (DEBUG) minWateringDistance = TimeSpan(0, 0, 2, 0);
  }
  return minWateringDistance;
}


int readSensor(int pin) {
  int value = analogRead(pin);
  value = map(value, SENSOR_MIN, SENSOR_MAX, 0, 100);
  value = constrain(value, 0, 100);
  return value;
}


void watering() {
  digitalWrite(PUMP_PIN, HIGH);
  delay(1000);
  digitalWrite(PUMP_PIN, LOW);
}