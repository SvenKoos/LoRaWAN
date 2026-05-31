#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_SHT31.h>
#include "Adafruit_EPD.h"
#include "RadioLib.h"
#include "t_echo_lite_config.h"
#include "Display_Fonts.h"

// --- NEU: Globale Objekte für Sensor und Timer ---
Adafruit_SHT31 sht31;
unsigned long lastMeasureTime = 0;
const unsigned long interval = 60000;  // 60 Sekunden in Millisekunden

// Globale Variablen für die aktuellen Messwerte
float currentTemperature = 0.0;
float currentHumidity = 0.0;
bool sensorAvailable = false;

static const uint32_t Local_MAC[2] = {
  NRF_FICR->DEVICEID[0],
  NRF_FICR->DEVICEID[1],
};

struct Display_Refresh_Operator {
  struct {
    bool transmission_fast_refresh_flag = false;
  } sx1262_test;
};

struct SX1262_Operator {
  using mode = enum { LORA,
                      FSK };
  struct {
    float value = 868.1;
    bool change_flag = false;
  } frequency;
  struct {
    float value = 125.0;
    bool change_flag = false;
  } bandwidth;
  struct {
    uint8_t value = 9;
    bool change_flag = false;
  } spreading_factor;
  struct {
    uint8_t value = 7;
    bool change_flag = false;
  } coding_rate;
  struct {
    uint8_t value = 0x12;
    bool change_flag = false;
  } sync_word;
  struct {
    int8_t value = 13;
    bool change_flag = false;
  } output_power;
  struct {
    float value = 60;
    bool change_flag = false;
  } current_limit;
  struct {
    int16_t value = 8;
    bool change_flag = false;
  } preamble_length;
  struct {
    bool value = false;
    bool change_flag = false;
  } crc;

  uint8_t current_mode = mode::LORA;
  volatile bool operation_flag = false;
  bool initialization_flag = false;
};

SX1262_Operator SX1262_OP;
Display_Refresh_Operator Display_Refresh_OP;

SPIClass Custom_SPI_0(NRF_SPIM0, SCREEN_MISO, SCREEN_SCLK, SCREEN_MOSI);
Adafruit_SSD1681 display(SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_DC, SCREEN_RST,
                         SCREEN_CS, SCREEN_SRAM_CS, SCREEN_BUSY, &Custom_SPI_0);

SPIClass Custom_SPI_3(NRF_SPIM3, SX1262_MISO, SX1262_SCLK, SX1262_MOSI);
SX1262 radio = new Module(SX1262_CS, SX1262_DIO1, SX1262_RST, SX1262_BUSY, Custom_SPI_3);

void Dio1_Action_Interrupt(void) {
  SX1262_OP.operation_flag = true;
}

void Set_SX1262_RF_Transmitter_Switch(bool status) {
  if (status == true) {
    digitalWrite(SX1262_RF_VC1, HIGH);  // send
    digitalWrite(SX1262_RF_VC2, LOW);
  } else {
    digitalWrite(SX1262_RF_VC1, LOW);  // receive
    digitalWrite(SX1262_RF_VC2, HIGH);
  }
}

bool SX1262_Set_Default_Parameters(String *assertion) {
  if (radio.setFrequency(SX1262_OP.frequency.value) != RADIOLIB_ERR_NONE) return false;
  if (radio.setBandwidth(SX1262_OP.bandwidth.value) != RADIOLIB_ERR_NONE) return false;
  if (radio.setOutputPower(SX1262_OP.output_power.value) != RADIOLIB_ERR_NONE) return false;
  if (radio.setCurrentLimit(SX1262_OP.current_limit.value) != RADIOLIB_ERR_NONE) return false;
  if (radio.setPreambleLength(SX1262_OP.preamble_length.value) != RADIOLIB_ERR_NONE) return false;
  if (radio.setCRC(SX1262_OP.crc.value) != RADIOLIB_ERR_NONE) return false;

  if (SX1262_OP.current_mode == SX1262_OP.mode::LORA) {
    if (radio.setSpreadingFactor(SX1262_OP.spreading_factor.value) != RADIOLIB_ERR_NONE) return false;
    if (radio.setCodingRate(SX1262_OP.coding_rate.value) != RADIOLIB_ERR_NONE) return false;
    if (radio.setSyncWord(SX1262_OP.sync_word.value) != RADIOLIB_ERR_NONE) return false;
  }
  return true;
}

// --- NEU Zu 2: Messdaten vom I2C-Sensor abfragen ---
void updateSensorData() {
  if (!sensorAvailable) return;

  // SHT31 spezifische Abfrage
  currentTemperature = sht31.readTemperature();
  currentHumidity = sht31.readHumidity();

  // Sicherheitscheck, falls das Kabel im Betrieb rutscht (gibt NaN zurück)
  if (isnan(currentTemperature) || isnan(currentHumidity)) {
    Serial.println("[Sensor] Fehler: Konnte keine gültigen Werte lesen!");
    return;
  }

  Serial.printf("[Sensor] Temp: %.2f °C, Hum: %.2f %%\n", currentTemperature, currentHumidity);
}

// --- NEU Zu 3: Werte ansprechend auf dem E-Paper darstellen ---
void GFX_Print_Sensor_Data() {
  display.fillScreen(EPD_WHITE);

  // Header
  display.setFont(&FreeSans9pt7b);
  display.setTextSize(1);
  display.setCursor(5, 20);
  display.print("T-Echo Telemetrie");

  // Trennlinie
  display.drawFastHLine(0, 30, SCREEN_WIDTH, EPD_BLACK);

  if (sensorAvailable) {
    // Temperatur-Block
    display.setFont(&FreeSans12pt7b);
    display.setCursor(10, 65);
    display.printf("%.1f C", currentTemperature);
    display.setFont(&Org_01);  // Kleines "°" basteln, da nicht im Zeichensatz
    display.drawCircle(62, 50, 2, EPD_BLACK);

    // Feuchtigkeits-Block
    display.setFont(&FreeSans12pt7b);
    display.setCursor(10, 110);
    display.printf("%.1f %% RH", currentHumidity);
  } else {
    display.setFont(&FreeMono9pt7b);
    display.setCursor(10, 70);
    display.print("Sensor Error!");
  }

  // Sende-Status im Fußbereich
  display.setFont(&Org_01);
  display.setTextSize(1);
  display.setCursor(5, 150);
  display.print("LoRa Status: Sende...");

  display.display();
}

// --- NEU Zu 4: Sende-Funktion (Aktuell Rohdaten-Dummy für späteren LoRaWAN-Ausbau) ---
void sendSensorData() {
  if (!SX1262_OP.initialization_flag) return;

  // HF-Switch auf Senden stellen
  Set_SX1262_RF_Transmitter_Switch(true);

  // Wir bauen einen kleinen String für das aktuelle Test-Senden
  String payload = "T:" + String(currentTemperature, 1) + "|H:" + String(currentHumidity, 1);

  Serial.printf("[LoRa] Sende Paket: %s\n", payload.c_str());

  // Paket abschicken (blockiert kurz bis TX abgeschlossen)
  int16_t state = radio.transmit(payload);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("[LoRa] Übertragung erfolgreich!");
  } else {
    Serial.printf("[LoRa] Fehler beim Senden: %d\n", state);
  }

  // Nach dem Senden wieder auf Empfang/Standby schalten, um Strom zu sparen
  Set_SX1262_RF_Transmitter_Switch(false);
}

bool SX1262_Initialization(void) {
  Custom_SPI_3.begin();
  Custom_SPI_3.setClockDivider(SPI_CLOCK_DIV2);

  int16_t state = -1;
  if (SX1262_OP.current_mode == SX1262_OP.mode::LORA) {
    state = radio.begin();
  } else {
    state = radio.beginFSK();
  }

  if (state == RADIOLIB_ERR_NONE) {
    String temp_str;
    if (SX1262_Set_Default_Parameters(&temp_str) == false) return false;
  } else {
    return false;
  }
  return true;
}

void setup(void) {
  Serial.begin(115200);

  // 3.3V Power ON für Peripherie
  pinMode(RT9080_EN, OUTPUT);
  digitalWrite(RT9080_EN, HIGH);

  pinMode(SCREEN_BS1, OUTPUT);
  digitalWrite(SCREEN_BS1, LOW);

  pinMode(SX1262_RF_VC1, OUTPUT);
  pinMode(SX1262_RF_VC2, OUTPUT);
  Set_SX1262_RF_Transmitter_Switch(false);  // Standardmäßig auf Empfang/Safe

  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);
  digitalWrite(LED_1, HIGH);
  digitalWrite(LED_2, HIGH);

  radio.setDio1Action(Dio1_Action_Interrupt);

  display.begin();
  display.setRotation(1);
  display.setTextColor(EPD_BLACK);

  sht31 = Adafruit_SHT31();

  // --- DIE COMPILER-FRIEDENS-VARIANTE ---
  Wire.setPins(36, 35); // SDA = 36, SCL = 35
  Wire.begin();
  
  // Wir rufen .begin() komplett ohne Argumente auf.
  // Das funktioniert garantiert bei JEDER Version dieser Bibliothek.
  sensorAvailable = sht31.begin(); 
  
  if (sensorAvailable) { 
    Serial.println("[Setup] SHT31 Sensor erfolgreich erkannt.");
  } else {
    Serial.println("[Setup] WARNUNG: SHT31 Sensor nicht gefunden!");
  }
  
  // Erste Messung direkt beim Start erwingen
  updateSensorData();

  if (SX1262_Initialization() == true) {
    SX1262_OP.initialization_flag = true;
  } else {
    SX1262_OP.initialization_flag = false;
  }

  // Initiale Anzeige auf dem Display
  GFX_Print_Sensor_Data();
}

void loop() {
  // --- NEU Zu 1: Der unblockierte Minuten-Timer ---
  unsigned long currentMillis = millis();

  if (currentMillis - lastMeasureTime >= interval) {
    lastMeasureTime = currentMillis;  // Zeitstempel aktualisieren

    digitalWrite(LED_1, LOW);  // Status-LED an beim Arbeiten

    updateSensorData();       // 2. Werte abfragen
    GFX_Print_Sensor_Data();  // 3. Werte anzeigen
    sendSensorData();         // 4. Werte senden

    digitalWrite(LED_1, HIGH);  // Status-LED wieder aus
  }
}
