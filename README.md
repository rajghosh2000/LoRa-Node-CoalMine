# LoRa Node CoalMine: Off-Grid Emergency Communication & Telemetry Gateway

[![Hardware: Seeed XIAO nRF52840](https://img.shields.io/badge/Hardware-Seeed_XIAO_nRF52840-008080.svg)](https://wiki.seeedstudio.com/XIAO_BLE/)
[![Transceiver: Semtech SX1262](https://img.shields.io/badge/LoRa-Semtech_SX1262_(868MHz)-blue.svg)](https://wiki.seeedstudio.com/xiao_esp32s3_wio_sx1262/)
[![Bluetooth: BLE 5.0 UART](https://img.shields.io/badge/Wireless-BLE_5.0_Nordic_UART-purple.svg)](https://www.nordicsemi.com/Products/nRF52840)
[![Data Logging: Pure CSV](https://img.shields.io/badge/Output-Structured_CSV-success.svg)](#csv-data-format)

An ultra-reliable, off-grid **LoRa-to-BLE communication and data-logging system** designed for hazardous and underground environments such as **coal mines, tunnels, and disaster relief zones** where cellular, Wi-Fi, and internet infrastructure are non-existent.

---

## 📌 System Architecture

```text
[ Miner / Field Phone A ]                     [ Miner / Surface Phone B ]
        │                                                  │
        │ Bluetooth Low Energy (BLE UART)                  │ Bluetooth Low Energy (BLE UART)
        ▼                                                  ▼
┌────────────────────────┐      LoRa P2P Link       ┌────────────────────────┐
│  Node A (Transceiver)  │  ◄════════════════════►  │  Node B (Transceiver)  │
│  • XIAO nRF52840       │     868.0 MHz (SX1262)   │  • XIAO nRF52840       │
│  • Wio-SX1262 (14dBm)  │   Penetrates deep walls  │  • Wio-SX1262 (14dBm)  │
│  • LiPo Battery Monitor│   SF5–SF12, BW 125–500k  │  • LiPo Battery Monitor│
└────────────────────────┘                          └────────────────────────┘
```

---

## 🚀 Key Features

* **100% Off-Grid & Zero Internet Required**: Direct device-to-device wireless communication without cellular networks, Wi-Fi access points, or cloud servers.
* **Dual Wireless Connectivity**:
  * **Long-Range Sub-GHz LoRa (Semtech SX1262)**: Punches through rock, coal seams, and thick reinforced concrete barriers.
  * **Short-Range BLE (Nordic nRF52840)**: Directly connects to smartphones, tablets, or laptops over Bluetooth.
* **Hardware-Driven Reliability (ACK & Retries)**:
  * Full end-to-end packet delivery receipts (`DELIVERED`).
  * Automatic retry mechanism with jittered backoff collision prevention.
  * Ring-buffer duplicate packet suppression to prevent retransmission noise.
* **Continuous Hardware Interrupt Reception**:
  * Utilizes hardware interrupts on `DIO1` and SX1262 continuous reception mode for zero dropped packets and ultra-low CPU power consumption.
* **On-Board LiPo Battery Management**:
  * Gated voltage-divider monitoring on `PIN_VBAT` (P0.31) and `VBAT_ENABLE` (P0.14).
  * On-demand battery status query (`BAT`) and automatic 60-second periodic telemetry row.
* **Dynamic Runtime Configuration**:
  * Adjust Spreading Factor (`SF=5` to `SF=12`) and Bandwidth (`BD=125` to `BD=500` kHz) on the fly directly via text commands without rebooting or recompiling.
  * Adaptive ACK timeout scaling automatically accounts for packet Time-on-Air (ToA).
* **Direct-to-Spreadsheet CSV Output**:
  * Streams pure CSV rows to the mobile terminal for direct import into Excel or Google Sheets for audit, safety tracking, and incident logging.

---

## 🛠️ Hardware Specifications & Pin Mapping

### Microcontroller & Radio
* **MCU**: [Seeed Studio XIAO nRF52840](https://wiki.seeedstudio.com/XIAO_BLE/) (Nordic nRF52840 ARM Cortex-M4 with FPU @ 64 MHz, BLE 5.0)
* **LoRa Transceiver**: [Seeed Studio Wio-SX1262 Module](https://wiki.seeedstudio.com/xiao_esp32s3_wio_sx1262/) (Semtech SX1262, 868 MHz band, 1.8V TCXO)
* **Power Source**: 3.7V LiPo / Li-Ion Battery or 5V USB-C

### Pinout Mapping (XIAO nRF52840 to Wio-SX1262)

| SX1262 Pin | XIAO Pin | nRF52840 GPIO | Function |
| :--- | :--- | :--- | :--- |
| **NSS / CS** | `D4` | `P0.04` | SPI Chip Select |
| **DIO1 / IRQ**| `D1` | `P0.03` | Packet Received Interrupt |
| **RESET** | `D2` | `P0.28` | Radio Hardware Reset |
| **BUSY** | `D3` | `P0.29` | Transceiver Busy Status |
| **SCK** | `D8` | `P0.13` | Hardware SPI Clock |
| **MISO** | `D9` | `P0.15` | Hardware SPI MISO |
| **MOSI** | `D10` | `P0.17` | Hardware SPI MOSI |
| **VBAT Sense** | `PIN_VBAT` | `P0.31` (AIN7)| Analog Battery Voltage Divider |
| **VBAT Enable**| `VBAT_ENABLE`| `P0.14` | Low-Power Voltage Divider Gate |

---

## 📊 CSV Data Format

All communication across Bluetooth and USB Serial streams in RFC-compliant CSV format:

```csv
Timestamp_ms,Direction,Node,Message,RSSI_dBm,Status
```

### Example Log Output
```csv
14205,BAT,A,"4.14V (92%)",N/A,CONNECT
15420,TX,A,"Tunnel Shaft 4 clear",N/A,SENT
15593,ACK,B,"msg #1",-81.0,DELIVERED
17800,RX,B,"Surface base received",-82.0,RECEIVED
75420,BAT,A,"4.13V (91%)",N/A,PERIODIC
```

### Field Definitions
1. **`Timestamp_ms`**: Milliseconds elapsed since the node booted up.
2. **`Direction`**:
   * `TX`: Outgoing message sent by this node.
   * `RX`: Ingoing message received from the remote node over LoRa.
   * `ACK`: Delivery acknowledgment confirmed by the remote node.
   * `BAT`: Battery telemetry event.
   * `CFG`: Configuration change or status event.
   * `ERR`: Error / Timeout / Dropped packet notification.
3. **`Node`**: ID of the reporting or target node (`A` or `B`).
4. **`Message`**: Content payload (escaped in `""`).
5. **`RSSI_dBm`**: Radio signal strength at reception (e.g. `-81.0 dBm`).
6. **`Status`**: `SENT`, `RECEIVED`, `DELIVERED`, `QUERY`, `PERIODIC`, `OK`, or `TIMEOUT`.

---

## 💬 Runtime Command Reference

Type any of these commands in your mobile chat terminal or USB Serial Monitor (115200 baud):

| Command | Example | Description |
| :--- | :--- | :--- |
| **`BAT`** | `BAT` | Queries battery voltage and charge percentage. |
| **`SF=<val>`** | `SF=8` or `SF=10` | Adjusts Spreading Factor runtime (**5 to 12**). |
| **`BD=<val>`** | `BD=250` or `BD=500` | Adjusts LoRa Bandwidth in kHz (**125**, **250**, **500**). |
| **`CFG`** | `CFG` or `STATUS` | Returns the currently active SF and Bandwidth. |
| *`<any text>`*| `Worker in zone 2` | Transmits text to the remote node over LoRa. |

> [!IMPORTANT]
> Both nodes must be configured with the **same Spreading Factor and Bandwidth** to communicate with each other. If you change Node A to `SF=8`, set Node B to `SF=8` as well.

---

## 📱 Mobile Connection Guide (100% Offline)

### Recommended App: **Serial Bluetooth Terminal** (Android)
1. Download [Serial Bluetooth Terminal](https://play.google.com/store/apps/details?id=de.kai_morich.serial_bluetooth_terminal) from Google Play.
2. Open the app, tap **Menu ($\equiv$) $\rightarrow$ Devices**.
3. Select the **Bluetooth LE** tab and tap **Scan**.
4. Select **`XIAO_Chat_A`** (for Node A) or **`XIAO_Chat_B`** (for Node B).
5. Tap the **Connect (plug)** icon.
6. **To export as CSV**: Tap **Menu ($\vdots$) $\rightarrow$ Save**, and change the filename extension to `.csv` (e.g. `coalmine_log.csv`).

### iOS (iPhone / iPad): **Adafruit Bluefruit LE Connect**
1. Install **Adafruit Bluefruit LE Connect** from the App Store.
2. Connect to `XIAO_Chat_A` or `XIAO_Chat_B`.
3. Open the **UART** tool to send and receive messages.

### Web Bluetooth Interface (Chrome / Edge)
Open [`web_chat/index.html`](file:///C:/Users/USER/Downloads/lora_nodes/web_chat/index.html) in your browser. Click **Connect Bluetooth**, pick your node, and click **📥 Save Log as CSV** anytime to download the spreadsheet directly to your phone or laptop.

---

## 🔬 Benchmark & Multi-SF Rigorous Validation Report

Every Spreading Factor supported by the SX1262 was rigorously benchmarked over the air between Node A and Node B:

| Spreading Factor | Time-on-Air / Round-Trip | Packet Delivery Ratio | Signal (RSSI) | Target Application |
| :---: | :---: | :---: | :---: | :--- |
| **SF5** | **`96 ms`** | **100%** | `-92.0 dBm` | High-frequency telemetry / instant alerts |
| **SF6** | **`128 ms`** | **100%** | `-93.0 dBm` | Low-latency team messaging |
| **SF7** *(Default)* | **`173 ms`** | **100%** | `-93.0 dBm` | Balanced long-range voice/text equivalents |
| **SF8** | **`246 ms`** | **100%** | `-92.0 dBm` | Heavy tunnel corridor penetration |
| **SF9** | **`395 ms`** | **100%** | `-92.0 dBm` | Deep underground shafts |
| **SF10** | **`716 ms`** | **100%** | `-91.5 dBm` | Severe obstacle penetration |
| **SF11** | **`1,356 ms`** | **100%** | `-90.0 dBm` | Maximum penetration across collapsed rock |
| **SF12** | **`2,607 ms`** | **100%** | `-90.0 dBm` | Extreme survival link (-137 dBm sensitivity) |

---

## 💻 Flashing Firmware (Arduino IDE / Arduino CLI)

### 1. Prerequisites
Add the Seeed Studio board index in Arduino IDE:
```text
https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json
```
Install:
* Core: `Seeeduino:nrf52` (v1.1.13 or newer)
* Library: `RadioLib` (v7.8.1 or newer)

### 2. Configure Node Identity
Open [`test4_ble_lora_chat/test4_ble_lora_chat.ino`](file:///C:/Users/USER/Downloads/lora_nodes/test4_ble_lora_chat/test4_ble_lora_chat.ino):
* For **Node A**: `#define IS_NODE_A true`
* For **Node B**: `#define IS_NODE_A false`

### 3. Flash via CLI
```bash
# Node A (COM27)
arduino-cli compile --fqbn Seeeduino:nrf52:xiaonRF52840 test4_ble_lora_chat
arduino-cli upload -p COM27 --fqbn Seeeduino:nrf52:xiaonRF52840 test4_ble_lora_chat

# Node B (COM29)
arduino-cli compile --fqbn Seeeduino:nrf52:xiaonRF52840 test4_ble_lora_chat
arduino-cli upload -p COM29 --fqbn Seeeduino:nrf52:xiaonRF52840 test4_ble_lora_chat
```

---

## 📂 Repository Structure

```text
├── test4_ble_lora_chat/
│   └── test4_ble_lora_chat.ino    # Production Firmware (Continuous RX + Dynamic SF/BW + Battery)
├── web_chat/
│   └── index.html                 # Offline Web Bluetooth Chat UI with 1-click CSV download
├── test1_ble/
│   └── test1_ble.ino              # Stage 1 verification: BLE UART echo test
├── test2_spi_sx1262/
│   └── test2_spi_sx1262.ino       # Stage 2 verification: SPI & SX1262 register check
├── test3_lora_node_a/
│   └── test3_lora_node_a.ino      # Stage 3 verification: LoRa Ping initiator
├── test3_lora_node_b/
│   └── test3_lora_node_b.ino      # Stage 3 verification: LoRa Pong responder
├── test_all_sf.py                 # Automated benchmark sweep for SF5 through SF12
├── rigorous_test.py               # Comprehensive validation test suite
├── sf_sweep_results.json          # Benchmark dataset across all SF configurations
├── test_report.json               # Raw JSON validation test output
└── README.md                      # Documentation & System Architecture
```

---

## 📄 License
This project is licensed under the MIT License - open-source for community, safety, and field operations.
