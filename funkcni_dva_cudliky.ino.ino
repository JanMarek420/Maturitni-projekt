#include <HX711_ADC.h>
#include "DS3232.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Stepper.h>


#define STEPS_PER_REV 200
#define STEP_PIN 6
#define DIR_PIN 7

DS3232 rtc;
LiquidCrystal_I2C lcd(0x27, 20, 4);

Stepper stepperMotor(STEPS_PER_REV, DIR_PIN, STEP_PIN);

const int HX711_5kg_dout = 4; 
const int HX711_5kg_sck = 5;  
const int HX711_1kg_dout = 2; 
const int HX711_1kg_sck = 3;  

HX711_ADC LoadCell5kg(HX711_5kg_dout, HX711_5kg_sck); 
HX711_ADC LoadCell1kg(HX711_1kg_dout, HX711_1kg_sck); 

const int redLed = 8;     
const int yellowLed = 9;  
const int greenLed = 10;  

const int tlacitkoNastaveni = 11;  
const int tlacitkoPotvrzeni = 12;  
const int tlacitkoZpet = 13;       

const float weightThreshold = 65.0;

int feedCount = 1;                  
int feedTimes[2][2] = {{8, 0}, {20, 0}};  
bool feedExecuted[2] = {false, false};    
bool feedBlocked[2] = {false, false};     

bool calibrated5kg = false; 
bool calibrated1kg = false; 


void setupLoadCell(HX711_ADC &sensor);
void calibrateSensor(HX711_ADC &sensor, bool &calibrated, const char* name);
void displayFeedTimes();
void updateLoadCell5kg();
void manageFeeding1kg(int hours, int minutes);
void nastavKrmeni();
bool stisknuto(int pin);

void setup() {
  
  Serial.begin(57600);
  Wire.begin();

  if (rtc.begin() != DS3232_OK) {
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RTC Error: Check");
    lcd.setCursor(0, 1);
    lcd.print("Wiring & Power");
    while (1);
  }

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System pripraven");
  delay(2000);
  lcd.clear();

  setupLoadCell(LoadCell5kg);
  setupLoadCell(LoadCell1kg);
  
  pinMode(redLed, OUTPUT);
  pinMode(yellowLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
  
  pinMode(tlacitkoNastaveni, INPUT_PULLUP);
  pinMode(tlacitkoPotvrzeni, INPUT_PULLUP);
  pinMode(tlacitkoZpet, INPUT_PULLUP);
  
  stepperMotor.setSpeed(60);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  
  calibrateSensor(LoadCell5kg, calibrated5kg, "5kg senzor");
  calibrateSensor(LoadCell1kg, calibrated1kg, "1kg senzor");

  nastavKrmeni();
}

void loop() {
  
  rtc.read();
  int hours = rtc.hours();
  int minutes = rtc.minutes();
  int seconds = rtc.seconds();

  lcd.setCursor(12, 3);
  lcd.print("      ");
  lcd.setCursor(12, 3);
  lcd.print(hours < 10 ? "0" : ""); lcd.print(hours);
  lcd.print(":");
  lcd.print(minutes < 10 ? "0" : ""); lcd.print(minutes);
  lcd.print(":");
  lcd.print(seconds < 10 ? "0" : ""); lcd.print(seconds);
  
  lcd.setCursor(0, 0);
  lcd.print("Pocet krmeni: ");
  lcd.print(feedCount);
  
  displayFeedTimes();
  
  updateLoadCell5kg();
  
  Serial.print("5kg senzor vaha: ");
  Serial.println(LoadCell5kg.getData());
  
  manageFeeding1kg(hours, minutes);
}


void setupLoadCell(HX711_ADC &sensor) {
  sensor.begin();
  sensor.start(2000, true);
  if (sensor.getTareTimeoutFlag() || sensor.getSignalTimeoutFlag()) {
    lcd.setCursor(0, 0);
    lcd.print("HX711 Error");
    while (1);
  }
  sensor.setCalFactor(1.0);
}


void calibrateSensor(HX711_ADC &sensor, bool &calibrated, const char* name) {
  Serial.println(name);
  Serial.println("***");
  Serial.println("Start calibration:");
  Serial.println("1. Umistete senzor na rovny povrch a odstran'te vsechnu zatez.");
  Serial.println("2. Zadejte 't' v seriovem monitoru pro nulovani senzoru.");

  while (true) {
    sensor.update();
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 't') {
        sensor.tareNoDelay(); 
        break;
      }
    }
  }
  Serial.println("Tare complete.");

  Serial.println("3. Umistete znamou zatez na senzor a zadejte jeji hodnotu (v jednotkach).");
  float known_mass = 0;
  while (true) {
    sensor.update();
    if (Serial.available() > 0) {
      known_mass = Serial.parseFloat();
      if (known_mass > 0) break;
    }
  }

  sensor.refreshDataSet();
  float newCalibrationValue = sensor.getNewCalibration(known_mass);
  sensor.setCalFactor(newCalibrationValue);
  Serial.print("Kalibracni hodnota nastavena na: ");
  Serial.println(newCalibrationValue);
  Serial.println("Kalibrace dokoncena.");
  calibrated = true;
}

void updateLoadCell5kg() {
  LoadCell5kg.update();
  float weight = LoadCell5kg.getData();
  
  if (weight > 65) {
    digitalWrite(greenLed, HIGH);
    digitalWrite(yellowLed, LOW);
    digitalWrite(redLed, LOW);
  } else if (weight >= 30) {
    digitalWrite(greenLed, LOW);
    digitalWrite(yellowLed, HIGH);
    digitalWrite(redLed, LOW);
  } else {
    digitalWrite(greenLed, LOW);
    digitalWrite(yellowLed, LOW);
    digitalWrite(redLed, HIGH);
  }
}

void manageFeeding1kg(int hours, int minutes) {
  LoadCell1kg.update();
  float weight1kg = LoadCell1kg.getData();

  for (int i = 0; i < feedCount; i++) {
    if (hours == feedTimes[i][0] && minutes == feedTimes[i][1] && !feedExecuted[i]) {
      if (weight1kg <= weightThreshold && !feedBlocked[i]) {
        stepperMotor.step(STEPS_PER_REV);
        feedExecuted[i] = true;
      } else if (weight1kg > weightThreshold && !feedBlocked[i]) {
        feedBlocked[i] = true;
        feedExecuted[i] = true;
      }
    }
  }
}

void nastavKrmeni() {
  int krok = 0;  
  int aktualniKrmeni = 0;  

  while (true) {
    lcd.clear();
    if (krok == 0) {
      lcd.setCursor(0, 0);
      lcd.print("Nastav pocet krmeni:");
      lcd.setCursor(0, 1);
      lcd.print(feedCount);

      if (stisknuto(tlacitkoNastaveni)) {
        feedCount++;
        if (feedCount > 2) feedCount = 1;  
      }

      if (stisknuto(tlacitkoPotvrzeni)) {
        krok = 1;
        aktualniKrmeni = 0;  
      }
    } else if (krok == 1) {
      lcd.setCursor(0, 0);
      lcd.print("Nastav hodiny pro:");
      lcd.setCursor(0, 1);
      lcd.print("Krmeni ");
      lcd.print(aktualniKrmeni + 1);
      lcd.print(": ");
      lcd.print(feedTimes[aktualniKrmeni][0]);

      if (stisknuto(tlacitkoNastaveni)) {
        feedTimes[aktualniKrmeni][0]++;
        if (feedTimes[aktualniKrmeni][0] > 23) feedTimes[aktualniKrmeni][0] = 0;
      }

      if (stisknuto(tlacitkoPotvrzeni)) {
        krok = 2;  
      }
    } else if (krok == 2) {
      lcd.setCursor(0, 0);
      lcd.print("Nastav minuty pro:");
      lcd.setCursor(0, 1);
      lcd.print("Krmeni ");
      lcd.print(aktualniKrmeni + 1);
      lcd.print(": ");
      lcd.print(feedTimes[aktualniKrmeni][1]);

      if (stisknuto(tlacitkoNastaveni)) {
        feedTimes[aktualniKrmeni][1]++;
        if (feedTimes[aktualniKrmeni][1] > 59) feedTimes[aktualniKrmeni][1] = 0;
      }

      if (stisknuto(tlacitkoPotvrzeni)) {
        aktualniKrmeni++;
        if (aktualniKrmeni >= feedCount) break;  
        krok = 1;  
      }
    }
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Nastaveni hotovo!");
  delay(2000);
}

bool stisknuto(int pin) {
  if (digitalRead(pin) == LOW) {
    delay(200);  
    while (digitalRead(pin) == LOW);  
    return true;
  }
  return false;
}

void displayFeedTimes() {
  if (feedCount >= 1) {
    lcd.setCursor(0, 1);
    lcd.print("Cas krmeni 1: ");
    lcd.print(feedTimes[0][0] < 10 ? "0" : ""); lcd.print(feedTimes[0][0]);
    lcd.print(":");
    lcd.print(feedTimes[0][1] < 10 ? "0" : ""); lcd.print(feedTimes[0][1]);
  }
  if (feedCount == 2) {
    lcd.setCursor(0, 2);
    lcd.print("Cas krmeni 2: ");
    lcd.print(feedTimes[1][0] < 10 ? "0" : ""); lcd.print(feedTimes[1][0]);
    lcd.print(":");
    lcd.print(feedTimes[1][1] < 10 ? "0" : ""); lcd.print(feedTimes[1][1]);
  }
}