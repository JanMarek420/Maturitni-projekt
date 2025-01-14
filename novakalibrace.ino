#include <HX711_ADC.h>
#include <EEPROM.h>

// Piny pro senzory
const int HX711_5kg_dout = 4;
const int HX711_5kg_sck = 5;
const int HX711_1kg_dout = 2;
const int HX711_1kg_sck = 3;

// Inicializace HX711 pro oba senzory
HX711_ADC LoadCell5kg(HX711_5kg_dout, HX711_5kg_sck);
HX711_ADC LoadCell1kg(HX711_1kg_dout, HX711_1kg_sck);

// EEPROM adresy pro kalibrační data a tare offsety
const int EEPROM_CAL_5KG = 0;
const int EEPROM_TARE_5KG = 8;
const int EEPROM_CAL_1KG = 16;
const int EEPROM_TARE_1KG = 24;

void setup() {
  Serial.begin(57600);
  Serial.println();
  Serial.println("Kalibracni program pro dva senzory HX711");

  // Kalibrace pro 5kg senzor
  Serial.println("\n=== Kalibrace pro 5kg senzor ===");
  calibrateSensor(LoadCell5kg, EEPROM_CAL_5KG, EEPROM_TARE_5KG, "5kg senzor");

  // Kalibrace pro 1kg senzor
  Serial.println("\n=== Kalibrace pro 1kg senzor ===");
  calibrateSensor(LoadCell1kg, EEPROM_CAL_1KG, EEPROM_TARE_1KG, "1kg senzor");

  Serial.println("\nKalibrace obou senzorů byla úspěšně dokončena!");
}

void loop() {
  // Po dokončení kalibrace není třeba loop() využívat
}

// Funkce pro kalibraci senzoru
void calibrateSensor(HX711_ADC &sensor, int calAddr, int tareAddr, const char *name) {
  sensor.begin();
  sensor.start(2000, true); // Stabilizace senzoru

  if (sensor.getTareTimeoutFlag() || sensor.getSignalTimeoutFlag()) {
    Serial.println("Timeout! Zkontrolujte zapojení senzoru.");
    while (1);
  }

  sensor.setCalFactor(1.0); // Počáteční kalibrační hodnota
  Serial.print(name);
  Serial.println(" pripraveno k kalibraci.");

  // Krok 1: Tare (nulování)
  Serial.println("1. Odeberte závaží ze senzoru a stiskněte 't' pro nulování.");
  while (true) {
    if (Serial.available() > 0 && Serial.read() == 't') {
      sensor.tareNoDelay(); // Provádí tare (nulování)
      delay(2000); // Čekání na dokončení
      break;
    }
  }
  EEPROM.put(tareAddr, sensor.getTareOffset()); // Uložení tare offsetu
  Serial.println("Tare dokončeno a uloženo.");

  // Krok 2: Kalibrace známou váhou
  Serial.println("2. Umístěte známou váhu na senzor a zadejte její hodnotu (v kg):");
  float known_mass = 0;
  while (true) {
    if (Serial.available() > 0) {
      known_mass = Serial.parseFloat(); // Načtení zadané hodnoty
      if (known_mass > 0) break;
    }
  }

  // Vypočítání kalibrační hodnoty
  sensor.refreshDataSet();
  float newCalFactor = sensor.getNewCalibration(known_mass);
  sensor.setCalFactor(newCalFactor);
  EEPROM.put(calAddr, newCalFactor); // Uložení kalibrační hodnoty

  // Výpis uložených hodnot
  Serial.print("Kalibrace pro ");
  Serial.print(name);
  Serial.println(" dokončena.");
  Serial.print("Kalibrační hodnota: ");
  Serial.println(newCalFactor);
  Serial.print("Tare offset: ");
  Serial.println(sensor.getTareOffset());
}
