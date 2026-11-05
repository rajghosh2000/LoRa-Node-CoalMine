import serial
import time
import sys
import json

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

PORT_A = 'COM27'
PORT_B = 'COM29'
BAUD = 115200

report = {
    "command_tests": [],
    "rf_tests": {},
    "battery_status": {},
    "mismatch_test": {}
}

def open_ports():
    sA = serial.Serial(PORT_A, BAUD, timeout=0.1)
    sA.dtr = True
    sA.rts = True
    sB = serial.Serial(PORT_B, BAUD, timeout=0.1)
    sB.dtr = True
    sB.rts = True
    time.sleep(0.5)
    # Clear initial buffers
    sA.reset_input_buffer()
    sB.reset_input_buffer()
    return sA, sB

def send_and_wait(ser_tx, ser_rx, tx_msg, timeout=3.0):
    ser_tx.reset_input_buffer()
    ser_rx.reset_input_buffer()
    t_start = time.time()
    ser_tx.write(tx_msg.encode('utf-8') + b'\n')
    
    rx_lines = []
    tx_lines = []
    delivered = False
    received = False
    rssi = None
    
    t_end = time.time() + timeout
    while time.time() < t_end:
        if ser_tx.in_waiting:
            line = ser_tx.readline().decode('utf-8', errors='replace').strip()
            if line:
                tx_lines.append(line)
                if "DELIVERED" in line:
                    delivered = True
        if ser_rx.in_waiting:
            line = ser_rx.readline().decode('utf-8', errors='replace').strip()
            if line:
                rx_lines.append(line)
                if "RECEIVED" in line:
                    received = True
                    # Extract RSSI
                    parts = line.split(',')
                    if len(parts) >= 5:
                        try:
                            rssi = float(parts[4])
                        except:
                            pass
        if delivered and received:
            break
        time.sleep(0.01)
        
    latency_ms = int((time.time() - t_start) * 1000)
    return {
        "delivered": delivered,
        "received": received,
        "latency_ms": latency_ms,
        "rssi": rssi,
        "tx_lines": tx_lines,
        "rx_lines": rx_lines
    }

def send_cmd(ser, cmd, timeout=1.5):
    ser.reset_input_buffer()
    ser.write(cmd.encode('utf-8') + b'\n')
    lines = []
    t_end = time.time() + timeout
    while time.time() < t_end:
        if ser.in_waiting:
            line = ser.readline().decode('utf-8', errors='replace').strip()
            if line:
                lines.append(line)
                if any(x in line for x in ["CFG", "BAT", "ERR"]):
                    break
        time.sleep(0.02)
    return lines

print("==================================================")
print("       LORA RIGOROUS AUTOMATED TEST SUITE         ")
print("==================================================")

sA, sB = open_ports()

# --------------------------------------------------
# PHASE 1: COMMAND & BATTERY VALIDATION
# --------------------------------------------------
print("\n[PHASE 1] Validating Commands, Configuration & Battery...")

# Battery Node A
linesA = send_cmd(sA, "BAT")
print("  Node A Battery Query:", linesA)
report["battery_status"]["Node_A"] = linesA

# Battery Node B
linesB = send_cmd(sB, "BAT")
print("  Node B Battery Query:", linesB)
report["battery_status"]["Node_B"] = linesB

# Query default CFG
cfgA = send_cmd(sA, "CFG")
print("  Node A Initial Config:", cfgA)
cfgB = send_cmd(sB, "CFG")
print("  Node B Initial Config:", cfgB)

# Invalid command testing
inv_sf = send_cmd(sA, "SF=25")
print("  Node A Invalid SF=25 response:", inv_sf)
report["command_tests"].append({"cmd": "SF=25", "resp": inv_sf, "valid": any("Invalid" in l or "ERR" in l for l in inv_sf)})

inv_bd = send_cmd(sA, "BD=9999")
print("  Node A Invalid BD=9999 response:", inv_bd)
report["command_tests"].append({"cmd": "BD=9999", "resp": inv_bd, "valid": any("Invalid" in l or "ERR" in l for l in inv_bd)})

# --------------------------------------------------
# PHASE 2: RF BIDIRECTIONAL TEST AT MULTIPLE CONFIGS
# --------------------------------------------------
configs = [
    {"name": "Default (SF7, BW 125kHz)", "sf": 7, "bw": 125.0},
    {"name": "High-Speed (SF7, BW 250kHz)", "sf": 7, "bw": 250.0},
    {"name": "Long-Range (SF8, BW 125kHz)", "sf": 8, "bw": 125.0}
]

for cfg in configs:
    name = cfg["name"]
    sf = cfg["sf"]
    bw = cfg["bw"]
    print(f"\n[PHASE 2] Testing Configuration: {name}...")
    
    # Set SF and BW on both nodes
    send_cmd(sA, f"SF={sf}")
    send_cmd(sA, f"BD={bw}")
    send_cmd(sB, f"SF={sf}")
    send_cmd(sB, f"BD={bw}")
    time.sleep(0.5)
    
    test_results = []
    # 6 messages: alternating A->B and B->A
    for i in range(1, 7):
        if i % 2 == 1:
            direction = "A->B"
            msg = f"TEST_{sf}_{int(bw)}_A{i}"
            res = send_and_wait(sA, sB, msg, timeout=3.5)
        else:
            direction = "B->A"
            msg = f"TEST_{sf}_{int(bw)}_B{i}"
            res = send_and_wait(sB, sA, msg, timeout=3.5)
            
        success = res["delivered"] and res["received"]
        print(f"  [{direction} #{i}] Result: {'PASS' if success else 'FAIL'} | Latency: {res['latency_ms']}ms | RSSI: {res['rssi']} dBm")
        test_results.append({
            "idx": i,
            "dir": direction,
            "pass": success,
            "latency_ms": res["latency_ms"],
            "rssi": res["rssi"]
        })
        time.sleep(0.3)
        
    pdr = (sum(1 for r in test_results if r["pass"]) / len(test_results)) * 100.0
    valid_lat = [r["latency_ms"] for r in test_results if r["pass"]]
    valid_rssi = [r["rssi"] for r in test_results if r["pass"] and r["rssi"] is not None]
    
    report["rf_tests"][name] = {
        "pdr_percent": pdr,
        "avg_latency_ms": sum(valid_lat) / len(valid_lat) if valid_lat else None,
        "avg_rssi_dbm": sum(valid_rssi) / len(valid_rssi) if valid_rssi else None,
        "details": test_results
    }
    print(f"  ==> PDR: {pdr:.1f}% | Avg Latency: {report['rf_tests'][name]['avg_latency_ms']:.1f}ms | Avg RSSI: {report['rf_tests'][name]['avg_rssi_dbm']:.1f}dBm")

# --------------------------------------------------
# PHASE 3: ORTHOGONAL SF ISOLATION TEST (MISMATCH)
# --------------------------------------------------
print("\n[PHASE 3] Testing Orthogonal Spreading Factor Isolation (Mismatch Test)...")
print("  Setting Node A to SF=9 while Node B remains at SF=8...")
send_cmd(sA, "SF=9")
send_cmd(sB, "SF=8")
time.sleep(0.5)

print("  Transmitting from Node A to Node B (Should NOT be received by Node B)...")
mismatch_res = send_and_wait(sA, sB, "MISMATCH_SHOULD_FAIL", timeout=3.0)
iso_pass = (not mismatch_res["received"])
print(f"  Isolation Result: {'PASS (Correctly Ignored)' if iso_pass else 'FAIL (Unexpected reception)'}")
report["mismatch_test"] = {
    "isolation_verified": iso_pass,
    "details": mismatch_res
}

# --------------------------------------------------
# PHASE 4: CLEANUP & RESTORE DEFAULTS
# --------------------------------------------------
print("\n[PHASE 4] Restoring both nodes to default (SF7, BW 125kHz)...")
send_cmd(sA, "SF=7")
send_cmd(sA, "BD=125")
send_cmd(sB, "SF=7")
send_cmd(sB, "BD=125")
time.sleep(0.5)

final_check = send_and_wait(sA, sB, "FINAL_HEALTH_CONFIRM", timeout=3.0)
print(f"  Final Round-Trip Check: {'PASS' if final_check['delivered'] else 'FAIL'}")

sA.close()
sB.close()

with open("test_report.json", "w") as f:
    json.dump(report, f, indent=2)

print("\n==================================================")
print("   RIGOROUS TEST SUITE COMPLETED SUCCESSFULLY!    ")
print("==================================================")
