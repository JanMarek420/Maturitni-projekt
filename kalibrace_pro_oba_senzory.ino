#include <HX711_ADC.h>
#include <EEPROM.h>

// Piny pro váhové senzory HX711
const int HX711_5kg_dout = 04; // 5kg senzor dout
const int HX711_5kg_sck = 5;  // 5kg senzor sck
const int HX711_1kg_dout = 2; // 1kg senzor dout
const int HX711_1kg_sck = 3;  // 1kg senzor sck

// Adresy pro uložení dat do EEPROM
const int EEPROM_ADDR_CALIBRATION_5KG = 0; // Kalibrační faktor pro 5kg senzor
const int EEPROM_ADDR_TARE_5KG = 16;        // Tare offset pro 5kg senzor
const int EEPROM_ADDR_CALIBRATION_1KG = 32; // Kalibrační faktor pro 1kg senzor
const int EEPROM_ADDR_TARE_1KG = 48;       // Tare offset pro 1kg senzor

// HX711 váhové senzory
HX711_ADC LoadCell5kg(HX711_5kg_dout, HX711_5kg_sck); // 5kg senzor
HX711_ADC LoadCell1kg(HX711_1kg_dout, HX711_1kg_sck); // 1kg senzor

void setup() {
  // Inicializace sériové komunikace
  Serial.begin(57600);
  Serial.println("Kalibrace vahovych senzoru (5kg a 1kg)");
  Serial.println("--------------------------------------");

  // Inicializace senzorů
  setupLoadCell(LoadCell5kg, "5kg senzor");
  setupLoadCell(LoadCell1kg, "1kg senzor");

  // Kalibrace obou senzorů
  calibrateSensor(LoadCell5kg, EEPROM_ADDR_CALIBRATION_5KG, EEPROM_ADDR_TARE_5KG, "5kg senzor");
  calibrateSensor(LoadCell1kg, EEPROM_ADDR_CALIBRATION_1KG, EEPROM_ADDR_TARE_1KG, "1kg senzor");

  Serial.println("Kalibrace dokoncena.");
}

void loop() {
  // Program se zastaví po kalibraci
  while (1);
}

// Funkce pro inicializaci senzoru
void setupLoadCell(HX711_ADC &sensor, const char *name) {
  sensor.begin();
  sensor.start(2000, true);

  if (sensor.getTareTimeoutFlag() || sensor.getSignalTimeoutFlag()) {
    Serial.print(name);
    Serial.println(": CHYBA senzoru! Zkontrolujte pripojeni.");
    while (1);
  }

  Serial.print(name);
  Serial.println(": Senzor inicializovan.");
}

// Funkce pro kalibraci senzoru a uložení dat do EEPROM
void calibrateSensor(HX711_ADC &sensor, int calAddr, int tareAddr, const char *name) {
  Serial.println("--------------------------------------");
  Serial.print(name);
  Serial.println(": Spoustim kalibraci.");

  // Nulování senzoru (tare)
  Serial.println("1. Odstrante zatez ze senzoru.");
  Serial.println("2. Stisknete 't' pro nulovani senzoru.");
  while (true) {
    if (Serial.available() > 0 && Serial.read() == 't') {
      sensor.tareNoDelay(); // Spustí nulování
      delay(2000);          // Čeká na dokončení
      break;
    }
  }
  writeEEPROMLong(tareAddr, sensor.getTareOffset()); // Uložení tare offsetu
  Serial.println("Nulovani hotovo.");

  // Kalibrace se známou hmotností
  Serial.println("3. Umistete znamou zatez na senzor.");
  Serial.println("4. Zadejte jeji hodnotu (v jednotkach, napr. 2.5 pro 2.5kg).");

  float knownMass = 0;
  while (true) {
    if (Serial.available() > 0) {
      knownMass = Serial.parseFloat();
      if (knownMass > 0) break;
    }
  }

  sensor.refreshDataSet();
  float newCalFactor = sensor.getNewCalibration(knownMass);
  sensor.setCalFactor(newCalFactor);
  writeEEPROMFloat(calAddr, newCalFactor); // Uložení kalibračního faktoru

  Serial.println("Kalibrace hotova.");
  Serial.print("Kalibracni faktor ulozen: ");
  Serial.println(newCalFactor);
}

// EEPROM funkce
void writeEEPROMFloat(int addr, float value) {
  EEPROM.put(addr, value);
}
float readEEPROMFloat(int addr) {
  float value;
  EEPROM.get(addr, value);
  return value;
}
void writeEEPROMLong(int addr, long value) {
  EEPROM.put(addr, value);
}
long readEEPROMLong(int addr) {
  long value;
  EEPROM.get(addr, value);
  return value;
}
