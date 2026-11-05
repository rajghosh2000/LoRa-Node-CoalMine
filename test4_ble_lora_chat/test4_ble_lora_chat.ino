/*
  Test 4: LoRa <-> BLE Chat Gateway (Continuous Hardware RX + Dynamic Adaptive SF/BW)
  Hardware: Seeed Studio XIAO nRF52840 + Wio-SX1262
  
  Outputs pure CSV format directly over BLE UART & Serial:
  Timestamp_ms,Direction,Node,Message,RSSI_dBm,Status
  
  Runtime Commands:
  - 'BAT' or 'battery' -> Queries LiPo voltage and percentage
  - 'SF=<val>'         -> Changes Spreading Factor (5 to 12), e.g. 'SF=8'
  - 'BD=<val>' / 'BW=' -> Changes Bandwidth (e.g. 'BD=250', 'BD=500', 'BD=125')
  - 'CFG' or 'CONFIG'  -> Displays active SF and Bandwidth configuration
*/

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <bluefruit.h>

// ===== SELECT NODE IDENTITY BEFORE FLASHING =====
#define IS_NODE_A  false  // Set to true for Node A (COM27), false for Node B (COM29)

#if IS_NODE_A
  static const char* NODE_ID     = "A";
  static const char* PEER_ID     = "B";
  static const char* BLE_NAME    = "XIAO_Chat_A";
#else
  static const char* NODE_ID     = "B";
  static const char* PEER_ID     = "A";
  static const char* BLE_NAME    = "XIAO_Chat_B";
#endif

// ===== Wio-SX1262 on Seeed XIAO nRF52840 Pin Mapping =====
#define LORA_CS    D4   // NSS
#define LORA_DIO1  D1   // IRQ
#define LORA_RST   D2   // Reset
#define LORA_BUSY  D3   // Busy

SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);

// Hardware interrupt flag for continuous LoRa packet reception
volatile bool packetReceived = false;
void onRadioPacketReceived() {
  packetReceived = true;
}

// ===== Battery Sensing Pins (XIAO nRF52840) =====
#ifndef PIN_VBAT
#define PIN_VBAT     (32) // P0.31 / AIN7
#endif
#ifndef VBAT_ENABLE
#define VBAT_ENABLE  (14) // P0.14
#endif

// ===== BLE UART =====
BLEUart bleuart;

// ===== LoRa Default Settings =====
static const float    LORA_FREQ_MHZ       = 868.0;
static const float    LORA_BW_KHZ         = 125.0;
static const uint8_t  LORA_SF             = 7;
static const uint8_t  LORA_CR             = 5;
static const int8_t   LORA_POWER_DBM      = 14;
static const uint8_t  MAX_RETRIES         = 3;

// Active runtime settings
uint8_t currentSF = LORA_SF;
float currentBW   = LORA_BW_KHZ;

// Adaptive ACK timeout scaling based on LoRa Spreading Factor (Time-on-Air)
uint32_t getAckTimeoutMs() {
  if (currentSF <= 7) return 2000;
  if (currentSF == 8) return 3000;
  if (currentSF == 9) return 4500;
  if (currentSF == 10) return 7000;
  if (currentSF == 11) return 12000;
  return 20000; // SF12
}

// ===== Outgoing Pending Queue for ACK tracking =====
struct PendingMsg {
  bool inUse = false;
  uint32_t msgId = 0;
  String packet;
  uint8_t retriesLeft = 0;
  uint32_t lastSendMs = 0;
};

static const int PENDING_MAX = 5;
PendingMsg pending[PENDING_MAX];
uint32_t localMsgIdSeq = 0;

// ===== Duplicate Suppression =====
static const int SEEN_MAX = 40;
String seenKeys[SEEN_MAX];
int seenCount = 0;

static bool seenBefore(const String &key) {
  for (int i = 0; i < seenCount; i++) {
    if (seenKeys[i] == key) return true;
  }
  if (seenCount < SEEN_MAX) {
    seenKeys[seenCount++] = key;
  } else {
    for (int i = 1; i < SEEN_MAX; i++) seenKeys[i - 1] = seenKeys[i];
    seenKeys[SEEN_MAX - 1] = key;
  }
  return false;
}

// Forward declarations
void startAdv(void);
void handleIncomingLoRa(const String &pkt);
void sendLoRaAck(const String &dst, uint32_t msgId);
void queueOutgoing(const String &text);
void reportBattery(const String &reason);
bool startRadioReceive();

// Core CSV Notification Helper
void notifyCsv(const String &dir, const String &node, const String &msg, const String &rssi, const String &status) {
  String cleanMsg = msg;
  cleanMsg.replace("\"", "\"\"");
  
  String row = String(millis()) + "," + dir + "," + node + ",\"" + cleanMsg + "\"," + rssi + "," + status;
  Serial.println(row);
  if (Bluefruit.connected()) {
    bleuart.println(row);
  }
}

// ---------- Battery Monitoring Helper ----------
float readBatteryVoltage() {
  pinMode(VBAT_ENABLE, OUTPUT);
  digitalWrite(VBAT_ENABLE, LOW);
  delay(10);

  analogReadResolution(12);
  int adc = analogRead(PIN_VBAT);

  digitalWrite(VBAT_ENABLE, HIGH);

  float v = (adc / 4095.0f) * 3.6f * (1510.0f / 510.0f);
  return v;
}

int calculateBatteryPercent(float v) {
  if (v >= 4.20f) return 100;
  if (v <= 3.30f) return 0;
  if (v > 3.80f) {
    return 50 + (int)((v - 3.80f) / (4.20f - 3.80f) * 50.0f);
  } else {
    return (int)((v - 3.30f) / (3.80f - 3.30f) * 50.0f);
  }
}

void reportBattery(const String &reason) {
  float v = readBatteryVoltage();
  int pct = calculateBatteryPercent(v);
  String batStr = String(v, 2) + "V (" + String(pct) + "%)";
  notifyCsv("BAT", NODE_ID, batStr, "N/A", reason);
}

// ---------- BLE Callbacks ----------
void connect_callback(uint16_t conn_handle) {
  BLEConnection* connection = Bluefruit.Connection(conn_handle);
  char peer_name[32] = { 0 };
  connection->getPeerName(peer_name, sizeof(peer_name));
  
  Serial.print("# BLE Connected to: ");
  Serial.println(peer_name);

  bleuart.println("Timestamp_ms,Direction,Node,Message,RSSI_dBm,Status");
  reportBattery("CONNECT");
  
  String cfgStr = "Active Config: SF=" + String(currentSF) + ", BW=" + String(currentBW, 1) + "kHz";
  notifyCsv("CFG", NODE_ID, cfgStr, "N/A", "READY");
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason) {
  (void) conn_handle;
  (void) reason;
  Serial.println("# BLE Client disconnected.");
}

void startAdv(void) {
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();

  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

// ---------- LoRa Communication ----------
bool startRadioReceive() {
  packetReceived = false;
  return (radio.startReceive() == RADIOLIB_ERR_NONE);
}

bool loraSend(const String &payload) {
  packetReceived = false;
  String txCopy = payload;
  int16_t state = radio.transmit(txCopy);
  startRadioReceive();
  return (state == RADIOLIB_ERR_NONE);
}

void sendLoRaAck(const String &dst, uint32_t msgId) {
  delay((int)random(20, 60)); // Jitter backoff
  String ack = "ACK:" + dst + ":" + String(msgId);
  loraSend(ack);
}

void queueOutgoing(const String &text) {
  uint32_t msgId = ++localMsgIdSeq;
  String pkt = "MSG:" + String(NODE_ID) + ":" + String(msgId) + ":" + text;

  for (int i = 0; i < PENDING_MAX; i++) {
    if (!pending[i].inUse) {
      pending[i].inUse = true;
      pending[i].msgId = msgId;
      pending[i].packet = pkt;
      pending[i].retriesLeft = MAX_RETRIES;
      pending[i].lastSendMs = 0; // Trigger send immediately

      notifyCsv("TX", NODE_ID, text, "N/A", "SENT");
      return;
    }
  }
  notifyCsv("ERR", NODE_ID, "Queue full - msg dropped", "N/A", "DROPPED");
}

void ackPending(uint32_t msgId) {
  for (int i = 0; i < PENDING_MAX; i++) {
    if (pending[i].inUse && pending[i].msgId == msgId) {
      pending[i].inUse = false;
      notifyCsv("ACK", PEER_ID, "msg #" + String(msgId), String(radio.getRSSI(), 1), "DELIVERED");
      return;
    }
  }
}

void handleRetries() {
  uint32_t now = millis();
  uint32_t timeoutMs = getAckTimeoutMs();

  for (int i = 0; i < PENDING_MAX; i++) {
    if (!pending[i].inUse) continue;

    if (pending[i].lastSendMs == 0 || (now - pending[i].lastSendMs) >= timeoutMs) {
      if (pending[i].retriesLeft == 0) {
        notifyCsv("ERR", PEER_ID, "msg #" + String(pending[i].msgId), "N/A", "TIMEOUT");
        pending[i].inUse = false;
        continue;
      }

      loraSend(pending[i].packet);
      pending[i].lastSendMs = millis();
      pending[i].retriesLeft--;
    }
  }
}

void checkLoRaRx() {
  if (!packetReceived) return;
  packetReceived = false;

  String rxPkt;
  int16_t state = radio.readData(rxPkt);
  startRadioReceive(); // Return immediately to continuous receive mode

  if (state == RADIOLIB_ERR_NONE) {
    rxPkt.trim();
    if (rxPkt.length() == 0) return;

    if (rxPkt.startsWith("MSG:")) {
      int p1 = rxPkt.indexOf(':', 4);
      int p2 = rxPkt.indexOf(':', p1 + 1);
      if (p1 > 0 && p2 > 0) {
        String src = rxPkt.substring(4, p1);
        String msgIdStr = rxPkt.substring(p1 + 1, p2);
        String text = rxPkt.substring(p2 + 1);
        uint32_t msgId = msgIdStr.toInt();

        if (src != NODE_ID && msgId > 0) {
          sendLoRaAck(src, msgId);

          String key = src + ":" + String(msgId);
          if (!seenBefore(key)) {
            digitalWrite(LED_BUILTIN, LOW);
            notifyCsv("RX", src, text, String(radio.getRSSI(), 1), "RECEIVED");
            delay(40);
            digitalWrite(LED_BUILTIN, HIGH);
          }
        }
      }
    } else if (rxPkt.startsWith("ACK:")) {
      int p1 = rxPkt.indexOf(':', 4);
      if (p1 > 0) {
        String dst = rxPkt.substring(4, p1);
        uint32_t msgId = rxPkt.substring(p1 + 1).toInt();
        if (dst == NODE_ID && msgId > 0) {
          ackPending(msgId);
        }
      }
    }
  }
}

// User command parser
void processUserInput(String text) {
  text.trim();
  if (text.length() == 0) return;

  String upper = text;
  upper.toUpperCase();

  // 1. Battery query
  if (upper == "BAT" || upper == "BATTERY") {
    reportBattery("QUERY");
    return;
  }

  // 2. Change Spreading Factor (5 to 12)
  if (upper.startsWith("SF=") || upper.startsWith("SF ")) {
    int newSf = upper.substring(3).toInt();
    if (newSf >= 5 && newSf <= 12) {
      int16_t state = radio.setSpreadingFactor(newSf);
      startRadioReceive();
      if (state == RADIOLIB_ERR_NONE) {
        currentSF = newSf;
        notifyCsv("CFG", NODE_ID, "Spreading Factor set to SF" + String(newSf), "N/A", "OK");
      } else {
        notifyCsv("CFG", NODE_ID, "Failed to set SF (code " + String(state) + ")", "N/A", "ERR");
      }
    } else {
      notifyCsv("CFG", NODE_ID, "Invalid SF (Allowed: 5 to 12)", "N/A", "ERR");
    }
    return;
  }

  // 3. Change Bandwidth (7.8 to 500.0)
  if (upper.startsWith("BD=") || upper.startsWith("BW=") || upper.startsWith("BD ") || upper.startsWith("BW ")) {
    float newBw = upper.substring(3).toFloat();
    if (newBw >= 7.8f && newBw <= 500.0f) {
      int16_t state = radio.setBandwidth(newBw);
      startRadioReceive();
      if (state == RADIOLIB_ERR_NONE) {
        currentBW = newBw;
        notifyCsv("CFG", NODE_ID, "Bandwidth set to " + String(newBw, 1) + " kHz", "N/A", "OK");
      } else {
        notifyCsv("CFG", NODE_ID, "Failed to set BW (code " + String(state) + ")", "N/A", "ERR");
      }
    } else {
      notifyCsv("CFG", NODE_ID, "Invalid BW (Common: 125, 250, 500 kHz)", "N/A", "ERR");
    }
    return;
  }

  // 4. Query active config
  if (upper == "CFG" || upper == "CONFIG" || upper == "STATUS") {
    String cfgStr = "SF=" + String(currentSF) + ", BW=" + String(currentBW, 1) + "kHz";
    notifyCsv("CFG", NODE_ID, cfgStr, "N/A", "OK");
    return;
  }

  // Regular chat message to transmit over LoRa
  queueOutgoing(text);
}

String bleInBuf = "";
String serInBuf = "";
uint32_t lastPeriodicBatMs = 0;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // LED off

  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && (millis() - start < 3000));

  Serial.println("Timestamp_ms,Direction,Node,Message,RSSI_dBm,Status");

  // 1. Initialize BLE
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName(BLE_NAME);
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
  bleuart.begin();
  startAdv();

  // 2. Initialize SX1262 LoRa
  SPI.begin();
  int16_t state = radio.begin(
    LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR,
    RADIOLIB_SX126X_SYNC_WORD_PRIVATE, LORA_POWER_DBM, 8, 1.8
  );

  if (state != RADIOLIB_ERR_NONE) {
    while (true) delay(1000);
  }

  // Attach DIO1 packet received action and enter continuous hardware RX mode
  radio.setPacketReceivedAction(onRadioPacketReceived);
  startRadioReceive();

  // Initial battery report on boot
  reportBattery("BOOT");
}

void loop() {
  while (bleuart.available()) {
    char c = (char)bleuart.read();
    if (c == '\r') continue;
    if (c == '\n') {
      processUserInput(bleInBuf);
      bleInBuf = "";
    } else {
      if (bleInBuf.length() < 120) bleInBuf += c;
    }
  }

  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      processUserInput(serInBuf);
      serInBuf = "";
    } else {
      if (serInBuf.length() < 120) serInBuf += c;
    }
  }

  checkLoRaRx();
  handleRetries();

  uint32_t now = millis();
  if (now - lastPeriodicBatMs >= 60000) {
    lastPeriodicBatMs = now;
    reportBattery("PERIODIC");
  }

  delay(2);
}
