/*
  Test 3: LoRa P2P Test - Node A (Transmitter / Ping Initiator)
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

uint32_t packetCount = 0;
uint32_t lastTxMs = 0;
const uint32_t TX_INTERVAL_MS = 2500;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // LED off

  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && (millis() - start < 3500));

  Serial.println("\n==============================================");
  Serial.println("=== Test 3: LoRa Ping-Pong - NODE A (TX) ===");
  Serial.println("==============================================");

  SPI.begin();

  // 868.0 MHz, BW 125 kHz, SF 7, CR 4/5, SyncWord 0x12, Power 14 dBm, Preamble 8, TCXO 1.8V
  int16_t state = radio.begin(868.0, 125.0, 7, 5, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 14, 8, 1.8);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("Radio init failed, code ");
    Serial.println(state);
    while (true) delay(1000);
  }

  Serial.println("SX1262 initialized successfully on Node A (868.0 MHz).");
  Serial.println("Transmitting PING every 2.5s and listening for PONG from Node B...\n");
}

void loop() {
  uint32_t now = millis();

  if (now - lastTxMs >= TX_INTERVAL_MS) {
    lastTxMs = now;
    packetCount++;

    String pingMsg = "PING #" + String(packetCount);
    Serial.print("[Node A] Transmitting LoRa: '");
    Serial.print(pingMsg);
    Serial.print("' ... ");

    digitalWrite(LED_BUILTIN, LOW); // LED on during TX
    int16_t txState = radio.transmit(pingMsg);
    digitalWrite(LED_BUILTIN, HIGH);

    if (txState == RADIOLIB_ERR_NONE) {
      Serial.println("OK! (Packet sent)");
    } else {
      Serial.print("Failed, code: ");
      Serial.println(txState);
    }

    // Now listen for PONG reply for up to 1200 ms
    Serial.print("[Node A] Listening for PONG reply... ");
    String replyMsg;
    int16_t rxState = radio.receive(replyMsg, 1200);

    if (rxState == RADIOLIB_ERR_NONE) {
      Serial.print("SUCCESS! Received: '");
      Serial.print(replyMsg);
      Serial.print("' | RSSI: ");
      Serial.print(radio.getRSSI());
      Serial.print(" dBm, SNR: ");
      Serial.print(radio.getSNR());
      Serial.println(" dB");

      // Quick double-blink for round-trip success
      digitalWrite(LED_BUILTIN, LOW); delay(50); digitalWrite(LED_BUILTIN, HIGH); delay(50);
      digitalWrite(LED_BUILTIN, LOW); delay(50); digitalWrite(LED_BUILTIN, HIGH);
    } else if (rxState == RADIOLIB_ERR_RX_TIMEOUT) {
      Serial.println("Timeout (No reply from Node B yet)");
    } else {
      Serial.print("RX Error, code: ");
      Serial.println(rxState);
    }

    Serial.println("----------------------------------------------");
  }
}
