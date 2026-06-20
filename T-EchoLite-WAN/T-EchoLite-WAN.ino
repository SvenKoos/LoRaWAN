#include <Arduino.h>
#include "Adafruit_EPD.h"
#include "RadioLib.h"
#include "t_echo_lite_config.h"
#include "Display_Fonts.h"
#include "Adafruit_SHT31.h"  // Bibliothek hinzufügen

static const uint32_t Local_MAC[2] = {
  NRF_FICR->DEVICEID[0],
  NRF_FICR->DEVICEID[1],
};

struct SX1262_Operator {
  using mode = enum {
    LORA,  // lora mode
    FSK,   // fsk mode
  };

  // aligned with T-Watch
  struct
  {
    float value = 868.1;
    bool change_flag = false;
  } frequency;
  struct
  {
    float value = 125.0;
    bool change_flag = false;
  } bandwidth;
  struct
  {
    // uint8_t value = 12;
    uint8_t value = 9;
    bool change_flag = false;
  } spreading_factor;
  struct
  {
    // uint8_t value = 8;
    uint8_t value = 7;
    bool change_flag = false;
  } coding_rate;
  struct
  {
    // uint8_t value = 0xAB;
    uint8_t value = 0x12;
    bool change_flag = false;
  } sync_word;
  struct
  {
    // int8_t value = 22;
    int8_t value = 13;
    bool change_flag = false;
  } output_power;
  struct
  {
    // float value = 140;
    float value = 60;
    bool change_flag = false;
  } current_limit;
  struct
  {
    // int16_t value = 16;
    int16_t value = 8;
    bool change_flag = false;
  } preamble_length;
  struct
  {
    bool value = false;
    bool change_flag = false;
  } crc;

  uint8_t current_mode = mode::LORA;

  bool initialization_flag = false;
};

SX1262_Operator SX1262_OP;

SPIClass Custom_SPI_0(NRF_SPIM0, SCREEN_MISO, SCREEN_SCLK, SCREEN_MOSI);
Adafruit_SSD1681 display(SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_DC, SCREEN_RST,
                         SCREEN_CS, SCREEN_SRAM_CS, SCREEN_BUSY, &Custom_SPI_0);

SPIClass Custom_SPI_3(NRF_SPIM3, SX1262_MISO, SX1262_SCLK, SX1262_MOSI);
SX1262 radio = new Module(SX1262_CS, SX1262_DIO1, SX1262_RST, SX1262_BUSY, Custom_SPI_3);

Adafruit_SHT31 sht31 = Adafruit_SHT31();  // Globales Objekt

// trigger battery measurement
bool Temp = false;
volatile bool KET1_Triggered_Flag = false;

void External_Interrupt_Triggered() {
  KET1_Triggered_Flag = true;
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
  if (radio.setFrequency(SX1262_OP.frequency.value) != RADIOLIB_ERR_NONE) {
    *assertion = "Failed to set frequency value";
    return false;
  }
  if (radio.setBandwidth(SX1262_OP.bandwidth.value) != RADIOLIB_ERR_NONE) {
    *assertion = "Failed to set bandwidth value";
    return false;
  }
  if (radio.setOutputPower(SX1262_OP.output_power.value) != RADIOLIB_ERR_NONE) {
    *assertion = "Failed to set output_power value";
    return false;
  }
  if (radio.setCurrentLimit(SX1262_OP.current_limit.value) != RADIOLIB_ERR_NONE) {
    *assertion = "Failed to set current_limit value";
    return false;
  }
  if (radio.setPreambleLength(SX1262_OP.preamble_length.value) != RADIOLIB_ERR_NONE) {
    *assertion = "Failed to set preamble_length value";
    return false;
  }
  if (radio.setCRC(SX1262_OP.crc.value) != RADIOLIB_ERR_NONE) {
    *assertion = "Failed to set crc value";
    return false;
  }
  if (SX1262_OP.current_mode == SX1262_OP.mode::LORA) {
    if (radio.setSpreadingFactor(SX1262_OP.spreading_factor.value) != RADIOLIB_ERR_NONE) {
      *assertion = "Failed to set spreading_factor value";
      return false;
    }
    if (radio.setCodingRate(SX1262_OP.coding_rate.value) != RADIOLIB_ERR_NONE) {
      *assertion = "Failed to set coding_rate value";
      return false;
    }
    if (radio.setSyncWord(SX1262_OP.sync_word.value) != RADIOLIB_ERR_NONE) {
      *assertion = "Failed to set sync_word value";
      return false;
    }
  } else {
  }
  return true;
}

void GFX_Print_SX1262_Info(void) {
  display.fillScreen(EPD_WHITE);

  display.setFont(&FreeMonoBold9pt7b);
  display.setCursor(5, 20);
  display.setTextSize(1);

  display.printf("SX1262 Info");

  display.setFont(&FreeSans9pt7b);

  display.setCursor(5, 40);
  display.printf("MAC 0: %u", Local_MAC[0]);
  display.setCursor(5, 60);
  display.printf("MAC 1: %u", Local_MAC[1]);
}

void GFX_Print_SX1262_Init_Successful_Refresh_Info(void) {
  display.setTextSize(1);
  display.setCursor(5, 80);
  display.printf("Status: Init successful");

  display.setCursor(5, 100);
  if (SX1262_OP.current_mode == SX1262_OP.mode::LORA) {
    display.printf("Mode: LoRa");
  } else {
    display.printf("Mode: FSK");
  }

  display.setCursor(5, 120);
  display.printf("Frequency: %.1f MHz", SX1262_OP.frequency.value);

  display.setCursor(5, 140);
  display.printf("Bandwidth: %.1f KHz", SX1262_OP.bandwidth.value);

  display.setCursor(5, 160);
  display.printf("Output Power: %d dBm", SX1262_OP.output_power.value);
}

void GFX_Print_SX1262_Init_Failed_Refresh_Info(void) {
  display.setTextSize(1);
  display.setCursor(5, 80);
  display.printf("Status: Init failed");
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
    if (SX1262_Set_Default_Parameters(&temp_str) == false) {
      Serial.printf("SX1262 Failed to set default parameters\n");
      Serial.printf("SX1262 assertion: %s\n", temp_str.c_str());
      return false;
    }
    if (radio.startReceive() != RADIOLIB_ERR_NONE) {
      Serial.printf("SX1262 Failed to start receive\n");
      return false;
    }
  } else {
    Serial.printf("SX1262 initialization failed\n");
    Serial.printf("Error code: %d\n", state);
    return false;
  }

  Serial.printf("SX1262 initialization successful\n");

  return true;
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

  // receive
  Set_SX1262_RF_Transmitter_Switch(false);

  pinMode(nRF52840_BOOT, INPUT_PULLUP);
  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);
  digitalWrite(LED_1, HIGH);
  digitalWrite(LED_2, HIGH);

  display.begin();
  display.setRotation(1);
  display.setTextColor(EPD_BLACK);

  GFX_Print_SX1262_Info();
  if (SX1262_Initialization() == true) {
    GFX_Print_SX1262_Init_Successful_Refresh_Info();
    SX1262_OP.initialization_flag = true;
  } else {
    GFX_Print_SX1262_Init_Failed_Refresh_Info();
    SX1262_OP.initialization_flag = false;
  }
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
  if (KET1_Triggered_Flag == true) {
    delay(300);

    KET1_Triggered_Flag = false;

    Serial.println("KEY1_Triggered");

    Temp = !Temp;
  }

  // 2. Sensor-Messung (alle 60 Sekunden)
  static unsigned long lastSensorMillis = -60000;
  unsigned long currentMillis = millis();
  if (currentMillis - lastSensorMillis >= 60000) {
    lastSensorMillis = currentMillis;

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

      display.clearBuffer();
      display.setFont(&FreeSans9pt7b);
      display.setTextSize(3);
      display.setCursor(5, 50);
      display.printf("%.1f C", t);
      display.setCursor(5, 120);
      display.printf("%.1f %%", h);
      display.display();

      // SPI nach Display-Refresh wieder schlafen legen, um LoRa nicht zu stören
      Custom_SPI_0.end();
    }

    // show battery measurement results
    if (Temp == false) {
      digitalWrite(LED_1, HIGH);

      digitalWrite(BATTERY_MEASUREMENT_CONTROL, LOW);  // Turn off battery voltage measurement
      Serial.print("Turn off battery voltage measurement\n");
    } else {
      digitalWrite(LED_1, LOW);

      digitalWrite(BATTERY_MEASUREMENT_CONTROL, HIGH);  // Enable battery voltage measurement
      Serial.print("Turn on battery voltage measurement\n");
    }
    uint32_t adc = analogRead(BATTERY_ADC_DATA);
    Serial.print("ADC Value: ");
    Serial.println(adc);

    Serial.printf("ADC Voltage: %.03f V\n", ((float)adc * ((3000.0 / 4096.0))) / 1000.0);

    Serial.printf("Battery Voltage: %.03f V\n", (((float)adc * ((3000.0 / 4096.0))) / 1000.0) * 2.0);
    Serial.println();

    if (adc > 0) {
      display.setFont(&FreeSans9pt7b);
      display.setTextSize(1);
      display.setCursor(5, 160);
      display.printf("Battery: %.03f V", (((float)adc * ((3000.0 / 4096.0))) / 1000.0) * 2.0);
      display.display();
    }
  }
}
