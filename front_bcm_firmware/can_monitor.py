import serial
import time
import sys
import re

PORT     = 'COM8'
BAUDRATE = 115200

# ─── DTC lookup tables ──────────────────────────────────────────────────────
DTC_NAMES = {
    0x1001: "B1001 – Headlight open circuit (Low Beam)",
    0x1002: "B1002 – Headlight open circuit (High Beam)",
    0x1003: "B1003 – DRL circuit fault",
    0x1004: "B1004 – Fog light circuit fault",
    0x1010: "B1010 – Wiper motor stall / jam",
    0x1011: "B1011 – Wiper position sensor fault",
    0x1012: "B1012 – Washer fluid critically low",
    0x1020: "B1020 – Trunk motor stall",
    0x1021: "B1021 – Trunk hall sensor fault",
    0x1022: "B1022 – Trunk open timeout (>5s)",
    0x1030: "B1030 – Turn signal relay fault (Left)",
    0x1031: "B1031 – Turn signal relay fault (Right)",
    0x1040: "B1040 – HC-SR04 ultrasonic read fail",
    0x1041: "B1041 – Reverse radar out of range",
    0x1050: "B1050 – Brake light circuit fault",
    0x2001: "C1001 – CAN bus-off",
    0x2002: "C1002 – Front BCM heartbeat timeout",
    0x2003: "C1003 – Rear BCM heartbeat timeout",
    0x2004: "C1004 – Translator/Qt link lost",
}

DTC_SEVERITY = { 0x00: "INFO", 0x01: "WARNING", 0x02: "ERROR", 0x03: "CRITICAL" }

ERR_CODES = {
    0x00: "ERR_NONE",
    0x01: "ERR_OPEN_CIRCUIT",
    0x02: "ERR_SENSOR_FAULT",
    0x03: "ERR_NODE_TIMEOUT",
    0x04: "ERR_OVERCURRENT",
    0x05: "ERR_ACTUATOR_STALL",
    0x06: "ERR_WIPER_STALL",
    0x07: "ERR_RADAR_FAIL",
    0x08: "ERR_TRUNK_TIMEOUT",
    0x09: "ERR_CAN_BUS_OFF",
}

WIPER_MODES = { 0: "OFF", 1: "INTERMITTENT", 2: "SLOW", 3: "NORMAL", 4: "FAST", 5: "AUTO" }
LIGHT_CMDS  = { 0: "OFF", 1: "DRL_ON", 2: "LOW_BEAM", 3: "HIGH_BEAM", 4: "FOG_ON", 5: "FOG_OFF", 6: "AUTO_MODE" }
TURN_CMDS   = { 0: "TURN_OFF", 1: "LEFT", 2: "RIGHT", 3: "HAZARD" }
TRUNK_CMDS  = { 0: "NO_ACTION", 1: "OPEN", 2: "CLOSE", 3: "STOP" }

# ─── Helpers ─────────────────────────────────────────────────────────────────
def parse_bytes(line):
    m = re.search(r"Data:\s*([0-9A-Fa-f ]+)$", line)
    if not m:
        return []
    return [int(x, 16) for x in m.group(1).strip().split()]

def parse_id(line):
    m = re.search(r"ID:(0x[0-9A-Fa-f]+)", line, re.IGNORECASE)
    return m.group(1).lower() if m else ""

# ─── Per-packet decoders ─────────────────────────────────────────────────────
def decode_0x400(b):
    """Front BCM status: actuator flags | wiper mode | brightness"""
    if len(b) < 3:
        return ""
    flags, wiper, bri = b[0], b[1], b[2]
    parts = []
    if flags & 0x01: parts.append("DRL")
    if flags & 0x02: parts.append("LOW_BEAM")
    if flags & 0x04: parts.append("FOG")
    if flags & 0x08: parts.append("WIPER_ON")
    if flags & 0x10: parts.append("WASHER")
    if flags & 0x20: parts.append("LEFT_TURN")
    if flags & 0x40: parts.append("RIGHT_TURN")
    if flags & 0x80: parts.append("⚡HIGH_BEAM")   # bit7 — added
    lights    = " | ".join(parts) if parts else "all OFF"
    wiper_str = WIPER_MODES.get(wiper, f"0x{wiper:02X}")
    return f"Lights=[{lights}]  Wiper={wiper_str}  Brightness={bri}%"

def decode_0x401(b):
    """Front BCM sensors: rain % and water level %"""
    if len(b) < 2:
        return ""
    rain, water = b[0], b[1]
    rain_label  = "🌧 RAINING" if rain > 0 else "☀ DRY"   # FIXED: was rain<50
    water_label = "⚠ LOW"     if water < 20 else "✓ OK"
    return f"Rain={rain}% ({rain_label})  WaterLevel={water}% ({water_label})"


def decode_0x500(b):
    """Fault DTC from Front BCM"""
    if len(b) < 5:
        return ""
    sev   = DTC_SEVERITY.get(b[0], f"0x{b[0]:02X}")
    dtc   = (b[1] << 8) | b[2]
    count = b[3]
    err   = ERR_CODES.get(b[4], f"0x{b[4]:02X}")
    name  = DTC_NAMES.get(dtc, f"UNKNOWN DTC 0x{dtc:04X}")
    return f"[{sev}] {name}  err={err}  count={count}"

def decode_0x710(b):
    """Heartbeat packet from Front BCM"""
    if len(b) < 2:
        return ""
    uptime, flags = b[0], b[1]
    bits = []
    if flags & 0x01: bits.append("INIT_OK")
    if flags & 0x02: bits.append("CAN_OK")
    if flags & 0x04: bits.append("SENSORS_OK")
    if flags & 0x08: bits.append("DTC_ACTIVE ⚠")
    return f"uptime={uptime}  flags=[{' | '.join(bits) or 'NONE'}]"

def decode_rx_cmd(line, b):
    """Decode CAN command received by Front BCM from Central ECU"""
    can_id = parse_id(line)
    if not b:
        return ""
    if can_id in ("0x100", "0x610"):
        cmd = LIGHT_CMDS.get(b[0], f"0x{b[0]:02X}")
        bri = b[1] if len(b) > 1 else "?"
        return f"LIGHT → cmd={cmd}  brightness={bri}%"
    if can_id == "0x101":
        return f"WIPER → mode={WIPER_MODES.get(b[0], f'0x{b[0]:02X}')}"
    if can_id == "0x102":
        return f"TURN  → {TURN_CMDS.get(b[0], f'0x{b[0]:02X}')}"
    if can_id == "0x103":
        return f"TRUNK → {TRUNK_CMDS.get(b[0], f'0x{b[0]:02X}')}"
    return ""


def main():
    try:
        ser = serial.Serial(PORT, BAUDRATE, timeout=1)
        print(f"--- Connected to {PORT} @ {BAUDRATE} baud ---")
        print("Monitoring CAN Bus  (spam filtered | data decoded)\n")
    except Exception as e:
        print(f"Cannot open port: {e}")
        sys.exit(1)

    last_hb_time   = 0
    last_400_key   = None
    last_400_time  = 0

    while True:
        try:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if not line:
                continue

            now = time.time()
            b   = parse_bytes(line)

            # ── SPAM FILTER: high-frequency Central ECU broadcasts ───────────
            if any(x in line for x in ("ID:0x130", "ID:0x300", "ID:0x700")):
                continue

            # ── 0x710  Heartbeat ─────────────────────────────────────────────
            if "ID:0x710" in line:
                if now - last_hb_time >= 2.0:
                    last_hb_time = now
                    print(f"\033[90m💓 [HEARTBEAT] {line}\033[0m")
                    print(f"\033[90m           └─ {decode_0x710(b)}\033[0m")
                continue

            # ── 0x500  Fault DTC ─────────────────────────────────────────────
            if "ID:0x500" in line:
                decoded = decode_0x500(b)
                print(f"\033[91;1m🚨 [FAULT] {line}\033[0m")
                print(f"\033[91;1m       └─ {decoded}\033[0m")
                continue

            # ── 0x400  Status (Lights / Wiper) ───────────────────────────────
            if "ID:0x400" in line:
                key = tuple(b)
                if key != last_400_key or (now - last_400_time) >= 2.0:
                    last_400_key  = key
                    last_400_time = now
                    print(f"\033[92;1m🚗 [STATUS] {line}\033[0m")
                    print(f"\033[92m        └─ {decode_0x400(b)}\033[0m")
                continue

            # ── 0x401  Sensors ───────────────────────────────────────────────
            if "ID:0x401" in line:
                print(f"\033[93m🟡 [SENSOR] {line}\033[0m")
                print(f"\033[93m        └─ {decode_0x401(b)}\033[0m")
                continue

            # ── CAN RX commands ──────────────────────────────────────────────
            if "[CAN RX]" in line:
                decoded = decode_rx_cmd(line, b)
                print(f"\033[96m📩 [CMD] {line}\033[0m")
                if decoded:
                    print(f"\033[96m     └─ {decoded}\033[0m")
                continue

            # ── Anything else ────────────────────────────────────────────────
            print(f"[*] {line}")

        except KeyboardInterrupt:
            print("\n--- Monitoring stopped ---")
            ser.close()
            break
        except Exception as err:
            print(f"Error: {err}")


if __name__ == '__main__':
    main()
