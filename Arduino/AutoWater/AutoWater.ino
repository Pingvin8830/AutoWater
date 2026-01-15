/*
  Система автополива Виктора Генадиевича

  Планы:
  - кнопки подстройки времени/управления
  - ребут?
  - имена логов?
  - корпус
  - плата
  - check SD files
  - звук?
  - !!! ПАМЯТЬ !!!
*/

#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <SD.h>

#include "states.h"
#include "chars.h"
#include "positions.h"
#include "limits.h"
#include "filenames.h"
#include "types.h"
#include "hards.h"
#include "details.h"

const int SENSORS_COUNT = 2;

LiquidCrystal_I2C lcd(0x27, 20, 4);
RTC_DS1307 rtc;
const byte RTC_CONFIRM_PIN   = 3;
const byte SD_CS_PIN         = 10;
const byte SENSORS_PINS[]    = {A0, A1};
const byte SENSORS_POWER_PIN = 5;
const byte PUMP_PIN          = 6;
const byte STATE_SER_PIN     = 7;
const byte STATE_LATCH_PIN   = 8;
const byte STATE_CLK_PIN     = 9;

DateTime now            = DateTime(__DATE__, __TIME__);
DateTime lastRTCCorrect = DateTime(__DATE__, __TIME__);
DateTime lastWatering   = DateTime(__DATE__, __TIME__);
DateTime pumpStart      = DateTime(__DATE__, __TIME__);

byte nowB[14];
byte lastWateringB[14];

byte state = 0;

int sensorsValues[SENSORS_COUNT];
bool isMeasured = false;
byte moistures[SENSORS_COUNT+1];


void setup() {
  initLCD();
  initRTC();
  initSD();
  setLastRTCCorrects();
  setLastWaterings();
  initSensors();
  initPump();
  initState();

  writeLog(TYPE_INIT, HARD_ALL, DETAIL_FINISHED);
}

void loop() {
  setNows();

  if (bool(state & MASK_RTC_ERR) && ! digitalRead(RTC_CONFIRM_PIN)) updateLastRTCCorrect();

  if (now.minute() == 0 && now.second() == 0) {
    if (! isMeasured) {
      measure();
      isMeasured = true;
      writeMeasures();
      updateAverageMoisture();
    }
  } else isMeasured = false;

  if (! bool(state & MASK_PUMP_ENABLE) && now >= lastWatering + getWateringTimeSpan((byte)now.month()) && moistures[SENSORS_COUNT] < MOISTURE_MIN) {
    enablePump();
  }
  if (bool(state & MASK_PUMP_ENABLE) && now - PUMP_WORKTIME == pumpStart) {
    disablePump();
    updateLastWaterings();
  }

  updateState();

  flushLCD();
}

void initLCD() {
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0); lcd.print("Watering: dd.mm.YYYY");
  lcd.setCursor(0, 1); lcd.print("Moistures: xxx% xxx%");
  lcd.setCursor(0, 2); lcd.print("State:      BBBBBBBB");
  lcd.setCursor(0, 3); lcd.print("dd.mm.YYYY  HH:MM:SS");
}

void initRTC() {
  pinMode(RTC_CONFIRM_PIN, INPUT_PULLUP);
  if (! rtc.begin()) {
    lcd.clear();
    lcd.print("Error init rtc");
    stop();
  } else {
    if (! rtc.isrunning()) {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      state = state | MASK_RTC_ERR;
    }
  }
}

void initSD() {
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, LOW);
  if (! SD.begin(SD_CS_PIN)) {
    lcd.clear();
    lcd.print("Error init SD");
    stop();
  }

  writeLog(TYPE_INIT, HARD_SD, DETAIL_FINISHED);
}

void initSensors() {
  pinMode(SENSORS_POWER_PIN, OUTPUT);
  measure();
  for (byte i=0; i<SENSORS_COUNT; i++) {
    if (! isCorrectSensor(i)) {
      if      (i == 0) state = state | MASK_SENSOR0_ERR;
      else if (i == 1) state = state | MASK_SENSOR1_ERR;
    }
  }
  if (bool(state & MASK_SENSOR0_ERR) && bool(state & MASK_SENSOR1_ERR)) {
    lcd.clear();
    lcd.print("All sensors is bad");

    writeLog(TYPE_CHECK, HARD_SENSORS, DETAIL_ALL_BAD);
    stop();
  }

  writeLog(TYPE_INIT, HARD_SENSORS, DETAIL_FINISHED);
}

void initPump() {
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  state = state & MASK_PUMP_DISABLE;

  writeLog(TYPE_INIT, HARD_PUMP, DETAIL_FINISHED);
}

void initState() {
  pinMode(STATE_SER_PIN,   OUTPUT);
  pinMode(STATE_LATCH_PIN, OUTPUT);
  pinMode(STATE_CLK_PIN,   OUTPUT);
  digitalWrite(STATE_SER_PIN,   LOW);
  digitalWrite(STATE_LATCH_PIN, LOW);
  digitalWrite(STATE_CLK_PIN,   LOW);
  writeLog(0, 7, 0);
}

void flushLCD() {
  lcd.setCursor(LCD_POSITION_WATERING[0], LCD_POSITION_WATERING[1]);
  if (now.second() % 2) {
    lcd.print(lastWateringB[6]); lcd.print(lastWateringB[7]); lcd.print(DD);
    lcd.print(lastWateringB[4]); lcd.print(lastWateringB[5]); lcd.print(DD);
    lcd.print(lastWateringB[0]); lcd.print(lastWateringB[1]); lcd.print(lastWateringB[2]); lcd.print(lastWateringB[3]);
  } else {
    lcd.print(SPACE);
    lcd.print(lastWateringB[8]); lcd.print(lastWateringB[9]); lcd.print(DT);
    lcd.print(lastWateringB[10]); lcd.print(lastWateringB[11]); lcd.print(DT);
    lcd.print(lastWateringB[12]); lcd.print(lastWateringB[13]);
    lcd.print(SPACE);
  }

  lcd.setCursor(LCD_POSITION_MOISTURE[0], LCD_POSITION_MOISTURE[1]);
  if (bool(state & MASK_SENSOR1_ERR)) {
    lcd.print(DASH); lcd.print(DASH); lcd.print(DASH); lcd.print(SPACE);
  } else {
    if (moistures[1] < 100) lcd.print(SPACE);
    if (moistures[1] <  10) lcd.print(SPACE);
    lcd.print(moistures[1]); lcd.print(PERCENT);
  }
  lcd.print(SPACE);
  if (bool(state & MASK_SENSOR0_ERR)) {
    lcd.print(DASH); lcd.print(DASH); lcd.print(DASH); lcd.print(SPACE);
  } else {
    if (moistures[0] < 100) lcd.print(SPACE);
    if (moistures[0] <  10) lcd.print(SPACE);
    lcd.print(moistures[0]); lcd.print(PERCENT);
  }
  
  lcd.setCursor(LCD_POSITION_STATE[0], LCD_POSITION_STATE[1]);
  lcd.print(bool(state & MASK_PUMP_ENABLE));
  lcd.print(bool(state & MASK_64));
  lcd.print(bool(state & MASK_32));
  lcd.print(bool(state & MASK_SENSOR1_ERR));
  lcd.print(bool(state & MASK_SENSOR0_ERR));
  lcd.print(bool(state & MASK_SD_WRITE));
  lcd.print(bool(state & MASK_SD_READ));
  lcd.print(bool(state & MASK_RTC_ERR));

  lcd.setCursor(LCD_POSITION_NOW[0], LCD_POSITION_NOW[1]);
  lcd.print(nowB[6]);  lcd.print(nowB[7]);  lcd.print(DD);                                                              // day
  lcd.print(nowB[4]);  lcd.print(nowB[5]);  lcd.print(DD);                                                              // month
  lcd.print(nowB[0]);  lcd.print(nowB[1]);  lcd.print(nowB[2]); lcd.print(nowB[3]); lcd.print(SPACE); lcd.print(SPACE); // year
  lcd.print(nowB[8]);  lcd.print(nowB[9]);  lcd.print(DT);                                                              // hour
  lcd.print(nowB[10]); lcd.print(nowB[11]); lcd.print(DT);                                                              // minute
  lcd.print(nowB[12]); lcd.print(nowB[13]);                                                                             // second
}

TimeSpan getWateringTimeSpan(byte month) {
  // REMOVE AFTER TESTINGS
  return TimeSpan(0, 0, 10, 0);
  if (month > 3 && month < 11) return TimeSpan(7, 0, 0, 0);
  else return TimeSpan(30, 0, 0, 0);
}

void setNows() {
  now = rtc.now();
  setDtChars(nowB, &now);
}

void setLastRTCCorrects() {
  int t[6];
  byte i=0;
  
  File sdFile = SD.open(RTC_CORRECT_FILENAME);
  if (! sdFile) {
    state = state | MASK_SD_READ;

    writeLog(TYPE_INIT, HARD_RTC, DETAIL_ALL_BAD);
  } else {
    while (sdFile.available()) {
      t[i] = sdFile.parseInt();
      i++;
    }
    sdFile.close();    
    lastRTCCorrect = DateTime(t[0], t[1], t[2], t[3], t[4], t[5]);
  }

  writeLog(TYPE_INIT, HARD_RTC, DETAIL_FINISHED);
}

void setLastWaterings() {
  int t[6];
  byte i=0;
  
  File sdFile = SD.open(WATERING_FILENAME);
  if (! sdFile) {
    state = state | MASK_SD_READ;

    writeLog(TYPE_INIT, HARD_PUMP, DETAIL_ERR_READ_FILE);
  } else {
    while (sdFile.available()) {
      t[i] = sdFile.parseInt();
      i++;
    }
    sdFile.close();
    lastWatering = DateTime(t[0], t[1], t[2], t[3], t[4], t[5]);
    setDtChars(lastWateringB, &lastWatering);
  }

  writeLog(TYPE_INIT, HARD_PUMP, DETAIL_FINISHED);
}

void setDtChars(byte *bA, DateTime *dtP) {
  DateTime dt = *dtP;
  int year = dt.year();
  bA[0] = year / 1000;        // 2026 / 1000 = 2
  year -= bA[0] * 1000;       // 2026-2*1000 = 26
  bA[1] = year / 100;         // 26 / 100 = 0
  year -= bA[1] * 100;        // 26-0*100 = 26
  bA[2]  = year / 10;         // 26 / 10 = 2
  bA[3]  = year % 10;         // 26 % 10 = 6
  bA[4]  = dt.month() / 10;   // 1 / 10 = 0
  bA[5]  = dt.month() % 10;   // 1 % 10 = 1
  bA[6]  = dt.day() / 10;     // 12 / 10 = 1
  bA[7]  = dt.day() % 10;     // 12 % 10 = 2
  bA[8]  = dt.hour() / 10;    // 11 / 10 = 1
  bA[9]  = dt.hour() % 10;    // 11 % 10 = 1
  bA[10] = dt.minute() / 10;  // 43 / 10 = 4
  bA[11] = dt.minute() % 10;  // 43 % 10 = 3
  bA[12] = dt.second() / 10;  // 26 / 10 = 2
  bA[13] = dt.second() % 10;  // 26 % 10 = 6
}

void updateState() {
  if (! isCorrectRTC()) {
    state = state | MASK_RTC_ERR;
  } else {
    state = state & MASK_RTC_OK;
  }
  for (byte i=0; i<SENSORS_COUNT; i++) {
    if (! isCorrectSensor(i)) {
      if      (i == 0) {
        state = state | MASK_SENSOR0_ERR;

        writeLog(TYPE_CHECK, HARD_SENSOR_0, DETAIL_BAD_VALUE);
      }
      else if (i == 1) {
        state = state | MASK_SENSOR1_ERR;

        writeLog(TYPE_CHECK, HARD_SENSOR_1, DETAIL_BAD_VALUE);
      }
    }
  }
  digitalWrite(STATE_LATCH_PIN, LOW);
  shiftOut(STATE_SER_PIN, STATE_CLK_PIN, MSBFIRST, state);
  digitalWrite(STATE_LATCH_PIN, HIGH);
  digitalWrite(STATE_LATCH_PIN, LOW);
  if (bool(state & MASK_SENSOR0_ERR) && bool(state & MASK_SENSOR1_ERR)) {
    lcd.clear();
    lcd.print("All sensors bad");

    writeLog(TYPE_CHECK, HARD_SENSORS, DETAIL_ALL_BAD);
    stop();
  }  
}

void updateLastRTCCorrect() {
  lastRTCCorrect = now;
  SD.remove(RTC_CORRECT_FILENAME);
  File sdFile = SD.open(RTC_CORRECT_FILENAME, FILE_WRITE);
  if (! sdFile) {
    state = state | MASK_SD_WRITE;

    writeLog(TYPE_DOING, HARD_RTC, DETAIL_ERR_WRITE_FILE);
    return;
  } else {
    sdFile.print(nowB[0]);  sdFile.print(nowB[1]);  sdFile.print(nowB[2]); sdFile.print(nowB[3]); sdFile.print(DD);
    sdFile.print(nowB[4]);  sdFile.print(nowB[5]);  sdFile.print(DD);
    sdFile.print(nowB[6]);  sdFile.print(nowB[7]);  sdFile.print(SPACE);
    sdFile.print(nowB[8]);  sdFile.print(nowB[9]);  sdFile.print(DT);
    sdFile.print(nowB[10]); sdFile.print(nowB[11]); sdFile.print(DT);
    sdFile.print(nowB[12]); sdFile.print(nowB[13]); sdFile.println();
    sdFile.close();
  }

  writeLog(TYPE_DOING, HARD_RTC, DETAIL_CORRECT_UPDATED);
}

void updateLastWaterings() {
  lastWatering = now;
  setDtChars(lastWateringB, &lastWatering);

  SD.remove(WATERING_FILENAME);

  writeLog(TYPE_DOING, HARD_PUMP, DETAIL_FILE_REMOVED);
  File sdFile = SD.open(WATERING_FILENAME, FILE_WRITE);
  if (! sdFile) {
    state = state | MASK_SD_WRITE;

    writeLog(TYPE_DOING, HARD_PUMP, DETAIL_ERR_WRITE_FILE);
    return;
  } else {
    sdFile.print(nowB[0]);  sdFile.print(nowB[1]);  sdFile.print(nowB[2]); sdFile.print(nowB[3]); sdFile.print(DD);
    sdFile.print(nowB[4]);  sdFile.print(nowB[5]);  sdFile.print(DD);
    sdFile.print(nowB[6]);  sdFile.print(nowB[7]);  sdFile.print(SPACE);
    sdFile.print(nowB[8]);  sdFile.print(nowB[9]);  sdFile.print(DT);
    sdFile.print(nowB[10]); sdFile.print(nowB[11]); sdFile.print(DT);
    sdFile.print(nowB[12]); sdFile.print(nowB[13]); sdFile.println();
    sdFile.close();
  }

  writeLog(TYPE_DOING, HARD_PUMP, DETAIL_WATERING_UPDATED);
}

void updateAverageMoisture() {
  if      (bool(state & MASK_SENSOR0_ERR)) moistures[SENSORS_COUNT] = moistures[1];
  else if (bool(state & MASK_SENSOR1_ERR)) moistures[SENSORS_COUNT] = moistures[0];
  else moistures[SENSORS_COUNT] = (moistures[0] + moistures[1]) / 2;
}

void enablePump() {
  digitalWrite(PUMP_PIN, HIGH);
  pumpStart = now;
  state = state | MASK_PUMP_ENABLE;

  writeLog(TYPE_DOING, HARD_PUMP, DETAIL_ENABLED);
}

void disablePump() {
  digitalWrite(PUMP_PIN, LOW);
  state = state & MASK_PUMP_DISABLE;

  writeLog(TYPE_DOING, HARD_PUMP, DETAIL_DISABLED);
}

bool isCorrectRTC() {
  if (! rtc.isrunning()) {

    writeLog(TYPE_CHECK, HARD_RTC, DETAIL_STOPPED);
    return false;  // RTC stopped
  }
  if (now < lastRTCCorrect) {

    writeLog(TYPE_CHECK, HARD_RTC, DETAIL_NOW_LESS_CORRECT);
    return false;  // Now < last correct datetime
  }
  if (now > lastRTCCorrect + RTC_ACCURACY) {

    writeLog(TYPE_CHECK, HARD_RTC, DETAIL_ACCURACY_OVERRIDE);
    return false;  // Override accuracy RTC
  }
  return true;
}

bool isCorrectSensor(byte num) {
  if (sensorsValues[num] < SENSORS_MIN[num] || sensorsValues[num] > SENSORS_MAX[num]) {

    byte s;
    if (num == 0) s = HARD_SENSOR_0;
    else if (num == 1) s = HARD_SENSOR_1;
    writeLog(TYPE_CHECK, s, DETAIL_BAD_VALUE);
    return false;
  }
  return true;
}

void measure() {
  digitalWrite(SENSORS_POWER_PIN, HIGH);
  delay(500);
  for (byte i=0; i<SENSORS_COUNT; i++) {
    sensorsValues[i] = analogRead(SENSORS_PINS[i]);
    moistures[i] = constrain(map(sensorsValues[i], SENSORS_MIN[i], SENSORS_MAX[i], 100, 0), 0, 100);
  }
  digitalWrite(SENSORS_POWER_PIN, LOW);

  writeLog(TYPE_DOING, HARD_SENSORS, DETAIL_MEASURE_COMPLETED);
}

void writeMeasures() {
  File sdFile = SD.open(VALUES_FILENAME, FILE_WRITE);
  if (! sdFile) {
    state = state | MASK_SD_WRITE;

    writeLog(TYPE_DOING, HARD_SENSORS, DETAIL_ERR_WRITE_FILE);
  } else {
    sdFile.print(nowB[0]);  sdFile.print(nowB[1]);  sdFile.print(nowB[2]); sdFile.print(nowB[3]); sdFile.print(DD); // year
    sdFile.print(nowB[4]);  sdFile.print(nowB[5]);  sdFile.print(DD);                                               // month
    sdFile.print(nowB[6]);  sdFile.print(nowB[7]);  sdFile.print(SPACE);                                            // day
    sdFile.print(nowB[8]);  sdFile.print(nowB[9]);  sdFile.print(DT);                                               // hour
    sdFile.print(nowB[10]); sdFile.print(nowB[11]); sdFile.print(DT);                                               // minute
    sdFile.print(nowB[12]); sdFile.print(nowB[13]); sdFile.print(SPACE);                                            // second
    sdFile.print(SPACE); sdFile.print(SPACE);
    for (byte i=0; i<SENSORS_COUNT; i++) {
      sdFile.print(sensorsValues[i]); sdFile.print(SPACE); sdFile.print(SPACE);
      sdFile.print(moistures[i]); sdFile.print(SPACE); sdFile.print(SPACE);
    }
    sdFile.print(moistures[SENSORS_COUNT]); sdFile.print(SPACE); sdFile.print(SPACE);
    sdFile.print(bool(state & MASK_PUMP_ENABLE));
    sdFile.print(bool(state & MASK_64));
    sdFile.print(bool(state & MASK_32));
    sdFile.print(bool(state & MASK_SENSOR1_ERR));
    sdFile.print(bool(state & MASK_SENSOR0_ERR));
    sdFile.print(bool(state & MASK_SD_WRITE));
    sdFile.print(bool(state & MASK_SD_READ));
    sdFile.print(bool(state & MASK_RTC_ERR));
    sdFile.println();
    sdFile.close();

    writeLog(TYPE_DOING, HARD_SENSORS, DETAIL_MEASURE_WRITED);
  }
}

void writeLog(byte type, byte hard, byte detail) {
  File sdFile = SD.open(LOG_FILENAME, FILE_WRITE);
  if (! sdFile) {
    lcd.clear();
    lcd.print("Err open log file to write");
    while (true) delay(1000);
  } else {
    // DateTime
    sdFile.print(nowB[0]);  sdFile.print(nowB[1]);  sdFile.print(nowB[2]); sdFile.print(nowB[3]); sdFile.print(DD);
    sdFile.print(nowB[4]);  sdFile.print(nowB[5]);  sdFile.print(DD);
    sdFile.print(nowB[6]);  sdFile.print(nowB[7]);  sdFile.print(SPACE);
    sdFile.print(nowB[8]);  sdFile.print(nowB[9]);  sdFile.print(DT);
    sdFile.print(nowB[10]); sdFile.print(nowB[11]); sdFile.print(DT);
    sdFile.print(nowB[12]); sdFile.print(nowB[13]); sdFile.print(SPACE);

    sdFile.print(type);   sdFile.print(SPACE);
    sdFile.print(hard);   sdFile.print(SPACE);
    sdFile.print(detail); sdFile.print(SPACE);

    sdFile.print(bool(state & MASK_PUMP_ENABLE));
    sdFile.print(bool(state & MASK_64));
    sdFile.print(bool(state & MASK_32));
    sdFile.print(bool(state & MASK_SENSOR1_ERR));
    sdFile.print(bool(state & MASK_SENSOR0_ERR));
    sdFile.print(bool(state & MASK_SD_WRITE));
    sdFile.print(bool(state & MASK_SD_READ));
    sdFile.print(bool(state & MASK_RTC_ERR));
    
    sdFile.println();
    sdFile.close();
  }
}

void stop() {

  writeLog(TYPE_ALARM, HARD_ALL, DETAIL_STOP);
  while (true) delay(1000);
}
