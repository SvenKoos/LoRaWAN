#include <Arduino.h>
#include <CayenneLPP.h>
#include "Adafruit_EPD.h"
#include "RadioLib.h"
#include "t_echo_lite_config.h"
#include "Display_Fonts.h"
#include "Adafruit_SHT31.h"

#include "ttn_config.h"

SPIClass Custom_SPI_0(NRF_SPIM0, SCREEN_MISO, SCREEN_SCLK, SCREEN_MOSI);
Adafruit_SSD1681 display(SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_DC, SCREEN_RST,
                         SCREEN_CS, SCREEN_SRAM_CS, SCREEN_BUSY, &Custom_SPI_0);

SPIClass Custom_SPI_3(NRF_SPIM3, SX1262_MISO, SX1262_SCLK, SX1262_MOSI);
SX1262 radio = new Module(SX1262_CS, SX1262_DIO1, SX1262_RST, SX1262_BUSY, Custom_SPI_3);

Adafruit_SHT31 sht31 = Adafruit_SHT31();  // Globales Objekt

// trigger battery measurement
bool manualTrigger = false;
volatile bool KET1_Triggered_Flag = false;

// LoRaWAN-Instanz - sie nutzt das existierende radio-Objekt
// create the LoRaWAN node
LoRaWANNode loraWAN(&radio, &Region, subBand);

bool ttn_joined = false;
bool loRaWAN_started = false;

// CayenneLPP-Objekt erstellen (51 Bytes Puffer reichen dicke aus)
CayenneLPP lpp(51);

// Zustandsvariablen für die änderungsbasierte Sendung
float lastSentTemp = -999.0;
float lastSentHum = -999.0;

// Heartbeat
unsigned long lastHeartbeatMillis = 0;
const unsigned long HEARTBEAT_INTERVAL = 30UL * 60UL * 1000UL;  // 30 Minuten Heartbeat

// thtreshold based sending
const float TEMP_THRESHOLD = 0.2;  // Abweichung in °C
const float HUM_THRESHOLD = 1.0;   // Abweichung in %

void External_Interrupt_Triggered() {
  KET1_Triggered_Flag = true;
}

void setRfSwitchState(uint8_t state) {
  Serial.println("Switch TX");
  // state 0 = RX, state 1 = TX
  if (state == 1) {  // TX
    digitalWrite(SX1262_RF_VC1, HIGH);
    digitalWrite(SX1262_RF_VC2, LOW);
  } else {  // RX
    digitalWrite(SX1262_RF_VC1, LOW);
    digitalWrite(SX1262_RF_VC2, HIGH);
  }
}

void Start_TTN_Join() {
  Serial.println("Initialisiere Radio-Hardware...");

  // 1. Alle SPI-Pins auf INPUT_PULLUP setzen (Sicherer Zustand für nRF52 Bus)
  pinMode(SX1262_SCLK, INPUT_PULLUP);
  pinMode(SX1262_MISO, INPUT_PULLUP);
  pinMode(SX1262_MOSI, INPUT_PULLUP);

  // 2. Hardware-Reset (Der "P2P-Wachmacher")
  pinMode(SX1262_RST, OUTPUT);
  digitalWrite(SX1262_RST, LOW);
  delay(50);  // Länger als P2P, um sicher zu gehen
  digitalWrite(SX1262_RST, HIGH);
  delay(100);

  // 3. SPI neu starten
  Custom_SPI_3.begin();

  // 1. Definiere die beteiligten Pins
  // Wir nutzen VC1 und VC2. RadioLib erwartet ein Array der Größe RFSWITCH_MAX_PINS (meist 5, wir füllen den Rest mit 0)
  uint32_t pins[] = { SX1262_RF_VC1, SX1262_RF_VC2, 0, 0, 0 };

  // 2. Definiere die Zustände (Logic Table)
  // Jeder Eintrag entspricht einem Modus:
  // [RX, TX_RFO, TX_PA]
  // Für jeden Pin definieren wir den Pegel (1=HIGH, 0=LOW)
  // VC1=HIGH(1)/LOW(0), VC2=LOW(0)/HIGH(1)
  // RX: VC1=LOW, VC2=HIGH  => {0, 1}
  // TX: VC1=HIGH, VC2=LOW  => {1, 0}

  Module::RfSwitchMode_t table[] = {
    { 0, 1 },  // RX Mode: VC1=0, VC2=1
    { 1, 0 },  // TX RFO Mode: VC1=1, VC2=0
    { 1, 0 }   // TX PA Mode: VC1=1, VC2=0
  };

  // 3. Übergabe an das Radio
  radio.setRfSwitchTable(pins, table);
  Serial.println("Radio Switch registered!");

  // Teste, ob das Radio den Befehl angenommen hat
  // Ein einfacher Pin-Toggle zur Bestätigung:
  digitalWrite(SX1262_RF_VC1, HIGH);
  delay(100);
  digitalWrite(SX1262_RF_VC1, LOW);
  Serial.println("Switch Pins wurden kurz getoggelt!");

  // 4. Radio init
  ConfigLoRa_t config;
  config.frequency = 868;   // The frequency here does not matter, as it will get changed by LoRaWAN anyway
  radio.tcxoVoltage = 1.6;  // Some radio modules like SX126x often come with TCXO
  int16_t state = radio.begin(config);

  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("Fehler Code: %d\n", state);
    return;
  }

  Serial.println("Radio init success!");

  // ... OTAA Start ...

  // 4. LoRaWAN starten
  state = loraWAN.beginOTAA(joinEUI, devEUI, nwkKey, appKey);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("OTAA Start OK, verbinde...");

    // Override the default join rate
    loraWAN.setDatarate(4);

    state = loraWAN.activateOTAA();
    // Prüfe auf ERR_NONE (0) ODER RADIOLIB_LORAWAN_NEW_SESSION (-1118)
    if (state == RADIOLIB_ERR_NONE || state == RADIOLIB_LORAWAN_NEW_SESSION) {
      Serial.printf("activateOTAA erfolgreich (Code: %d)\n", state);

      Serial.print("[LoRaWAN] DevAddr: ");
      Serial.println((unsigned long)loraWAN.getDevAddr(), HEX);

      loraWAN.setDatarate(5);

      loRaWAN_started = true;
    } else {
      Serial.printf("Fehler bei activateOTAA: %d\n", state);
    }
  } else {
    Serial.printf("Fehler bei beginOTAA: %d\n", state);
  }
}

void setup(void) {
  Serial.begin(115200);

  // 3.3V Power ON
  pinMode(RT9080_EN, OUTPUT);
  digitalWrite(RT9080_EN, HIGH);

  pinMode(SCREEN_BS1, OUTPUT);
  digitalWrite(SCREEN_BS1, LOW);

  pinMode(SX1262_RF_VC1, OUTPUT);
  pinMode(SX1262_RF_VC2, OUTPUT);

  pinMode(nRF52840_BOOT, INPUT_PULLUP);
  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);
  digitalWrite(LED_1, HIGH);
  digitalWrite(LED_2, HIGH);

  display.begin();
  display.setRotation(1);
  display.setTextColor(EPD_BLACK);
  display.display();

  // 2. I2C-Pins festlegen UND den Bus starten
  delay(100);
  Wire.setPins(35, 36);
  Wire.begin();
  delay(100);

  // 3. Sensor initialisieren
  if (!sht31.begin(0x44)) {
    Serial.println("SHT3x nicht gefunden!");
  } else {
    Serial.println("SHT3x bereit.");
  }

  // enable  battery measurement
  pinMode(BATTERY_ADC_DATA, INPUT);
  pinMode(BATTERY_MEASUREMENT_CONTROL, OUTPUT);
  digitalWrite(BATTERY_MEASUREMENT_CONTROL, LOW);  // Turn off battery voltage measurement

  attachInterrupt(nRF52840_BOOT, External_Interrupt_Triggered, FALLING);

  // Set the analog reference to 3.0V (default = 3.6V)
  analogReference(AR_INTERNAL_3_0);
  // Set the resolution to 12-bit (0..4095)
  analogReadResolution(12);  // Can be 8, 10, 12 or 14
}

void loop() {
  if (!loRaWAN_started) {
    delay(2000);  // Gib dem nRF52 2 Sekunden Zeit nach dem Boot
    Serial.println("Starte LoRaWAN-Join jetzt...");
    Start_TTN_Join();
  }

  if (KET1_Triggered_Flag == true) {
    delay(300);

    KET1_Triggered_Flag = false;

    Serial.println("KEY1_Triggered");

    manualTrigger = !manualTrigger;
  }

  // 2. Sensor-Messung (alle 2*60 Sekunden)
  static unsigned long lastSensorMillis = -2 * 60000;
  unsigned long currentMillis = millis();
  if (currentMillis - lastSensorMillis >= 2 * 60000) {
    lastSensorMillis = currentMillis;

    // handle battery measurement request
    if (manualTrigger == false) {
      digitalWrite(LED_2, HIGH);

      digitalWrite(BATTERY_MEASUREMENT_CONTROL, LOW);  // Turn off battery voltage measurement
      Serial.print("Turn off battery voltage measurement\n");
    } else {
      digitalWrite(LED_2, LOW);

      digitalWrite(BATTERY_MEASUREMENT_CONTROL, HIGH);  // Enable battery voltage measurement
      Serial.print("Turn on battery voltage measurement\n");
    }

    // 1. I2C Bus aktiv für die Messung vorbereiten
    Wire.begin();

    float t = sht31.readTemperature();
    float h = sht31.readHumidity();

    // 2. I2C Bus sofort wieder "hart" beenden/freigeben
    // Das verhindert, dass die I2C-Hardware auf den Pins hängen bleibt
    Wire.end();

    if (!isnan(t)) {
      // 3. SPI-Bus für das Display exklusiv zurückgewinnen
      Custom_SPI_0.end();
      Custom_SPI_0.begin();
      delay(10);  // Kurze Stabilisierung für den SPI-Bus nach Reinit

      // ADC Wert direkt VOR dem Display-Zusammenbau holen, damit wir alles zusammen haben
      uint32_t adc = analogRead(BATTERY_ADC_DATA);
      float battVoltage = 0.0;
      if (adc > 0) {
        battVoltage = (((float)adc * ((3000.0 / 4096.0))) / 1000.0) * 2.0;
      }

      display.clearBuffer();

      display.setFont(&FreeSans9pt7b);
      display.setTextSize(3);
      display.setCursor(5, 50);
      display.printf("%.1f C", t);
      display.setCursor(5, 120);
      display.printf("%.1f %%", h);

      // Batteriespannung im selben Puffer ergänzen
      if (adc > 0) {
        display.setTextSize(1);
        display.setCursor(5, 160);
        display.printf("Battery: %.03f V", battVoltage);
      }

      display.display();
      delay(10);  // Kurz warten, damit der Controller den Befehl verarbeitet

      // SPI nach Display-Refresh wieder schlafen legen, um LoRa nicht zu stören
      Custom_SPI_0.end();

      // Serielle Ausgabe zur Kontrolle
      if (adc > 0) {
        Serial.print("ADC Value: ");
        Serial.println(adc);
        Serial.printf("Battery Voltage: %.03f V\n", battVoltage);
      }

      // LoRa / CayenneLPP sending
      if (loRaWAN_started) {
        bool sendReasonTempChange = (abs(t - lastSentTemp) >= TEMP_THRESHOLD);
        bool sendReasonHumChange = (abs(h - lastSentHum) >= HUM_THRESHOLD);
        bool sendReasonHeartbeat = (currentMillis - lastHeartbeatMillis >= HEARTBEAT_INTERVAL);

        // Wenn sich genug geändert hat, ein Heartbeat fällig ist ODER der Button gedrückt wurde:
        if (sendReasonTempChange || sendReasonHumChange || sendReasonHeartbeat || manualTrigger) {

          lpp.reset();
          // Kanal 1: Temperatur
          lpp.addTemperature(1, t);
          // Kanal 2: Rel. Luftfeuchtigkeit
          lpp.addRelativeHumidity(2, h);

          Serial.println("Sende Uplink an TTN...");

          // Da das SX1262 Radio-Objekt im RadioLib-LoRaWANNode gekapselt ist,
          // senden wir den Uplink über die loraWAN-Instanz (Port 1)
          int16_t state = loraWAN.sendReceive(lpp.getBuffer(), lpp.getSize(), 1);

          if (state == RADIOLIB_ERR_NONE) {
            Serial.println("Uplink erfolgreich gesendet!");
          } else {
            Serial.print("Uplink-Fehler: ");
            Serial.println(state);
          }
          // Werte für den nächsten Vergleich sichern
          lastSentTemp = t;
          lastSentHum = h;
          lastHeartbeatMillis = currentMillis;
        } else
          Serial.println("Skip sending Uplink to TTN - no change in measurements.");
      }
    }
  }
}
