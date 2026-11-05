import serial
import time
import sys
import json

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

PORT_A = 'COM27'
PORT_B = 'COM29'
BAUD = 115200

def open_ports():
    sA = serial.Serial(PORT_A, BAUD, timeout=0.1)
    sA.dtr = True
    sA.rts = True
    sB = serial.Serial(PORT_B, BAUD, timeout=0.1)
    sB.dtr = True
    sB.rts = True
    time.sleep(0.5)
    sA.reset_input_buffer()
    sB.reset_input_buffer()
    return sA, sB

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

def send_and_wait(ser_tx, ser_rx, tx_msg, timeout=5.0):
    ser_tx.reset_input_buffer()
    ser_rx.reset_input_buffer()
    t_start = time.time()
    ser_tx.write(tx_msg.encode('utf-8') + b'\n')
    
    delivered = False
    received = False
    rssi = None
    
    t_end = time.time() + timeout
    while time.time() < t_end:
        if ser_tx.in_waiting:
            line = ser_tx.readline().decode('utf-8', errors='replace').strip()
            if line and "DELIVERED" in line:
                delivered = True
        if ser_rx.in_waiting:
            line = ser_rx.readline().decode('utf-8', errors='replace').strip()
            if line and "RECEIVED" in line:
                received = True
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
        "rssi": rssi
    }

print("==================================================")
print("     COMPLETE SPREADING FACTOR SWEEP (SF5-SF12)   ")
print("==================================================")

sA, sB = open_ports()

# List of all valid Spreading Factors on SX1262
SF_LIST = [5, 6, 7, 8, 9, 10, 11, 12]

results = {}

# Timeouts scale with SF because ToA doubles at each step
TIMEOUTS = {
    5: 3.0,
    6: 3.0,
    7: 3.5,
    8: 4.5,
    9: 6.0,
    10: 8.5,
    11: 14.0,
    12: 22.0
}

for sf in SF_LIST:
    print(f"\n--- Testing Spreading Factor: SF{sf} ---")
    
    # 1. Reconfigure both nodes
    cmd_sf = f"SF={sf}"
    respA = send_cmd(sA, cmd_sf)
    respB = send_cmd(sB, cmd_sf)
    time.sleep(0.3)
    
    # Verify active config
    cfgA = send_cmd(sA, "CFG")
    print(f"  Node A Config: {cfgA}")
    
    timeout = TIMEOUTS.get(sf, 5.0)
    
    # Test Node A -> Node B
    msg_a2b = f"SF{sf}_PING_A2B"
    res1 = send_and_wait(sA, sB, msg_a2b, timeout=timeout)
    p1 = res1["delivered"] and res1["received"]
    print(f"  [A -> B] Result: {'PASS' if p1 else 'FAIL'} | Latency: {res1['latency_ms']}ms | RSSI: {res1['rssi']} dBm")
    
    time.sleep(0.3)
    
    # Test Node B -> Node A
    msg_b2a = f"SF{sf}_PONG_B2A"
    res2 = send_and_wait(sB, sA, msg_b2a, timeout=timeout)
    p2 = res2["delivered"] and res2["received"]
    print(f"  [B -> A] Result: {'PASS' if p2 else 'FAIL'} | Latency: {res2['latency_ms']}ms | RSSI: {res2['rssi']} dBm")
    
    all_pass = p1 and p2
    avg_latency = (res1["latency_ms"] + res2["latency_ms"]) / 2.0
    
    results[f"SF{sf}"] = {
        "status": "PASS" if all_pass else "FAIL",
        "a_to_b": res1,
        "b_to_a": res2,
        "avg_latency_ms": avg_latency
    }
    print(f"  ==> SF{sf} Overall: {'100% PASS' if all_pass else 'FAIL'}")

# Restore back to default SF7
print("\n--- Restoring both nodes to default SF7, BW 125kHz ---")
send_cmd(sA, "SF=7")
send_cmd(sA, "BD=125")
send_cmd(sB, "SF=7")
send_cmd(sB, "BD=125")
time.sleep(0.5)

final_res = send_and_wait(sA, sB, "SF_SWEEP_COMPLETE", timeout=3.5)
print(f"Final baseline check: {'PASS' if final_res['delivered'] else 'FAIL'}")

sA.close()
sB.close()

with open("sf_sweep_results.json", "w") as f:
    json.dump(results, f, indent=2)

print("\n==================================================")
print("     ALL SPREADING FACTORS TEST COMPLETED!        ")
print("==================================================")
