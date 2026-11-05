/*
  Test 2: XIAO nRF52840 -> SPI -> Wio-SX1262 Hardware Check
  Verifies SPI communication, TCXO 1.8V initialization, and register access.
*/

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

// Wio-SX1262 on Seeed XIAO nRF52840 pin mapping:
#define LORA_CS    D4   // NSS / Chip Select (P0.04)
#define LORA_DIO1  D1   // IRQ / Packet received (P0.03)
#define LORA_RST   D2   // Reset (P0.28)
#define LORA_BUSY  D3   // Busy indicator (P0.29)

// Module(NSS, DIO1, NRST, BUSY)
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);

int16_t state = 0;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // LED off (active LOW on XIAO)

  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && (millis() - start < 3500));

  Serial.println("\n==============================================");
  Serial.println("=== Test 2: XIAO nRF52840 <-> SX1262 SPI ===");
  Serial.println("==============================================");
  Serial.println("Pins: CS=D4, DIO1=D1, RST=D2, BUSY=D3");
  Serial.println("Connecting to SX1262...");

  SPI.begin();

  // Wio-SX1262: Frequency 868 MHz, BW 125 kHz, SF7, CR 4/5, SyncWord 0x12, Power 14 dBm, Preamble 8, TCXO 1.8V
  state = radio.begin(
    868.0,                              // Frequency
    125.0,                              // Bandwidth
    7,                                  // SF
    5,                                  // CR (4/5)
    RADIOLIB_SX126X_SYNC_WORD_PRIVATE, // Sync word (0x12)
    14,                                 // Power (dBm)
    8,                                  // Preamble length
    1.8                                 // TCXO voltage (1.8V for Wio-SX1262)
  );

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("\n>>> SUCCESS! SX1262 IS COMMUNICATING OVER SPI! <<<");
    Serial.println("LoRa parameters set: 868.0 MHz, BW 125 kHz, SF7, CR 4/5, TCXO 1.8V");
    // Blink green/builtin LED to show success
    for (int i = 0; i < 6; i++) {
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
    }
  } else {
    Serial.print("\n>>> FAILED! RadioLib error code: ");
    Serial.println(state);
    if (state == RADIOLIB_ERR_CHIP_NOT_FOUND) {
      Serial.println("Error: RADIOLIB_ERR_CHIP_NOT_FOUND (-2). Check SPI wiring or board headers.");
    } else if (state == RADIOLIB_ERR_INVALID_TCXO_VOLTAGE) {
      Serial.println("Error: RADIOLIB_ERR_INVALID_TCXO_VOLTAGE (-705).");
    } else {
      Serial.println("Error initializing SX1262.");
    }
  }
}

int16_t lastState = -999;

void loop() {
  if (state != RADIOLIB_ERR_NONE) {
    // Retry initialization if previously failed
    state = radio.begin(868.0, 125.0, 7, 5, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 14, 8, 1.8);
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println("\n>>> SUCCESS! SX1262 DETECTED AND INITIALIZED! <<<");
    }
  }

  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1900);
  Serial.print("[Test 2] SX1262 status code: ");
  Serial.print(state);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(" (OK - SPI Communicating)");
  } else {
    Serial.println(" (FAILED - Chip not found)");
  }
}
