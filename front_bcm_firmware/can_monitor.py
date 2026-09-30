import serial
import time
import sys
import re
import os

try:
    import colorama
    colorama.init()
except ImportError:
    pass

PORT     = 'COM9'
BAUDRATE = 115200

# Fix console encoding on Windows to prevent Emoji crashes
if sys.platform == 'win32':
    sys.stdout.reconfigure(encoding='utf-8')

# --- DTC lookup tables ---
DTC_NAMES = {
    0x1001: "B1001 - Headlight open circuit (Low Beam)",
    0x1002: "B1002 - Headlight open circuit (High Beam)",
    0x1003: "B1003 - DRL circuit fault",
    0x1004: "B1004 - Fog light circuit fault",
    0x1010: "B1010 - Wiper motor stall / jam",
    0x1011: "B1011 - Wiper position sensor fault",
    0x1012: "B1012 - Washer fluid critically low",
    0x1020: "B1020 - Trunk motor stall",
    0x1021: "B1021 - Trunk hall sensor fault",
    0x1022: "B1022 - Trunk open timeout (>5s)",
    0x1030: "B1030 - Turn signal relay fault (Left)",
    0x1031: "B1031 - Turn signal relay fault (Right)",
    0x1040: "B1040 - HC-SR04 ultrasonic read fail",
    0x1041: "B1041 - Reverse radar out of range",
    0x1050: "B1050 - Brake light circuit fault",
    0x2001: "C1001 - CAN bus-off",
    0x2002: "C1002 - Front BCM heartbeat timeout",
    0x2003: "C1003 - Rear BCM heartbeat timeout",
    0x2004: "C1004 - Translator/Qt link lost",
}

DTC_SEVERITY = { 0: "INFO", 1: "WARNING", 2: "ERROR", 3: "FATAL" }
ERR_CODES    = { 0x01: "ERR_SHORT_GND", 0x02: "ERR_SHORT_BAT", 0x03: "ERR_OPEN_LOAD", 0x04: "ERR_OVER_TEMP", 0x05: "ERR_OVER_CURR", 0x06: "ERR_TIMEOUT", 0x07: "ERR_RADAR_FAIL", 0x08: "ERR_TRUNK_TIMEOUT", 0x09: "ERR_CAN_BUS_OFF" }

WIPER_MODES = { 0: "OFF", 1: "INTERMITTENT", 2: "SLOW", 3: "NORMAL", 4: "FAST", 5: "AUTO" }
LIGHT_CMDS  = { 0: "OFF", 1: "DRL_ON", 2: "LOW_BEAM", 3: "HIGH_BEAM", 4: "FOG_ON", 5: "FOG_OFF", 6: "AUTO_MODE" }
TURN_CMDS   = { 0: "TURN_OFF", 1: "LEFT", 2: "RIGHT", 3: "HAZARD" }
TRUNK_CMDS  = { 0: "NO_ACTION", 1: "OPEN", 2: "CLOSE", 3: "STOP" }

def parse_packet(line):
    # Expects format like: CAN:300:7:01A30007554192
    m = re.match(r"CAN:([0-9A-Fa-f]+):(\d):([0-9A-Fa-f]*)", line)
    if not m:
        return None, []
    
    can_id_hex = m.group(1).zfill(3).lower() # standard 3-digit hex ID
    data_str = m.group(3)
    
    # parse hex string into list of integers
    b = [int(data_str[i:i+2], 16) for i in range(0, len(data_str), 2)]
    return can_id_hex, b

def decode_0x400(b):
    if len(b) < 3: return ""
    flags, wiper, bri = b[0], b[1], b[2]
    parts = []
    if flags & 0x01: parts.append("DRL")
    if flags & 0x02: parts.append("LOW_BEAM")
    if flags & 0x04: parts.append("FOG")
    if flags & 0x08: parts.append("WIPER_ON")
    if flags & 0x10: parts.append("WASHER")
    if flags & 0x20: parts.append("LEFT_TURN")
    if flags & 0x40: parts.append("RIGHT_TURN")
    if flags & 0x80: parts.append("HIGH_BEAM")
    lights = " | ".join(parts) if parts else "all OFF"
    return f"Lights=[{lights}]  Wiper={WIPER_MODES.get(wiper, f'0x{wiper:02X}')}  Brightness={bri}%"

def decode_0x401(b):
    if len(b) < 2: return ""
    rain_label  = "🌧️ RAINING" if b[0] > 0 else "☀️ DRY"
    water_label = "⚠️ LOW"     if b[1] < 20 else "✅ OK"
    return f"Rain={b[0]}% ({rain_label})  WaterLevel={b[1]}% ({water_label})"

def decode_0x500(b):
    if len(b) < 5: return ""
    sev = DTC_SEVERITY.get(b[0], f"0x{b[0]:02X}")
    dtc = (b[1] << 8) | b[2]
    name = DTC_NAMES.get(dtc, f"UNKNOWN DTC 0x{dtc:04X}")
    return f"[{sev}] {name}  err={ERR_CODES.get(b[4], f'0x{b[4]:02X}')}  count={b[3]}"

def decode_0x710(b):
    if len(b) < 2: return ""
    bits = []
    if b[1] & 0x01: bits.append("INIT_OK")
    if b[1] & 0x02: bits.append("CAN_OK")
    if b[1] & 0x04: bits.append("SENSORS_OK")
    if b[1] & 0x08: bits.append("DTC_ACTIVE ⚠️")
    return f"uptime={b[0]}  flags=[{' | '.join(bits) or 'NONE'}]"

def decode_rx_cmd(can_id, b):
    if not b: return ""
    if can_id == "100":
        return f"LIGHT 💡 cmd={LIGHT_CMDS.get(b[0], f'0x{b[0]:02X}')}  brightness={b[1] if len(b)>1 else '?'}%"
    if can_id == "101": return f"WIPER 🌧️ mode={WIPER_MODES.get(b[0], f'0x{b[0]:02X}')}"
    if can_id == "102": return f"TURN  ⬅️ {TURN_CMDS.get(b[0], f'0x{b[0]:02X}')}"
    if can_id == "103": return f"TRUNK 🧳 {TRUNK_CMDS.get(b[0], f'0x{b[0]:02X}')}"
    if can_id == "105":
        ack = {0: "APPROVED", 1: "REJECTED", 2: "PENDING"}.get(b[1], "UNKNOWN")
        val = {0: "OK", 1: "BAD_GEAR", 2: "SPEED", 3: "DTC_ACTIVE", 4: "RESOURCE_BUSY", 5: "UNKNOWN_CMD", 6: "SAFETY_LOCK", 7: "BCM_OFFLINE"}.get(b[2], "UNKNOWN")
        return f"ACK 🛡️ cmd=0x10{b[0]} status={ack} reason={val}"
    return ""

def decode_0x610(b):
    if len(b) < 2: return "Invalid Watchdog payload"
    nodes = {0: "TRANSLATOR", 1: "CENTRAL_ECU", 2: "FRONT_BCM", 3: "REAR_BCM"}
    node_name = nodes.get(b[0], f"UNKNOWN_NODE_{b[0]}")
    return f"{node_name} is offline! (Silent for {b[1]}s)"

def decode_0x301(b):
    if not b: return ""
    return f"Lights State Flags=0x{b[0]:02X}"

def decode_0x302(b):
    if len(b) < 2: return ""
    states = {0: "CLOSED", 1: "OPENING", 2: "OPEN", 3: "CLOSING"}
    return f"Trunk State={states.get(b[0], 'UNKNOWN')} ({b[1]}% open)"

def decode_0x303(b):
    if len(b) < 2: return ""
    dist = (b[0] << 8) | b[1]
    return f"Radar Distance = {dist} cm"

def decode_heartbeat(b, node_name):
    if len(b) < 2: return ""
    bits = []
    if b[1] & 0x01: bits.append("INIT_OK")
    if b[1] & 0x02: bits.append("CAN_OK")
    if b[1] & 0x04: bits.append("SENSORS_OK")
    if b[1] & 0x08: bits.append("DTC_ACTIVE ⚠️")
    return f"[{node_name}] uptime={b[0]}s flags=[{' | '.join(bits) or 'NONE'}]"

def main():
    try:
        ser = serial.Serial(PORT, BAUDRATE, timeout=1)
        print(f"\033[92m--- Connected to {PORT} @ {BAUDRATE} baud ---\033[0m")
        print("Monitoring CAN Bus (Smart Spam Filter Active)\n")
    except Exception as e:
        print(f"\033[91mCannot open port {PORT}. Error: {e}\033[0m")
        sys.exit(1)

    msg_cache = {}
    msg_time = {}

    while True:
        try:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if not line: continue
            
            now = time.time()
            can_id, b = parse_packet(line)
            
            if not can_id:
                print(f"\033[90m[*] {line}\033[0m")
                continue

            # --- SMART SPAM FILTER ---
            # If payload changed, print immediately. If identical, print only once per 1.5 seconds.
            key = tuple(b)
            if key == msg_cache.get(can_id) and (now - msg_time.get(can_id, 0)) < 1.5:
                continue
            
            msg_cache[can_id] = key
            msg_time[can_id] = now

            # --- DECODERS ---
            if can_id in ("700", "710", "720"):
                node = {"700": "CENTRAL", "710": "FRONT", "720": "REAR"}.get(can_id)
                print(f"\033[90m🩺 [HEARTBEAT] ID:0x{can_id} Data:{b}\033[0m")
                print(f"\033[90m           ↳ {decode_heartbeat(b, node)}\033[0m")
                continue

            if can_id == "130":
                print(f"\033[95m⏱️ [SYNC] Blink Tick (0x130)\033[0m")
                continue

            if can_id in ("500", "510"):
                print(f"\033[91;1m🚨 [FAULT] ID:0x{can_id} Data:{b}\033[0m")
                print(f"\033[91;1m       ↳ {decode_0x500(b)}\033[0m")
                continue

            if can_id == "610":
                print(f"\033[91;1m⚠️ [WATCHDOG] ID:0x610 Data:{b}\033[0m")
                print(f"\033[91;1m          ↳ {decode_0x610(b)}\033[0m")
                continue

            if can_id == "300":
                gears = {0: "P", 1: "R", 2: "N", 3: "D", 4: "S"}
                gear = gears.get(b[0], "?") if b else "?"
                speed = (b[2] << 8) | b[3] if len(b) >= 4 else 0
                fuel = b[4] if len(b) >= 5 else 0
                print(f"\033[94m🏎️ [VEHICLE] ID:0x300 Data:{b}\033[0m")
                print(f"\033[94m         ↳ Gear={gear} | Speed={speed} km/h | Fuel={fuel}%\033[0m")
                continue
                
            if can_id == "301":
                print(f"\033[94m💡 [LIGHTS_ST] ID:0x301 Data:{b} ↳ {decode_0x301(b)}\033[0m")
                continue
            if can_id == "302":
                print(f"\033[94m🧳 [TRUNK_ST]  ID:0x302 Data:{b} ↳ {decode_0x302(b)}\033[0m")
                continue
            if can_id == "303":
                print(f"\033[94m📡 [RADAR_ST]  ID:0x303 Data:{b} ↳ {decode_0x303(b)}\033[0m")
                continue

            if can_id == "400":
                print(f"\033[92;1m🚘 [STATUS] ID:0x400 Data:{b}\033[0m")
                print(f"\033[92m        ↳ {decode_0x400(b)}\033[0m")
                continue

            if can_id == "401":
                print(f"\033[93m🌡️ [SENSOR] ID:0x401 Data:{b}\033[0m")
                print(f"\033[93m        ↳ {decode_0x401(b)}\033[0m")
                continue

            if can_id.startswith("10"):
                decoded = decode_rx_cmd(can_id, b)
                print(f"\033[96m🎮 [CMD] ID:0x{can_id} Data:{b}\033[0m")
                if decoded:
                    print(f"\033[96m     ↳ {decoded}\033[0m")
                continue

            print(f"[*] ID:0x{can_id} | Data:{b}")

        except KeyboardInterrupt:
            print("\n--- Monitoring stopped ---")
            ser.close()
            break
        except Exception:
            pass 

if __name__ == '__main__':
    main()
