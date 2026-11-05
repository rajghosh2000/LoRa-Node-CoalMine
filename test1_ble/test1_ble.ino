/*
  Test 1: XIAO nRF52840 BLE UART Echo Test
  Device Name: "XIAO_Chat_A" (or B)
  Connect with: Adafruit Bluefruit Connect app, nRF Connect, or Serial Bluetooth Terminal
*/

#include <bluefruit.h>

BLEUart bleuart;

void connect_callback(uint16_t conn_handle) {
  BLEConnection* connection = Bluefruit.Connection(conn_handle);
  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));
  Serial.print("BLE Connected to: ");
  Serial.println(central_name);
  bleuart.println("Hello from XIAO nRF52840! Type a message and hit send.");
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason) {
  (void) conn_handle;
  (void) reason;
  Serial.print("BLE Disconnected, reason = 0x");
  Serial.println(reason, HEX);
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

void setup() {
  Serial.begin(115200);
  // Wait up to 3 seconds for Serial Monitor if connected
  unsigned long start = millis();
  while (!Serial && (millis() - start < 3000));

  Serial.println("\n=== Test 1: XIAO nRF52840 BLE Test ===");

  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName("XIAO_LoRa_A");

  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  bleuart.begin();
  startAdv();

  Serial.println("Advertising as 'XIAO_LoRa_A'.");
  Serial.println("Use 'nRF Connect' or 'Adafruit Bluefruit LE' app on phone to test.");
}

void loop() {
  // Echo from USB Serial to BLE
  while (Serial.available()) {
    delay(2);
    uint8_t buf[64];
    int count = Serial.readBytes(buf, sizeof(buf));
    bleuart.write(buf, count);
  }

  // Echo from BLE to USB Serial & back to BLE (echo)
  while (bleuart.available()) {
    uint8_t ch = (uint8_t)bleuart.read();
    Serial.write(ch);
  }
}
