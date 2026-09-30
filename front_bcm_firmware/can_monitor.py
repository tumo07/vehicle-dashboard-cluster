#!/usr/bin/env python3
# ==============================================================================
# Smart Vehicle Dashboard Cluster - Complete CAN Bus Protocol v3.0 Monitor
# ==============================================================================
# Decodes 100% of CAN v3.0 messages:
#  - Group A: 0x100 - 0x105 (Qt -> Central ECU User Commands & ACK)
#  - Group B: 0x200 - 0x211 (Central ECU -> BCMs Approved Execution Commands)
#  - Group C: 0x300 - 0x305 (Central ECU -> Qt Real-Time Status Broadcasts)
#  - Group D: 0x400 - 0x411 (BCMs -> Central ECU Status & Sensor Reports)
#  - Group E: 0x500 - 0x510 (BCMs -> Central ECU Fault / DTC Reports)
#  - Group F: 0x600 - 0x610 (Central ECU Diagnostics, Banners & Watchdog Alerts)
#  - Group G: 0x700 - 0x720 (Heartbeats & Sync) + 0x130 (Blink Tick)
# ==============================================================================

import serial
import serial.tools.list_ports
import time
import sys
import re
import os
import builtins
from datetime import datetime

# Guarantee all prints are immediately flushed on Windows
_builtin_print = builtins.print
def print(*args, **kwargs):
    kwargs.setdefault('flush', True)
    _builtin_print(*args, **kwargs)

try:
    import colorama
    colorama.init()
except ImportError:
    pass

DEFAULT_PORT = 'COM9'
BAUDRATE     = 115200

# Fix console encoding on Windows to prevent Unicode/Emoji crashes
if sys.platform == 'win32':
    try:
        sys.stdout.reconfigure(encoding='utf-8')
    except Exception:
        pass

# --- ANSI Colors ---
C_RESET   = "\033[0m"
C_BOLD    = "\033[1m"
C_RED     = "\033[91m"
C_GREEN   = "\033[92m"
C_YELLOW  = "\033[93m"
C_BLUE    = "\033[94m"
C_MAGENTA = "\033[95m"
C_CYAN    = "\033[96m"
C_WHITE   = "\033[97m"
C_GRAY    = "\033[90m"

# ==============================================================================
# CAN v3.0 Lookup Tables & Enums
# ==============================================================================

DTC_NAMES = {
    0x1000: "B0000 - No body fault",
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

DTC_SEVERITY = {
    0: "INFO",
    1: "WARNING",
    2: "ERROR",
    3: "CRITICAL"
}

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
    0x09: "ERR_CAN_BUS_OFF"
}

LIGHT_CMDS = {
    0: "OFF",
    1: "DRL_ON",
    2: "LOW_BEAM",
    3: "HIGH_BEAM",
    4: "FOG_ON",
    5: "FOG_OFF",
    6: "AUTO_MODE"
}

WIPER_MODES = {
    0: "OFF",
    1: "INTERMITTENT",
    2: "SLOW",
    3: "NORMAL",
    4: "FAST",
    5: "AUTO"
}

TURN_CMDS = {
    0: "TURN_OFF",
    1: "LEFT_TURN",
    2: "RIGHT_TURN",
    3: "HAZARD"
}

TRUNK_CMDS = {
    0: "NO_ACTION",
    1: "OPEN",
    2: "CLOSE",
    3: "STOP"
}

DIAG_CMDS = {
    1: "READ_ALL_DTCS",
    2: "READ_DTC_COUNT",
    3: "CLEAR_ALL_DTCS",
    4: "READ_FREEZE_FRAME",
    5: "READ_LIVE_DATA",
    6: "ECU_RESET",
    7: "NODE_STATUS"
}

ACK_STATUS = {
    0: "APPROVED ✅",
    1: "REJECTED ❌",
    2: "PENDING ⏳"
}

VALIDATION_REASONS = {
    0: "OK",
    1: "BAD_GEAR (Must be P or N)",
    2: "SPEED_TOO_HIGH (>5 km/h)",
    3: "DTC_ACTIVE (Safety Lockout)",
    4: "RESOURCE_BUSY (Trunk In Motion)",
    5: "UNKNOWN_COMMAND",
    6: "SAFETY_LOCK",
    7: "BCM_OFFLINE"
}

TRUNK_MOTOR_STATES = {
    0: "IDLE",
    1: "OPENING",
    2: "CLOSING",
    3: "STALLED",
    0xFF: "UNKNOWN"
}

PARKING_LEVELS = {
    0: "CLEAR (Green)",
    1: "CAUTION (Yellow)",
    2: "WARNING (Orange)",
    3: "CRITICAL 🚨 (Red + Audio)"
}

NODE_NAMES = {
    0: "TRANSLATOR",
    1: "CENTRAL_ECU",
    2: "FRONT_BCM",
    3: "REAR_BCM"
}

GEARS = {
    0: "P (PARK)",
    1: "R (REVERSE)",
    2: "N (NEUTRAL)",
    3: "D (DRIVE)",
    4: "S (SPORT)",
    0xFF: "UNKNOWN"
}

# Commands and Faults that must NEVER be filtered by spam filter
TRANSIENT_EVENT_IDS = {
    "100", "101", "102", "103", "104", "105",
    "200", "201", "202", "210", "211",
    "500", "510", "600", "601", "610"
}

def get_timestamp():
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]

def parse_packet(line):
    """
    Robust parser handling:
      - Standard Gateway: CAN:300:7:01A30007554192
      - SLCAN format:     t300701A30007554192
      - STM32 Debug Log:  [CAN TX] ID:0x300 DLC:7 Data: 01 A3 ...
    """
    line = line.strip()
    if not line:
        return None, []

    # Format 1: CAN:<ID>:<DLC>:<DATA_HEX>
    m1 = re.search(r"CAN:([0-9A-Fa-f]{3,4}):(\d):([0-9A-Fa-f]*)", line)
    if m1:
        can_id = m1.group(1).zfill(3).lower()
        hex_data = m1.group(3)
        b = [int(hex_data[i:i+2], 16) for i in range(0, len(hex_data), 2)]
        return can_id, b

    # Format 2: [CAN TX/RX] ID:0x<ID> DLC:<DLC> Data: <HEX ...>
    m2 = re.search(r"ID:0x([0-9A-Fa-f]+)\s+DLC:(\d)\s+Data:(.*)", line)
    if m2:
        can_id = m2.group(1).zfill(3).lower()
        raw_bytes = m2.group(3).strip().split()
        b = [int(x, 16) for x in raw_bytes if x]
        return can_id, b

    # Format 3: SLCAN t<ID><DLC><DATA>
    m3 = re.match(r"^[tT]([0-9A-Fa-f]{3})(\d)([0-9A-Fa-f]*)", line)
    if m3:
        can_id = m3.group(1).lower()
        hex_data = m3.group(3)
        b = [int(hex_data[i:i+2], 16) for i in range(0, len(hex_data), 2)]
        return can_id, b

    return None, []

# ==============================================================================
# Message Decoders
# ==============================================================================

def decode_0x100(b):
    if not b: return "No payload"
    cmd_val = b[0]
    cmd_name = LIGHT_CMDS.get(cmd_val, f"UNKNOWN(0x{cmd_val:02X})")
    brightness = b[1] if len(b) > 1 else 100
    return f"Cmd: {cmd_name} | Brightness: {brightness}%"

def decode_0x101(b):
    if not b: return "No payload"
    mode = WIPER_MODES.get(b[0], f"UNKNOWN(0x{b[0]:02X})")
    washer = "WASHER SPRAY ACTIVE 💦" if (len(b) > 1 and b[1] == 1) else "Off"
    return f"Mode: {mode} | Washer: {washer}"

def decode_0x102(b):
    if not b: return "No payload"
    return f"Turn Signal: {TURN_CMDS.get(b[0], f'UNKNOWN(0x{b[0]:02X})')}"

def decode_0x103(b):
    if not b: return "No payload"
    return f"Trunk Action: {TRUNK_CMDS.get(b[0], f'UNKNOWN(0x{b[0]:02X})')}"

def decode_0x104(b):
    if not b: return "No payload"
    cmd = DIAG_CMDS.get(b[0], f"CMD_0x{b[0]:02X}")
    params = [f"0x{x:02X}" for x in b[1:]]
    return f"Diag Cmd: {cmd} | Params: [{', '.join(params)}]"

def decode_0x105(b):
    if len(b) < 3: return f"Raw: {b}"
    target = {0: "LIGHT", 1: "WIPER", 2: "TURN", 3: "TRUNK", 4: "DIAG"}.get(b[0], f"0x{b[0]:02X}")
    status = ACK_STATUS.get(b[1], f"0x{b[1]:02X}")
    reason = VALIDATION_REASONS.get(b[2], f"REASON_{b[2]}")
    return f"Target: {target} | Status: {status} | Validation: {reason}"

def decode_0x200(b):
    if not b: return "No payload"
    mask = b[0]
    bri = b[1] if len(b) > 1 else 100
    items = []
    if mask & 0x01: items.append("DRL")
    if mask & 0x02: items.append("LOW_BEAM")
    if mask & 0x04: items.append("HIGH_BEAM")
    if mask & 0x08: items.append("FOG")
    active_str = " | ".join(items) if items else "ALL_OFF"
    return f"Front Lights: [{active_str}] | PWM Brightness: {bri}%"

def decode_0x201(b):
    if not b: return "No payload"
    mode = WIPER_MODES.get(b[0], f"0x{b[0]:02X}")
    spray = "ON 💦" if (len(b) > 1 and b[1]) else "OFF"
    return f"Wiper: {mode} | Spray: {spray}"

def decode_0x202(b):
    if not b: return "No payload"
    mask = b[0]
    arms = []
    if mask & 0x01: arms.append("LEFT_ARM")
    if mask & 0x02: arms.append("RIGHT_ARM")
    if mask & 0x04: arms.append("HAZARD_ARM")
    return f"Front Turn: [{', '.join(arms) or 'OFF'}]"

def decode_0x210(b):
    if not b: return "No payload"
    mask = b[0]
    arms = []
    if mask & 0x01: arms.append("LEFT_ARM")
    if mask & 0x02: arms.append("RIGHT_ARM")
    if mask & 0x04: arms.append("HAZARD_ARM")
    return f"Rear Turn: [{', '.join(arms) or 'OFF'}]"

def decode_0x211(b):
    if not b: return "No payload"
    return f"Rear Trunk Execute: {TRUNK_CMDS.get(b[0], f'0x{b[0]:02X}')}"

def decode_0x300(b):
    if len(b) < 5: return f"Raw: {b}"
    gear = GEARS.get(b[0], f"0x{b[0]:02X}")
    flags = b[1]
    f_list = []
    if flags & 0x01: f_list.append("ENGINE_ON")
    if flags & 0x02: f_list.append("MOVING")
    if flags & 0x04: f_list.append("PARK_BRAKE")
    if flags & 0x08: f_list.append("SEATBELT_OK")
    if flags & 0x10: f_list.append("TRUNK_AJAR ⚠️")
    if flags & 0x20: f_list.append("DTC_ACTIVE 🚨")
    if flags & 0x40: f_list.append("HAZARD")
    if flags & 0x80: f_list.append("REVERSE")

    speed = (b[2] << 8) | b[3]
    fuel  = b[4]
    temp  = (b[5] - 40) if len(b) > 5 else 0
    volt  = (8.0 + (b[6] / 31.875)) if len(b) > 6 else 12.0

    return (f"Gear: {gear} | Speed: {speed} km/h | Fuel: {fuel}% | "
            f"Coolant: {temp}°C | Battery: {volt:.1f}V | Flags: [{', '.join(f_list) or 'NONE'}]")

def decode_0x301(b):
    if not b: return "No payload"
    flags = b[0]
    wiper = b[1] if len(b) > 1 else 0
    items = []
    if flags & 0x01: items.append("DRL")
    if flags & 0x02: items.append("LOW_BEAM")
    if flags & 0x04: items.append("HIGH_BEAM")
    if flags & 0x08: items.append("FOG")
    if flags & 0x10: items.append("TURN_LEFT")
    if flags & 0x20: items.append("TURN_RIGHT")
    if flags & 0x40: items.append("HAZARD")
    if flags & 0x80: items.append("BRAKE")
    return f"Active Lights: [{', '.join(items) or 'ALL_OFF'}] | Wiper Mode: {WIPER_MODES.get(wiper, f'0x{wiper:02X}')}"

def decode_0x302(b):
    if not b: return "No payload"
    l = "ON 💡" if (b[0] & 0x01) else "OFF"
    r = "ON 💡" if (b[0] & 0x02) else "OFF"
    return f"Turn Blink Phase: Left={l} | Right={r}"

def decode_0x303(b):
    if len(b) < 2: return f"Raw: {b}"
    pct = b[0]
    state = TRUNK_MOTOR_STATES.get(b[1], f"0x{b[1]:02X}")
    return f"Trunk State: {state} ({pct}% open)"

def decode_0x304(b):
    if len(b) < 3: return f"Raw: {b}"
    dist = (b[0] << 8) | b[1]
    dist_str = f"{dist} cm" if dist != 0xFFFF else "NO_OBJECT (>400cm)"
    level = PARKING_LEVELS.get(b[2], f"0x{b[2]:02X}")
    return f"Radar: {dist_str} | Warning Level: {level}"

def decode_0x400(b):
    if len(b) < 3: return f"Raw: {b}"
    flags, wiper, health = b[0], b[1], b[2]
    parts = []
    if flags & 0x01: parts.append("DRL")
    if flags & 0x02: parts.append("LOW_BEAM")
    if flags & 0x04: parts.append("FOG")
    if flags & 0x08: parts.append("WIPER_ON")
    if flags & 0x10: parts.append("WASHER")
    if flags & 0x20: parts.append("LEFT_TURN")
    if flags & 0x40: parts.append("RIGHT_TURN")
    if flags & 0x80: parts.append("HIGH_BEAM")
    return f"Actuators: [{', '.join(parts) or 'OFF'}] | Wiper: {WIPER_MODES.get(wiper, f'0x{wiper:02X}')} | Motor Health: {health}%"

def decode_0x401(b):
    if len(b) < 2: return f"Raw: {b}"
    rain = b[0]
    water = b[1]
    rain_str = "🌧️ RAINING" if rain > 20 else "☀️ DRY"
    water_str = "⚠️ CRITICALLY LOW" if water < 20 else "✅ OK"
    return f"Rain Sensor: {rain}% ({rain_str}) | Washer Tank Level: {water}% ({water_str})"

def decode_0x410(b):
    if len(b) < 3: return f"Raw: {b}"
    flags = b[0]
    parts = []
    if flags & 0x01: parts.append("BRAKE")
    if flags & 0x02: parts.append("LEFT_TURN")
    if flags & 0x04: parts.append("RIGHT_TURN")
    if flags & 0x08: parts.append("TRUNK_MOTOR_ACTIVE")
    state = TRUNK_MOTOR_STATES.get(b[1], f"0x{b[1]:02X}")
    return f"Actuators: [{', '.join(parts) or 'NONE'}] | Trunk Motor: {state} | Position: {b[2]}%"

def decode_0x411(b):
    if len(b) < 4: return f"Raw: {b}"
    dist = (b[0] << 8) | b[1]
    dist_str = f"{dist} cm" if dist != 0xFFFF else "NO_OBJECT"
    hall = "OPEN 🔓" if b[2] == 1 else "LATCHED/CLOSED 🔒"
    level = PARKING_LEVELS.get(b[3], f"0x{b[3]:02X}")
    return f"Ultrasonic: {dist_str} | Hall Latch: {hall} | Park Level: {level}"

def decode_fault(b):
    if len(b) < 5: return f"Raw: {b}"
    sev = DTC_SEVERITY.get(b[0], f"0x{b[0]:02X}")
    dtc = (b[1] << 8) | b[2]
    dtc_name = DTC_NAMES.get(dtc, f"UNKNOWN_DTC_0x{dtc:04X}")
    cnt = b[3]
    err = ERR_CODES.get(b[4], f"0x{b[4]:02X}")
    return f"[{sev}] {dtc_name} | Type: {err} | Count: {cnt}"

def decode_banner_fault(b):
    if len(b) < 6: return f"Raw: {b}"
    node = NODE_NAMES.get(b[0], f"NODE_{b[0]}")
    sev = DTC_SEVERITY.get(b[1], f"0x{b[1]:02X}")
    dtc = (b[2] << 8) | b[3]
    dtc_name = DTC_NAMES.get(dtc, f"0x{dtc:04X}")
    cnt = b[4]
    err = ERR_CODES.get(b[5], f"0x{b[5]:02X}")
    return f"Source: {node} | Severity: {sev} | Fault: {dtc_name} | Err: {err} | Count: {cnt}"

def decode_watchdog(b):
    if len(b) < 2: return f"Raw: {b}"
    node = NODE_NAMES.get(b[0], f"NODE_{b[0]}")
    duration = b[1]
    if duration == 0:
        return f"{node} is RECOVERED and back ONLINE! ✅"
    return f"{node} is OFFLINE! (No heartbeat for {duration}s) ⚠️"

def decode_heartbeat(b, node_name):
    if len(b) < 2: return f"Raw: {b}"
    uptime = b[0]
    flags  = b[1]
    bits = []
    if flags & 0x01: bits.append("INIT_OK")
    if flags & 0x02: bits.append("CAN_OK")
    if flags & 0x04: bits.append("SENSORS_OK")
    if flags & 0x08: bits.append("DTC_ACTIVE ⚠️")
    return f"[{node_name}] Uptime: {uptime}s | Status: [{' | '.join(bits) or 'NONE'}]"

def resolve_port(target_port):
    ports = list(serial.tools.list_ports.comports())
    if target_port:
        for p in ports:
            if p.device.upper() == target_port.upper():
                return p.device
    
    # Filter out virtual bluetooth ports
    usb_ports = [p for p in ports if not (p.hwid and "BTHENUM" in p.hwid)]
    
    if target_port:
        print(f"{C_YELLOW}⚠️  Port '{target_port}' is NOT currently connected!{C_RESET}")
    
    if usb_ports:
        print(f"{C_CYAN}🔍 Available USB Serial Ports:{C_RESET}")
        for p in usb_ports:
            print(f"   • {C_BOLD}{p.device}{C_RESET}: {p.description}")
        print(f"{C_GREEN}➡️  Auto-connecting to: {usb_ports[0].device}{C_RESET}\n")
        return usb_ports[0].device

    print(f"{C_RED}❌ No physical USB serial port detected!{C_RESET}")
    print(f"{C_YELLOW}💡 Please plug in your USB-to-UART / J-Link adapter connected to the Gateway.{C_RESET}")
    return target_port or DEFAULT_PORT

# ==============================================================================
# Main Monitor Loop
# ==============================================================================

def main():
    args = sys.argv[1:]
    verbose_mode = False
    if "-v" in args or "--verbose" in args or "--raw" in args:
        verbose_mode = True
        args = [a for a in args if a not in ("-v", "--verbose", "--raw")]

    target_port = args[0] if args else DEFAULT_PORT

    print(f"\n{C_CYAN}{C_BOLD}========================================================================{C_RESET}")
    print(f"{C_CYAN}{C_BOLD}    SMART VEHICLE DASHBOARD CLUSTER - COMPLETE CAN v3.0 MONITOR         {C_RESET}")
    print(f"{C_CYAN}{C_BOLD}========================================================================{C_RESET}")

    port = resolve_port(target_port)
    print(f"Connecting to port: {C_YELLOW}{port}{C_RESET} @ {BAUDRATE} baud...")

    try:
        ser = serial.Serial(port, BAUDRATE, timeout=1)
        print(f"{C_GREEN}✅ Successfully connected to {port}!{C_RESET}")
        mode_str = "VERBOSE (All packets)" if verbose_mode else "SMART FILTER (Periodic 2s heartbeats + instant events)"
        print(f"{C_WHITE}Monitoring all CAN traffic [{mode_str}]...{C_RESET}\n")
    except Exception as e:
        print(f"{C_RED}❌ Cannot open serial port {port}.{C_RESET}")
        print(f"Details: {e}")
        print(f"Usage: python can_monitor.py <COM_PORT> [-v] (e.g. COM9, COM4, COM3 -v)")
        sys.exit(1)

    msg_cache = {}
    hb_cache = {}
    offline_nodes = set()
    last_rx_time = time.time()
    last_idle_notice = time.time()

    import threading

    def send_can(cid, dlc, hex_data, label):
        tx_pkt = f"TX:{cid}:{dlc}:{hex_data}\r\n".encode('utf-8')
        try:
            ser.write(tx_pkt)
            print(f"{C_YELLOW}🚀 >>> [TX SENT] {label} (ID:0x{cid} DLC:{dlc} Data:{hex_data}){C_RESET}")
        except Exception as ex:
            print(f"{C_RED}❌ [TX ERROR] Failed to send: {ex}{C_RESET}")

    def tx_input_loop():
        nonlocal verbose_mode
        print(f"{C_YELLOW}⌨️  INTERACTIVE CAN INJECTOR ACTIVE:{C_RESET}")
        print(f"{C_GRAY}   Type [1]=Low Beam, [2]=High Beam, [0]=Lights Off, [drl]=DRL, [fog]=Fog, [fogoff]=Fog Off{C_RESET}")
        print(f"{C_GRAY}   Type [h]=Hazard, [l]=Left, [r]=Right, [toff]=Turn Off, [w]=Wiper, [woff]=Wiper Off, [t]=Trunk Open, [tclose]=Trunk Close{C_RESET}")
        print(f"{C_GRAY}   Type [v]=Toggle Verbose/Filtered mode, [clear]=Clear DTCs{C_RESET}\n")
        while True:
            try:
                cmd = sys.stdin.readline()
                if not cmd: break
                cmd = cmd.strip().lower()
                if not cmd: continue

                if cmd in ('v', 'verbose', 'raw'):
                    verbose_mode = not verbose_mode
                    print(f"{C_CYAN}🔔 Verbose mode: {'ENABLED (Displaying 100% of raw CAN traffic)' if verbose_mode else 'DISABLED (Filtered mode with periodic heartbeats)'}{C_RESET}")
                elif cmd in ('1', 'low'):
                    send_can("100", "2", "0264", "CMD_LIGHT: LOW BEAM (100%)")
                elif cmd in ('2', 'high'):
                    send_can("100", "2", "0364", "CMD_LIGHT: HIGH BEAM (100%)")
                elif cmd in ('0', 'off'):
                    send_can("100", "2", "0000", "CMD_LIGHT: ALL LIGHTS OFF")
                elif cmd == 'drl':
                    send_can("100", "2", "0164", "CMD_LIGHT: DRL ON")
                elif cmd == 'fog':
                    send_can("100", "2", "0464", "CMD_LIGHT: FOG ON")
                elif cmd == 'fogoff':
                    send_can("100", "2", "0500", "CMD_LIGHT: FOG OFF")
                elif cmd in ('h', 'hazard'):
                    send_can("102", "1", "03", "CMD_TURN: HAZARD")
                elif cmd in ('l', 'left'):
                    send_can("102", "1", "01", "CMD_TURN: LEFT")
                elif cmd in ('r', 'right'):
                    send_can("102", "1", "02", "CMD_TURN: RIGHT")
                elif cmd in ('toff', 'stopturn'):
                    send_can("102", "1", "00", "CMD_TURN: OFF")
                elif cmd in ('w', 'wiper'):
                    send_can("101", "2", "0300", "CMD_WIPER: NORMAL MODE")
                elif cmd in ('woff',):
                    send_can("101", "2", "0000", "CMD_WIPER: OFF")
                elif cmd in ('t', 'trunk'):
                    send_can("103", "1", "01", "CMD_TRUNK: OPEN")
                elif cmd in ('tclose',):
                    send_can("103", "1", "02", "CMD_TRUNK: CLOSE")
                elif cmd in ('clear', 'cleardtc'):
                    send_can("601", "2", "0000", "CMD_BANNER_CLEAR: ALL DTCS")
                else:
                    if cmd.upper().startswith("CAN:") or cmd.upper().startswith("TX:"):
                        parts = cmd.strip().split(':')
                        if len(parts) >= 4:
                            send_can(parts[1], parts[2], parts[3], f"RAW INJECTION: 0x{parts[1]}")
                        else:
                            ser.write((cmd.upper() + "\r\n").encode('utf-8'))
                    else:
                        print(f"{C_GRAY}Unknown command '{cmd}'. Available: 1, 2, 0, drl, fog, fogoff, h, l, r, toff, w, woff, t, tclose, v, clear{C_RESET}")
            except Exception:
                break

    threading.Thread(target=tx_input_loop, daemon=True).start()

    while True:
        try:
            raw_line = ser.readline().decode('utf-8', errors='ignore').strip()
            now = time.time()
            if not raw_line:
                if (now - last_rx_time) > 4.0 and (now - last_idle_notice) > 6.0:
                    last_idle_notice = now
                    curr_ports = [p.device.upper() for p in serial.tools.list_ports.comports()]
                    if port.upper() not in curr_ports:
                        print(f"{C_RED}⚠️ [SERIAL] Port {port} was disconnected from computer! Waiting for re-plug...{C_RESET}")
                    else:
                        print(f"{C_GRAY}[{get_timestamp()}] ⏳ Listening on {port}... (Tip: type '1', '2', 'h', 'l', 'r' to inject test commands, or 'v' for raw mode){C_RESET}")
                continue

            last_rx_time = now
            can_id, b = parse_packet(raw_line)
            ts = get_timestamp()

            if not can_id:
                # Print non-packet debug line (greyed out)
                if not raw_line.startswith("?"):
                    print(f"{C_GRAY}[{ts}] {raw_line}{C_RESET}")
                continue

            # ==================================================================
            # SPAM FILTER POLICY:
            # 1. TRANSIENT EVENTS (Commands, ACKs, Faults, Banners, Watchdog):
            #    --> NEVER FILTERED! Always prints immediately.
            # 2. HEARTBEATS (0x700, 0x710, 0x720):
            #    --> Prints immediately on flag change OR every 2.0s to show clock.
            # 3. PERIODIC BROADCASTS (0x300, 0x301, 0x303, 0x400, etc.):
            #    --> Prints immediately on payload change OR every 4.0s to show state.
            # 4. VERBOSE MODE:
            #    --> Never filters anything!
            # ==================================================================
            if not verbose_mode:
                if can_id in ("700", "710", "720"):
                    flags = b[1] if len(b) > 1 else 0
                    last_t, last_fl = hb_cache.get(can_id, (0, -1))
                    if (flags == last_fl) and (now - last_t < 2.0):
                        continue
                    hb_cache[can_id] = (now, flags)
                elif can_id == "130":
                    continue  # Sync tick (500ms) only shown in verbose mode
                elif can_id not in TRANSIENT_EVENT_IDS:
                    key = tuple(b)
                    last_t, last_k = msg_cache.get(can_id, (0, None))
                    if (key == last_k) and (now - last_t < 4.0):
                        continue
                    msg_cache[can_id] = (now, key)

            # ==================================================================
            # GROUP A: Qt -> Central ECU User Commands
            # ==================================================================
            if can_id == "100":
                print(f"{C_YELLOW}{C_BOLD}[{ts}] 💡 [CMD_LIGHT]     ID:0x100 Data:{b}{C_RESET}")
                print(f"{C_YELLOW}           ↳ {decode_0x100(b)}{C_RESET}")
                continue

            if can_id == "101":
                print(f"{C_YELLOW}{C_BOLD}[{ts}] 🌧️ [CMD_WIPER]     ID:0x101 Data:{b}{C_RESET}")
                print(f"{C_YELLOW}           ↳ {decode_0x101(b)}{C_RESET}")
                continue

            if can_id == "102":
                print(f"{C_YELLOW}{C_BOLD}[{ts}] ⬅️ [CMD_TURN]      ID:0x102 Data:{b}{C_RESET}")
                print(f"{C_YELLOW}           ↳ {decode_0x102(b)}{C_RESET}")
                continue

            if can_id == "103":
                print(f"{C_YELLOW}{C_BOLD}[{ts}] 🧳 [CMD_TRUNK]     ID:0x103 Data:{b}{C_RESET}")
                print(f"{C_YELLOW}           ↳ {decode_0x103(b)}{C_RESET}")
                continue

            if can_id == "104":
                print(f"{C_CYAN}{C_BOLD}[{ts}] 🛠️ [CMD_DIAG]      ID:0x104 Data:{b}{C_RESET}")
                print(f"{C_CYAN}           ↳ {decode_0x104(b)}{C_RESET}")
                continue

            if can_id == "105":
                print(f"{C_GREEN}{C_BOLD}[{ts}] 🛡️ [CENTRAL_ACK]   ID:0x105 Data:{b}{C_RESET}")
                print(f"{C_GREEN}           ↳ {decode_0x105(b)}{C_RESET}")
                continue

            # ==================================================================
            # GROUP B: Central ECU -> BCMs Approved Execution Commands
            # ==================================================================
            if can_id == "200":
                print(f"{C_MAGENTA}{C_BOLD}[{ts}] ⚡ [EXEC_F_LIGHT]  ID:0x200 Data:{b}{C_RESET}")
                print(f"{C_MAGENTA}           ↳ {decode_0x200(b)}{C_RESET}")
                continue

            if can_id == "201":
                print(f"{C_MAGENTA}{C_BOLD}[{ts}] ⚡ [EXEC_F_WIPER]  ID:0x201 Data:{b}{C_RESET}")
                print(f"{C_MAGENTA}           ↳ {decode_0x201(b)}{C_RESET}")
                continue

            if can_id == "202":
                print(f"{C_MAGENTA}{C_BOLD}[{ts}] ⚡ [EXEC_F_TURN]   ID:0x202 Data:{b}{C_RESET}")
                print(f"{C_MAGENTA}           ↳ {decode_0x202(b)}{C_RESET}")
                continue

            if can_id == "210":
                print(f"{C_MAGENTA}{C_BOLD}[{ts}] ⚡ [EXEC_R_TURN]   ID:0x210 Data:{b}{C_RESET}")
                print(f"{C_MAGENTA}           ↳ {decode_0x210(b)}{C_RESET}")
                continue

            if can_id == "211":
                print(f"{C_MAGENTA}{C_BOLD}[{ts}] ⚡ [EXEC_R_TRUNK]  ID:0x211 Data:{b}{C_RESET}")
                print(f"{C_MAGENTA}           ↳ {decode_0x211(b)}{C_RESET}")
                continue

            # ==================================================================
            # GROUP C: Central ECU -> Qt Real-Time Status Broadcasts
            # ==================================================================
            if can_id == "300":
                print(f"{C_BLUE}{C_BOLD}[{ts}] 🏎️ [VEHICLE_STATE] ID:0x300 Data:{b}{C_RESET}")
                print(f"{C_BLUE}           ↳ {decode_0x300(b)}{C_RESET}")
                continue

            if can_id == "301":
                print(f"{C_CYAN}{C_BOLD}[{ts}] 💡 [LIGHTS_STATE]  ID:0x301 Data:{b}{C_RESET}")
                print(f"{C_CYAN}           ↳ {decode_0x301(b)}{C_RESET}")
                continue

            if can_id == "302":
                print(f"{C_CYAN}[{ts}] 🚥 [TURN_BLINK]    ID:0x302 Data:{b} ↳ {decode_0x302(b)}{C_RESET}")
                continue

            if can_id == "303":
                print(f"{C_BLUE}[{ts}] 🧳 [TRUNK_STATE]   ID:0x303 Data:{b} ↳ {decode_0x303(b)}{C_RESET}")
                continue

            if can_id == "304":
                print(f"{C_BLUE}[{ts}] 📡 [REVERSE_RADAR] ID:0x304 Data:{b} ↳ {decode_0x304(b)}{C_RESET}")
                continue

            # ==================================================================
            # GROUP D: BCMs -> Central ECU Reports
            # ==================================================================
            if can_id == "400":
                print(f"{C_GREEN}[{ts}] 🚘 [FRONT_STATUS]  ID:0x400 Data:{b}{C_RESET}")
                print(f"{C_GREEN}           ↳ {decode_0x400(b)}{C_RESET}")
                continue

            if can_id == "401":
                print(f"{C_GREEN}[{ts}] 🌧️ [FRONT_SENSORS] ID:0x401 Data:{b}{C_RESET}")
                print(f"{C_GREEN}           ↳ {decode_0x401(b)}{C_RESET}")
                continue

            if can_id == "410":
                print(f"{C_GREEN}[{ts}] 🚗 [REAR_STATUS]   ID:0x410 Data:{b}{C_RESET}")
                print(f"{C_GREEN}           ↳ {decode_0x410(b)}{C_RESET}")
                continue

            if can_id == "411":
                print(f"{C_GREEN}[{ts}] 📡 [REAR_SENSORS]  ID:0x411 Data:{b}{C_RESET}")
                print(f"{C_GREEN}           ↳ {decode_0x411(b)}{C_RESET}")
                continue

            # ==================================================================
            # GROUP E & F: Faults, Banners & Watchdog
            # ==================================================================
            if can_id in ("500", "510"):
                origin = "FRONT_BCM" if can_id == "500" else "REAR_BCM"
                print(f"{C_RED}{C_BOLD}[{ts}] 🚨 [FAULT_{origin}] ID:0x{can_id} Data:{b}{C_RESET}")
                print(f"{C_RED}{C_BOLD}           ↳ {decode_fault(b)}{C_RESET}")
                continue

            if can_id == "600":
                print(f"{C_RED}{C_BOLD}[{ts}] 📢 [BANNER_FAULT]  ID:0x600 Data:{b}{C_RESET}")
                print(f"{C_RED}{C_BOLD}           ↳ {decode_banner_fault(b)}{C_RESET}")
                continue

            if can_id == "601":
                dtc = (b[0] << 8) | b[1] if len(b) >= 2 else 0
                print(f"{C_GREEN}{C_BOLD}[{ts}] ✅ [BANNER_CLEAR]  ID:0x601 Resolved DTC: 0x{dtc:04X}{C_RESET}")
                continue

            if can_id == "610":
                is_recovery = (len(b) >= 2 and b[1] == 0)
                node_name = NODE_NAMES.get(b[0], f"NODE_{b[0]}")
                if is_recovery:
                    offline_nodes.discard(node_name)
                    print(f"{C_GREEN}{C_BOLD}[{ts}] ✅ [WATCHDOG]      ID:0x610 Data:{b}{C_RESET}")
                    print(f"{C_GREEN}{C_BOLD}           ↳ {node_name} is RECOVERED and back ONLINE!{C_RESET}")
                else:
                    offline_nodes.add(node_name)
                    print(f"{C_RED}{C_BOLD}[{ts}] ⚠️ [WATCHDOG]      ID:0x610 Data:{b}{C_RESET}")
                    print(f"{C_RED}{C_BOLD}           ↳ {decode_watchdog(b)}{C_RESET}")
                continue

            # ==================================================================
            # GROUP G: Heartbeats & Clock Sync
            # ==================================================================
            if can_id in ("700", "710", "720"):
                node = {"700": "CENTRAL_ECU", "710": "FRONT_BCM", "720": "REAR_BCM"}.get(can_id, "NODE")
                if node in offline_nodes:
                    offline_nodes.discard(node)
                    print(f"{C_GREEN}{C_BOLD}[{ts}] ✅ [NODE_ONLINE]   ID:0x{can_id} {node} is RECOVERED and back ONLINE!{C_RESET}")
                print(f"{C_GRAY}[{ts}] 🩺 [HEARTBEAT]     ID:0x{can_id} Data:{b}{C_RESET}")
                print(f"{C_GRAY}           ↳ {decode_heartbeat(b, node)}{C_RESET}")
                continue

            if can_id == "130":
                print(f"{C_GRAY}[{ts}] ⏱️ [SYNC_TICK]     ID:0x130 (Blink 500ms){C_RESET}")
                continue

            # Generic unmapped CAN message
            print(f"[{ts}] 📦 [UNKNOWN_CAN]   ID:0x{can_id} Data:{b}")

        except KeyboardInterrupt:
            print(f"\n{C_YELLOW}--- Monitoring stopped by user ---{C_RESET}")
            try:
                ser.close()
            except Exception:
                pass
            break
        except Exception as e:
            # Prevent loop crash on transient serial hiccup
            continue

if __name__ == '__main__':
    main()
