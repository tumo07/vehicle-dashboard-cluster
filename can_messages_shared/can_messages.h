#ifndef CAN_MESSAGES_H
#define CAN_MESSAGES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==============================================================================
// ARCHITECTURE OVERVIEW
// ==============================================================================
// Central ECU (Main ECU) is the COORDINATOR and DECISION MAKER
// - Receives ALL commands/requests from Qt (via CAN Translator)
// - Validates state and context (e.g., is vehicle in PARK before trunk open?)
// - Approves/Rejects commands and sends to respective BCMs
// - Monitors all sensor data and error states
// - Communicates real-time status/errors back to Qt for UI updates
// 
// BCMs (Front & Rear) are CONTROL EXECUTORS
// - Receive APPROVED commands from Central ECU only
// - Execute actuator control (relays, motors, LEDs)
// - Report status, sensor data, and errors back to Central ECU
// - Central ECU acts as intermediary between Qt and BCMs
//
// Qt Application (via CAN Translator)
// - Sends user commands/requests to Central ECU
// - Receives real-time sensor updates from Central ECU
// - Displays diagnostic errors in banner style (OBD-like)
// - Shows vehicle state, faults, and audit logs


// ==============================================================================
// 1. CAN MESSAGE IDs - ORGANIZED BY DIRECTION & PURPOSE
// ==============================================================================

// ========== GROUP A: Qt -> Central ECU (User Commands/Requests) ==========
// DLC: Variable (1-8 bytes per message)
// Priority: HIGH (processed immediately, validated for conflicts)

#define CAN_ID_CMD_LIGHT_CONTROL    0x100  // Headlight, DRL, fog light commands
#define CAN_ID_CMD_WIPER_CONTROL    0x101  // Wiper speed, washer spray commands
#define CAN_ID_CMD_TURN_SIGNAL      0x102  // Left/Right turn signal requests
#define CAN_ID_CMD_TRUNK_CONTROL    0x103  // Trunk open/close requests (state-dependent)
#define CAN_ID_CMD_DIAGNOSTIC       0x104  // Diagnostic mode enable, clear faults, etc.


// ========== GROUP B: Central ECU -> BCMs (Approved Commands) ==========
// Only APPROVED commands reach BCMs
// DLC: 1-2 bytes per message

#define CAN_ID_EXEC_FRONT_LIGHTS    0x200  // Central ECU -> Front BCM: Execute light commands
#define CAN_ID_EXEC_FRONT_WIPERS    0x201  // Central ECU -> Front BCM: Execute wiper commands
#define CAN_ID_EXEC_REAR_LIGHTS     0x210  // Central ECU -> Rear BCM: Execute turn signals
#define CAN_ID_EXEC_REAR_TRUNK      0x211  // Central ECU -> Rear BCM: Execute trunk control


// ========== GROUP C: Central ECU -> Qt (Real-Time Status/Sensor Updates) ==========
// Continuous or event-triggered updates
// DLC: 1-8 bytes per message
// Used for HMI updates, gauges, status indicators

#define CAN_ID_STATUS_VEHICLE_STATE 0x300  // Speed, RPM, Gear, Temp, Battery voltage
#define CAN_ID_STATUS_LIGHTS_STATE  0x301  // Current light states (HLs, DRL, fog, etc.)
#define CAN_ID_STATUS_WIPERS_STATE  0x302  // Current wiper speed, washer level
#define CAN_ID_STATUS_TRUNK_STATE   0x303  // Trunk position (0-100%), motor state
#define CAN_ID_STATUS_DOORS_LOCKS   0x304  // Door/window/lock states (for future expansion)
#define CAN_ID_STATUS_CLIMATE       0x305  // A/C, heating, seat warmers (future)


// ========== GROUP D: BCMs -> Central ECU (Status Reports & Feedback) ==========
// Status from actuators, sensor readings, operational state
// DLC: 1-8 bytes per message

#define CAN_ID_REPORT_FRONT_STATUS  0x400  // Front BCM status: Light actuators, wiper state
#define CAN_ID_REPORT_FRONT_SENSORS 0x401  // Front BCM sensors: Rain detect, light intensity
#define CAN_ID_REPORT_REAR_STATUS   0x410  // Rear BCM status: Light actuators, trunk motor
#define CAN_ID_REPORT_REAR_SENSORS  0x411  // Rear BCM sensors: Distance (HC-SR04), trunk hall


// ========== GROUP E: Errors & Diagnostics ==========
// Real-time error reporting (OBD-style DTC codes)
// Sent immediately when faults occur, or periodically for monitoring
// DLC: 2-8 bytes per message

#define CAN_ID_ERROR_CENTRAL_ECU    0x500  // Central ECU diagnostics (power, comms, logic)
#define CAN_ID_ERROR_FRONT_BCM      0x510  // Front BCM faults (relay, LED, wiper motor)
#define CAN_ID_ERROR_REAR_BCM       0x520  // Rear BCM faults (relay, motor, sensor)
#define CAN_ID_DIAG_REQUEST         0x530  // Qt -> Central ECU: Request diagnostic data
#define CAN_ID_DIAG_RESPONSE        0x531  // Central ECU -> Qt: Diagnostic data (DTC list, counters)


// ========== GROUP F: Heartbeat & Sync ==========
// Ensure all nodes are alive and synchronized
// DLC: 1-2 bytes per message
// Interval: 100ms or 500ms

#define CAN_ID_HEARTBEAT_CENTRAL    0x600  // Central ECU alive pulse
#define CAN_ID_HEARTBEAT_FRONT_BCM  0x610  // Front BCM alive pulse
#define CAN_ID_HEARTBEAT_REAR_BCM   0x620  // Rear BCM alive pulse


// ==============================================================================
// 2. COMMAND PAYLOADS & BITMASKS
// ==============================================================================

// ========== CAN_ID_CMD_LIGHT_CONTROL (Qt -> Central ECU) ==========
// Byte 0: Light Control Bitmask
#define CMD_LIGHT_HEADLIGHT_ON      (1 << 0)  // 0x01 - Main headlights
#define CMD_LIGHT_HEADLIGHT_OFF     (1 << 1)  // 0x02 - Turn off headlights
#define CMD_LIGHT_DRL_ON            (1 << 2)  // 0x04 - Daytime Running Lights
#define CMD_LIGHT_DRL_OFF           (1 << 3)  // 0x08
#define CMD_LIGHT_FOG_ON            (1 << 4)  // 0x10 - Fog lights
#define CMD_LIGHT_FOG_OFF           (1 << 5)  // 0x20
#define CMD_LIGHT_AUTO_MODE         (1 << 6)  // 0x40 - Auto mode (sensor-driven)
// Byte 1: Brightness (0-100%) if applicable

// ========== CAN_ID_CMD_WIPER_CONTROL (Qt -> Central ECU) ==========
// Byte 0: Wiper Mode
#define CMD_WIPER_OFF               0x00
#define CMD_WIPER_INTERMITTENT      0x01
#define CMD_WIPER_SLOW              0x02
#define CMD_WIPER_NORMAL            0x03
#define CMD_WIPER_FAST              0x04
#define CMD_WIPER_AUTO              0x05  // Rain sensor driven
// Byte 1: Washer trigger (0x01 = spray)

// ========== CAN_ID_CMD_TURN_SIGNAL (Qt -> Central ECU) ==========
// Byte 0: Turn Signal Command
#define CMD_TURN_OFF                0x00
#define CMD_TURN_LEFT               0x01
#define CMD_TURN_RIGHT              0x02
#define CMD_TURN_HAZARD             0x03

// ========== CAN_ID_CMD_TRUNK_CONTROL (Qt -> Central ECU) ==========
// Byte 0: Trunk Command
#define CMD_TRUNK_NO_ACTION         0x00
#define CMD_TRUNK_OPEN              0x01
#define CMD_TRUNK_CLOSE             0x02
#define CMD_TRUNK_STOP              0x03  // Emergency stop
// Byte 1: Reserved for future (unlock code, security check, etc.)


// ==============================================================================
// 3. VEHICLE STATE ENUMS & CONTEXT FLAGS
// ==============================================================================

// Vehicle Gear/Mode (from sensors, used for validation logic)
typedef enum {
    GEAR_PARK       = 0x00,
    GEAR_REVERSE    = 0x01,
    GEAR_NEUTRAL    = 0x02,
    GEAR_DRIVE      = 0x03,
    GEAR_SPORT      = 0x04,
    GEAR_UNKNOWN    = 0xFF
} VehicleGear_t;

// Vehicle State Flags (CAN_ID_STATUS_VEHICLE_STATE byte 1)
#define STATE_ENGINE_RUNNING        (1 << 0)  // 0x01
#define STATE_VEHICLE_MOVING        (1 << 1)  // 0x02 - Speed > threshold
#define STATE_DOORS_LOCKED          (1 << 2)  // 0x04
#define STATE_SEATBELTS_FASTENED    (1 << 3)  // 0x08
#define STATE_HAZARD_LIGHTS_ON      (1 << 4)  // 0x10
#define STATE_PARK_BRAKE_ON         (1 << 5)  // 0x20
#define STATE_TRUNK_AJAR            (1 << 6)  // 0x40


// ==============================================================================
// 4. REAL-TIME STATUS PAYLOADS
// ==============================================================================

// ========== CAN_ID_STATUS_VEHICLE_STATE (Central ECU -> Qt) ==========
// Byte 0: Vehicle Gear (VehicleGear_t)
// Byte 1: Vehicle State Flags
// Byte 2-3: Speed (uint16, km/h)
// Byte 4: RPM (uint8, 0-100% of max)
// Byte 5: Coolant Temp (int8, -40 to +125°C, offset by 40)
// Byte 6: Battery Voltage (uint8, 8-16V encoded as 0-255)

// ========== CAN_ID_STATUS_LIGHTS_STATE (Central ECU -> Qt) ==========
// Byte 0: Light Status Bitmask
#define STATUS_HL_ACTIVE            (1 << 0)  // Headlights ON
#define STATUS_DRL_ACTIVE           (1 << 1)  // DRL ON
#define STATUS_FOG_ACTIVE           (1 << 2)  // Fog lights ON
#define STATUS_TURN_LEFT_ACTIVE     (1 << 3)  // Left turn blinking
#define STATUS_TURN_RIGHT_ACTIVE    (1 << 4)  // Right turn blinking
#define STATUS_HAZARD_ACTIVE        (1 << 5)  // Hazard blinking
#define STATUS_BRAKE_LIGHTS_ACTIVE  (1 << 6)  // Brake lights ON
// Byte 1: Reserved

// ========== CAN_ID_STATUS_WIPERS_STATE (Central ECU -> Qt) ==========
// Byte 0: Current Wiper Mode (CMD_WIPER_* values)
// Byte 1: Washer Fluid Level (0-100%)
// Byte 2: Wiper Motor Health (0-100%, 0=error, 100=healthy)

// ========== CAN_ID_STATUS_TRUNK_STATE (Central ECU -> Qt) ==========
// Byte 0: Trunk Position (0-100%, 0=closed, 100=fully open)
// Byte 1: Trunk Motor State
#define TRUNK_STATE_IDLE            0x00
#define TRUNK_STATE_OPENING         0x01
#define TRUNK_STATE_CLOSING         0x02
#define TRUNK_STATE_STALLED         0x03  // Motor error/stall
#define TRUNK_STATE_UNKNOWN         0xFF


// ==============================================================================
// 5. ERROR CODES & DTC (Diagnostic Trouble Codes) - OBD-STYLE
// ==============================================================================

// Power Train Codes (P-series: 0x0000-0x0FFF)
typedef enum {
    DTC_P0000 = 0x0000,  // No errors
    DTC_P0101 = 0x0101,  // Mass Air Flow (MAF) sensor malfunction
    DTC_P0201 = 0x0201,  // Fuel Injector Circuit (Cylinder 1)
    DTC_P0301 = 0x0301,  // Cylinder 1 Misfire
    // ... extend as needed
} PowerTrainDTC_t;

// Body/Chassis Codes (B-series: 0x1000-0x1FFF)
typedef enum {
    DTC_B1000 = 0x1000,  // No body errors
    DTC_B1001 = 0x1001,  // Headlight circuit fault (left)
    DTC_B1002 = 0x1002,  // Headlight circuit fault (right)
    DTC_B1010 = 0x1010,  // Wiper motor stall/jam
    DTC_B1011 = 0x1011,  // Wiper position sensor fault
    DTC_B1020 = 0x1020,  // Trunk motor stall
    DTC_B1021 = 0x1021,  // Trunk position sensor fault (ultrasonic)
    DTC_B1030 = 0x1030,  // Turn signal relay fault (left)
    DTC_B1031 = 0x1031,  // Turn signal relay fault (right)
    DTC_B1040 = 0x1040,  // Rear fog light circuit fault
    DTC_B1050 = 0x1050,  // Door lock actuator fault
} BodyDTC_t;

// Network/Communication Codes (C-series: 0x2000-0x2FFF)
typedef enum {
    DTC_C0000 = 0x2000,  // No network errors
    DTC_C1001 = 0x2001,  // CAN bus off
    DTC_C1002 = 0x2002,  // CAN timeout (Front BCM not responding)
    DTC_C1003 = 0x2003,  // CAN timeout (Rear BCM not responding)
    DTC_C1004 = 0x2004,  // CAN timeout (Qt not responding)
    DTC_C1010 = 0x2010,  // ECU internal communication fault
    DTC_C1020 = 0x2020,  // BCM internal communication fault
} NetworkDTC_t;

// Severity Levels for Error Display
typedef enum {
    DTC_SEVERITY_INFO       = 0x00,  // Informational (no banner)
    DTC_SEVERITY_WARNING    = 0x01,  // Yellow banner
    DTC_SEVERITY_ERROR      = 0x02,  // Red banner
    DTC_SEVERITY_CRITICAL   = 0x03   // Red flashing banner + audible alert
} DTC_Severity_t;


// ==============================================================================
// 6. ERROR REPORT STRUCTURE (CAN_ID_ERROR_*) - EXTENDED DIAGNOSTICS
// ==============================================================================

// Byte 0: Severity Level (DTC_Severity_t)
// Byte 1-2: DTC Code (uint16, BodyDTC_t / NetworkDTC_t / PowerTrainDTC_t)
// Byte 3: Error Counter (how many times occurred since boot)
// Byte 4: Timestamp/Frame (can be used for trending)
// Byte 5-7: Additional context (e.g., which actuator, voltage reading, etc.)


// ==============================================================================
// 7. DIAGNOSTIC REQUEST/RESPONSE STRUCTURE
// ==============================================================================

// CAN_ID_DIAG_REQUEST (Qt -> Central ECU)
// Byte 0: Diagnostic Command
#define DIAG_CMD_READ_DTCS         0x01  // Read all active DTCs
#define DIAG_CMD_READ_DTC_COUNT    0x02  // Get count of DTCs
#define DIAG_CMD_CLEAR_DTCS        0x03  // Clear all DTCs (with password)
#define DIAG_CMD_READ_FREEZE_FRAME 0x04  // Read snapshot at error moment
#define DIAG_CMD_READ_LIVE_DATA    0x05  // Get real-time sensor data
// Byte 1-7: Optional parameters (e.g., password for clear)

// CAN_ID_DIAG_RESPONSE (Central ECU -> Qt)
// Multi-frame response (segmented for large data)
// Byte 0: Response Type (mirrors DIAG_CMD_*)
// Byte 1: Total Frames / Frame Index
// Byte 2-7: Payload (DTC codes, counts, sensor values, etc.)


// ==============================================================================
// 8. STATE VALIDATION & APPROVAL RULES (Central ECU Logic)
// ==============================================================================
// These are NOT transmitted but implemented in Central ECU firmware
// Used to make approve/reject decisions

// Rule: Trunk can only open if:
//   - Vehicle is in PARK or NEUTRAL
//   - Vehicle speed <= 5 km/h (stationary)
//   - Engine is running (for powered trunk)
//   - No existing trunk error (DTC_B1020, DTC_B1021)
//   - Doors are locked (security)

// Rule: Wipers can't operate if:
//   - No wiper motor health (DTC_B1010, DTC_B1011)
//   - Washer fluid too low (threshold)

// Rule: Lights can be toggled anytime, but:
//   - Auto mode respects ambient light sensor
//   - High beams flash restricted if on high speed
//   - All light actuators checked for faults (DTC_B1001-B1002)


// ==============================================================================
// 9. COMMAND APPROVAL/REJECTION RESPONSE (Central ECU -> Qt)
// ==============================================================================
// Not a separate CAN ID; uses existing status messages
// Qt polls STATUS messages to see if command was accepted
// Or immediate NACK via ERROR message if validation failed

// Implicit NACK: Error code issued immediately
// Implicit ACK: Status message updates within 100ms of command


// ==============================================================================
// 10. HELPER MACROS
// ==============================================================================

// Combine two bytes into a 16-bit unsigned integer
#define PACK_U16(high, low)         ((uint16_t)(((uint16_t)(high) << 8) | (low)))

// Extract bytes from a 16-bit integer
#define UNPACK_HIGH_BYTE(val)       ((uint8_t)(((uint16_t)(val) >> 8) & 0xFF))
#define UNPACK_LOW_BYTE(val)        ((uint8_t)((uint16_t)(val) & 0xFF))

// Temperature encoding (offset by 40°C to support -40 to +125°C)
#define ENCODE_TEMP(celsius)        ((uint8_t)((celsius) + 40))
#define DECODE_TEMP(encoded)        ((int8_t)(encoded) - 40)

// Battery voltage encoding (scale 8-16V to 0-255)
#define ENCODE_VOLTAGE(volts)       ((uint8_t)(((volts) - 8.0) * 31.875))  // 255/8
#define DECODE_VOLTAGE(encoded)     (8.0 + ((encoded) / 31.875))

// Speed encoding (uint16 in km/h)
#define ENCODE_SPEED(kmh)           ((uint16_t)(kmh))
#define DECODE_SPEED(encoded)       ((uint16_t)(encoded))


// ==============================================================================
// 11. COMMAND VALIDATION RESULT CODES
// ==============================================================================
// Sent back in error message if command rejected
typedef enum {
    VALIDATION_OK               = 0x00,  // Command approved
    VALIDATION_ERR_INVALID_CMD  = 0x01,  // Unknown command
    VALIDATION_ERR_BAD_STATE    = 0x02,  // Vehicle state prevents action
    VALIDATION_ERR_SPEED        = 0x03,  // Vehicle moving, can't execute
    VALIDATION_ERR_GEAR         = 0x04,  // Wrong gear (e.g., not in PARK)
    VALIDATION_ERR_FAULT        = 0x05,  // Relevant actuator has fault
    VALIDATION_ERR_SAFETY       = 0x06,  // Safety lock (e.g., doors not locked)
    VALIDATION_ERR_TIMEOUT      = 0x07,  // BCM didn't respond to execution
    VALIDATION_ERR_RESOURCE     = 0x08   // Resource busy (e.g., trunk already moving)
} ValidationResult_t;


#ifdef __cplusplus
}
#endif

#endif // CAN_MESSAGES_H
