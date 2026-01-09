/*
  Система автополива Виктора Генадиевича
*/

const bool DEBUG = true;

// File names
const String LAST_WATERING_FILENAME = "lastwtr.txt";
const String VALUES_FILENAME        = "values.log";
const String LOG_FILENAME           = "main.log";

// Sensor limits
const int SENSOR_MIN                = 338;
const int SENSOR_MAX                = 508;
const int MOISTURE_MIN              = 10; // percents

// Libs
#include "RTClib.h"             // Библиотека часов реального времени
#include <SD.h>                 // Библиотека SD карты памяти
#include <LiquidCrystal_I2C.h>  // Библиотека ЖКД

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

// Values
DateTime now = DateTime(__DATE__, __TIME__);
DateTime lastWateringDateTime = DateTime(__DATE__, __TIME__);
int sensor0;
int sensor1;
int moisture0;
int moisture1;
byte state = 0; // 00000000 00 (32)Sensor1 (16)Sensor0 (8)SDError (4)SDEnabled (2)RTCCheck (1)RTCEnabled

// Files
File logFile;
File valuesFile;
File lastWateringFile;


void setup() {
  setPinsModes();
  initModules();
  setLastWateringDateTime();
}


void loop() {
  now = getNow();
  TimeSpan minWateringDistance = getMinWateringDistance();
  readSensors();
  writeMoistures();

  if (now - minWateringDistance >= lastWateringDateTime && (moisture0+moisture1)/2 <= MOISTURE_MIN) {
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
  lcd.setCursor(0, 0);
  lcd.print("Initializing SD: ");
  lcd.setCursor(0, 1);
  state = state | ! int(SD.begin(CS_SD_PIN))*4;
  if (state & 4) {
    lcd.print("BAD");
  } else {
    lcd.print("OK");
  }
  delay(2000);
  lcd.clear();
}


void initSensors() {
  lcd.setCursor(0, 0);
  lcd.print("Initializing sensors.");
  lcd.setCursor(0, 1);
  lcd.print("Sleep 50 ");
  delay(50);  // Ожидание стабилизации генератора импульсов датчика
  lcd.print("OK");
  lcd.setCursor(0, 2);
  lcd.print("Sensors: ");

  sensor1 = analogRead(SENSOR_1);
  if (sensor1 < SENSOR_MIN || sensor1 > SENSOR_MAX) {
    lcd.print("BAD ");
    state = state | 32;
  } else {
    lcd.print(" OK ");
  }
  sensor0 = analogRead(SENSOR_0);
  if (sensor0 < SENSOR_MIN || sensor0 > SENSOR_MAX) {
    lcd.print("BAD ");
    state = state | 16;
  } else {
    lcd.print(" OK ");
  }

  lcd.setCursor(0, 3);
  lcd.print("Sensors raw: ");
  lcd.print(sensor1);
  lcd.print(' ');
  lcd.print(sensor0);

  delay(2000);
  lcd.clear();
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
  lcd.print("Moisture: ");
  
  if (! bool(state & 32)) {
    if (moisture1 < 100) lcd.print(' ');
    if (moisture1 < 10)  lcd.print(' ');
    lcd.print(moisture1);
    lcd.print("%");
  } else {
    lcd.print("BAD  ");
  }

  if (! bool(state & 16)) {
    if (moisture0 < 100) lcd.print(' ');
    if (moisture0 < 10)  lcd.print(' ');
    lcd.print(moisture0);
    lcd.print("% ");
  } else {
    lcd.print(" BAD ");
  }
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

  digitalWrite(STATE_LATCH, LOW);
  shiftOut(STATE_SER, STATE_CLK, MSBFIRST, state);
  digitalWrite(STATE_LATCH, HIGH);
  digitalWrite(STATE_LATCH, LOW);
}


void setLastWateringDateTime() {
  if (! state & 4) {
    lastWateringFile = SD.open(LAST_WATERING_FILENAME);
    if (lastWateringFile) {
      if (lastWateringFile.available() > 0) {
        int day = lastWateringFile.parseInt();
        int month = lastWateringFile.parseInt();
        int year = lastWateringFile.parseInt();
        int hour = lastWateringFile.parseInt();
        int minute = lastWateringFile.parseInt();
        int second = lastWateringFile.parseInt();
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


void readSensors() {
  sensor0 = analogRead(SENSOR_0);
  sensor1 = analogRead(SENSOR_1);
  if (sensor0 < SENSOR_MIN || sensor0 > SENSOR_MAX) {
    state = state | 16;
  } else {
    moisture0 = map(sensor0, SENSOR_MIN, SENSOR_MAX, 100, 0);
    moisture0 = constrain(moisture0, 0, 100);
  }
  if (sensor1 < SENSOR_MIN || sensor1 > SENSOR_MAX) {
    state = state | 32;
  } else {
    moisture1 = map(sensor1, SENSOR_MIN, SENSOR_MAX, 100, 0);
    moisture1 = constrain(moisture1, 0, 100);
  }
}


void watering() {
  digitalWrite(PUMP_PIN, HIGH);
  delay(1000);
  digitalWrite(PUMP_PIN, LOW);
}


void writeMoistures() {
  
}