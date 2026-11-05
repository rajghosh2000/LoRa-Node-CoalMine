/*
  Test 3: LoRa P2P Test - Node B (Receiver / Pong Responder)
  Hardware: Seeed XIAO nRF52840 + Wio-SX1262
*/

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

#define LORA_CS    D4   // NSS / Chip Select
#define LORA_DIO1  D1   // IRQ / DIO1
#define LORA_RST   D2   // Reset
#define LORA_BUSY  D3   // Busy indicator

SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // LED off

  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && (millis() - start < 3500));

  Serial.println("\n==============================================");
  Serial.println("=== Test 3: LoRa Ping-Pong - NODE B (RX) ===");
  Serial.println("==============================================");

  SPI.begin();

  // 868.0 MHz, BW 125 kHz, SF 7, CR 4/5, SyncWord 0x12, Power 14 dBm, Preamble 8, TCXO 1.8V
  int16_t state = radio.begin(868.0, 125.0, 7, 5, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 14, 8, 1.8);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("Radio init failed, code ");
    Serial.println(state);
    while (true) delay(1000);
  }

  Serial.println("SX1262 initialized successfully on Node B (868.0 MHz).");
  Serial.println("Listening continuously for packets from Node A...\n");
}

void loop() {
  String rxData;
  // Listen for incoming packets with 2000ms timeout
  int16_t state = radio.receive(rxData, 2000);

  if (state == RADIOLIB_ERR_NONE) {
    digitalWrite(LED_BUILTIN, LOW); // Flash LED on RX

    Serial.print("[Node B] RX LoRa: '");
    Serial.print(rxData);
    Serial.print("' | RSSI: ");
    Serial.print(radio.getRSSI());
    Serial.print(" dBm, SNR: ");
    Serial.print(radio.getSNR());
    Serial.println(" dB");

    // If message starts with PING, reply with PONG
    if (rxData.startsWith("PING")) {
      String reply = "PONG" + rxData.substring(4);
      delay(30); // small backoff

      Serial.print("[Node B] Replying with: '");
      Serial.print(reply);
      Serial.print("' ... ");

      int16_t txState = radio.transmit(reply);
      if (txState == RADIOLIB_ERR_NONE) {
        Serial.println("OK! (PONG sent back to Node A)");
      } else {
        Serial.print("TX failed, code: ");
        Serial.println(txState);
      }
    }

    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("----------------------------------------------");
  } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
    // Normal timeout, continue listening
    Serial.print(".");
  } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
    Serial.println("\n[Node B] RX CRC Mismatch");
  } else {
    Serial.print("\n[Node B] RX error, code: ");
    Serial.println(state);
  }
}
