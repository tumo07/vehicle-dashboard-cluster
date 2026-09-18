/**
 * =============================================================================
 * @file    can_messages.h
 * @project Smart Vehicle Dashboard Cluster
 * @version 3.0 - Complete Coordinator Architecture
 * =============================================================================
 *
 * ARCHITECTURE OVERVIEW
 * =====================
 *
 * MESSAGE FLOW:
 *  [1] User presses Qt button
 *  [2] Qt --(GROUP A 0x10x)--> Central ECU: "I want to open trunk"
 *  [3] Central ECU validates against state machine:
 *       |-- REJECTED --(0x105 NACK)--> Qt: banner "Trunk locked - vehicle moving"
 *       `-- APPROVED -+--(GROUP B 0x21x)--> Rear BCM: "Execute trunk open"
 *                     `--(GROUP C 0x303)--> Qt: "Trunk state: OPENING"
 *  [4] Rear BCM executes, reports every 200ms --(GROUP D 0x41x)--> Central ECU
 *  [5] Central ECU relays --(GROUP C 0x303)--> Qt: "Trunk 45%..."
 *  [6] Fault: BCM --(GROUP E 0x51x)--> Central ECU {DTC_B1020}
 *      Central ECU --(GROUP F 0x600)--> Qt: [RED BANNER "B1020 Trunk motor stall"]
 *  [7] Qt diag: Qt --(0x104)--> Central ECU: "Read DTCs"
 *      Central ECU --(0x305, multi-frame)--> Qt: [DTC list]
 *
 * ID MAP:
 *   0x100-0x105  GROUP A  Qt <-> Central ECU  (commands + ACK)
 *   0x200-0x211  GROUP B  Central ECU -> BCMs (approved commands)
 *   0x300-0x305  GROUP C  Central ECU -> Qt   (real-time status)
 *   0x400-0x411  GROUP D  BCMs -> Central ECU (status + sensors)
 *   0x500-0x510  GROUP E  BCMs -> Central ECU (fault reports)
 *   0x600-0x610  GROUP F  Central ECU -> Qt   (banner + diag)
 *   0x700-0x720  GROUP G  Heartbeat & sync
 *   0x130        BLINK_TICK  turn-signal sync
 * =============================================================================
 */

#ifndef CAN_MESSAGES_H
#define CAN_MESSAGES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * SECTION 1: NODE IDENTIFIERS
 * ============================================================================ */
#define NODE_ID_TRANSLATOR      0x00U
#define NODE_ID_CENTRAL_ECU     0x01U
#define NODE_ID_FRONT_BCM       0x02U
#define NODE_ID_REAR_BCM        0x03U

/* ============================================================================
 * SECTION 2: CAN MESSAGE IDs
 * ============================================================================ */

/* GROUP A: Qt -> Central ECU (User Commands) */
#define CAN_ID_CMD_LIGHT_CONTROL    0x100U  /* DLC:2 Byte0:CmdLight_t Byte1:brightness 0-100% */
#define CAN_ID_CMD_WIPER_CONTROL    0x101U  /* DLC:2 Byte0:CmdWiper_t Byte1:0x01=spray */
#define CAN_ID_CMD_TURN_SIGNAL      0x102U  /* DLC:1 Byte0:CmdTurn_t */
#define CAN_ID_CMD_TRUNK_CONTROL    0x103U  /* DLC:1 Byte0:CmdTrunk_t — validated by Central ECU */
#define CAN_ID_CMD_DIAGNOSTIC       0x104U  /* DLC:2-8 Byte0:DiagCmd_t Byte1-7:params */
#define CAN_ID_CMD_ACK              0x105U
/* Central ECU -> Qt, within 10ms of any GROUP A message
 * DLC:3 Byte0:cmd-id-low-nibble Byte1:AckStatus_t Byte2:ValidationResult_t */

/* GROUP B: Central ECU -> BCMs (Approved Commands Only) */
#define CAN_ID_EXEC_FRONT_LIGHTS    0x200U  /* DLC:2 Byte0:ExecFrontLight_t Byte1:brightness */
#define CAN_ID_EXEC_FRONT_WIPERS    0x201U  /* DLC:2 Byte0:CmdWiper_t Byte1:spray */
#define CAN_ID_EXEC_FRONT_TURN      0x202U  /* DLC:1 Byte0:ExecTurn_t bitmask */
#define CAN_ID_EXEC_REAR_TURN       0x210U  /* DLC:1 Byte0:ExecTurn_t bitmask */
#define CAN_ID_EXEC_REAR_TRUNK      0x211U  /* DLC:1 Byte0:CmdTrunk_t */

/* GROUP C: Central ECU -> Qt (Real-time Status) */
#define CAN_ID_STATUS_VEHICLE_STATE 0x300U
/* DLC:7 every 100ms
 * Byte0: VehicleGear_t
 * Byte1: VehicleStateFlags_t bitmask
 * Byte2-3: Speed km/h big-endian uint16
 * Byte4: Fuel 0-100%
 * Byte5: Coolant temp ENCODE_TEMP()
 * Byte6: Battery voltage ENCODE_VOLTAGE() */

#define CAN_ID_STATUS_LIGHTS_STATE  0x301U
/* DLC:2 on-change or every 500ms
 * Byte0: LightStatusFlags_t bitmask
 * Byte1: CmdWiper_t current wiper mode */

#define CAN_ID_STATUS_TURN_BLINK    0x302U
/* DLC:1 every 500ms in phase with BLINK_TICK
 * Byte0: bit0=L_phase bit1=R_phase (1=LED ON) */

#define CAN_ID_STATUS_TRUNK_STATE   0x303U
/* DLC:2 every 200ms while moving, 1s when idle
 * Byte0: open % 0-100%
 * Byte1: TrunkMotorState_t */

#define CAN_ID_STATUS_REVERSE_RADAR 0x304U
/* DLC:4 every 100ms in GEAR_REVERSE
 * Byte0-1: distance cm PACK_U16 (0xFFFF=no object)
 * Byte2: ParkingLevel_t
 * Byte3: 0x00 reserved */

#define CAN_ID_STATUS_DIAG_RESPONSE 0x305U
/* DLC:8 multi-frame response
 * Byte0: DiagCmd_t echoed
 * Byte1: frame index (0=first)
 * Byte2: total frames
 * Byte3-7: payload (DTC codes, counters, values) */

/* GROUP D: BCMs -> Central ECU (Status + Sensor Reports) */
#define CAN_ID_REPORT_FRONT_STATUS  0x400U
/* DLC:3 every 200ms
 * Byte0: FrontActuatorFlags_t
 * Byte1: CmdWiper_t current mode
 * Byte2: wiper motor health 0-100% */

#define CAN_ID_REPORT_FRONT_SENSORS 0x401U
/* DLC:2 every 500ms
 * Byte0: ambient light 0-100%
 * Byte1: washer fluid 0-100% */

#define CAN_ID_REPORT_REAR_STATUS   0x410U
/* DLC:3 every 200ms
 * Byte0: RearActuatorFlags_t
 * Byte1: TrunkMotorState_t
 * Byte2: trunk open % 0-100% */

#define CAN_ID_REPORT_REAR_SENSORS  0x411U
/* DLC:4 every 100ms
 * Byte0-1: HC-SR04 distance cm PACK_U16 (0xFFFF=no object)
 * Byte2: trunk hall-effect (0=CLOSED 1=OPEN)
 * Byte3: ParkingLevel_t */

/* GROUP E: BCMs -> Central ECU (Fault / DTC Reports) */
#define CAN_ID_FAULT_FRONT_BCM      0x500U
/* DLC:5
 * Byte0: DTC_Severity_t
 * Byte1-2: DTC code big-endian uint16
 * Byte3: occurrence counter (wraps 255)
 * Byte4: SimpleErrorCode_t */

#define CAN_ID_FAULT_REAR_BCM       0x510U  /* DLC:5 same layout */

/* GROUP F: Central ECU -> Qt (Banner Display + Diagnostic Events) */
#define CAN_ID_BANNER_FAULT         0x600U
/* DLC:6 sent immediately after Central ECU processes a fault
 * Byte0: NODE_ID_* source
 * Byte1: DTC_Severity_t -> Qt banner colour (INFO=blue WARN=yellow ERR=red CRIT=flashing)
 * Byte2-3: DTC code big-endian uint16
 * Byte4: occurrence counter
 * Byte5: SimpleErrorCode_t */

#define CAN_ID_BANNER_CLEAR         0x601U
/* DLC:2 fault resolved, Qt dismisses banner
 * Byte0-1: DTC code (0x0000=clear ALL) */

#define CAN_ID_WATCHDOG_ALERT       0x610U
/* DLC:2 node stopped heartbeating
 * Byte0: NODE_ID_* silent node
 * Byte1: seconds since last heartbeat */

/* GROUP G: Heartbeat & Sync */
#define CAN_ID_HEARTBEAT_CENTRAL    0x700U  /* DLC:2 every 100ms Byte0:counter Byte1:HeartbeatFlags_t */
#define CAN_ID_HEARTBEAT_FRONT_BCM  0x710U  /* DLC:2 same layout */
#define CAN_ID_HEARTBEAT_REAR_BCM   0x720U  /* DLC:2 same layout */

#define CAN_ID_BLINK_TICK           0x130U
/* Central ECU -> All BCMs every 500ms
 * BCMs toggle armed turn-signal GPIO on receipt -> perfect sync
 * DLC:1 Byte0:0x01 */


/* ============================================================================
 * SECTION 3: COMMAND PAYLOAD ENUMS
 * ============================================================================ */

typedef enum {
    CMD_LIGHT_OFF           = 0x00,
    CMD_LIGHT_DRL_ON        = 0x01,
    CMD_LIGHT_HEADLIGHT_LOW = 0x02,
    CMD_LIGHT_HEADLIGHT_HIGH= 0x03,
    CMD_LIGHT_FOG_ON        = 0x04,
    CMD_LIGHT_FOG_OFF       = 0x05,
    CMD_LIGHT_AUTO_MODE     = 0x06
} CmdLight_t;

typedef enum {
    CMD_WIPER_OFF           = 0x00,
    CMD_WIPER_INTERMITTENT  = 0x01,
    CMD_WIPER_SLOW          = 0x02,
    CMD_WIPER_NORMAL        = 0x03,
    CMD_WIPER_FAST          = 0x04,
    CMD_WIPER_AUTO          = 0x05
} CmdWiper_t;

typedef enum {
    CMD_TURN_OFF            = 0x00,
    CMD_TURN_LEFT           = 0x01,
    CMD_TURN_RIGHT          = 0x02,
    CMD_TURN_HAZARD         = 0x03
} CmdTurn_t;

typedef enum {
    CMD_TRUNK_NO_ACTION     = 0x00,
    CMD_TRUNK_OPEN          = 0x01,
    CMD_TRUNK_CLOSE         = 0x02,
    CMD_TRUNK_STOP          = 0x03
} CmdTrunk_t;

typedef enum {
    DIAG_CMD_READ_ALL_DTCS      = 0x01,
    DIAG_CMD_READ_DTC_COUNT     = 0x02,
    DIAG_CMD_CLEAR_ALL_DTCS     = 0x03,
    DIAG_CMD_READ_FREEZE_FRAME  = 0x04,
    DIAG_CMD_READ_LIVE_DATA     = 0x05,
    DIAG_CMD_ECU_RESET          = 0x06,
    DIAG_CMD_NODE_STATUS        = 0x07
} DiagCmd_t;

typedef enum {
    ACK_STATUS_APPROVED     = 0x00,
    ACK_STATUS_REJECTED     = 0x01,
    ACK_STATUS_PENDING      = 0x02
} AckStatus_t;

/* EXEC bitmasks */
#define EXEC_FRONT_DRL          (1U << 0)
#define EXEC_FRONT_HEADLIGHT    (1U << 1)
#define EXEC_FRONT_HIGH_BEAM    (1U << 2)
#define EXEC_FRONT_FOG          (1U << 3)
#define EXEC_TURN_LEFT_ARM      (1U << 0)
#define EXEC_TURN_RIGHT_ARM     (1U << 1)
#define EXEC_TURN_HAZARD_ARM    (1U << 2)


/* ============================================================================
 * SECTION 4: VEHICLE STATE FLAGS & STATUS PAYLOADS
 * ============================================================================ */

typedef enum {
    GEAR_PARK       = 0x00,
    GEAR_REVERSE    = 0x01,
    GEAR_NEUTRAL    = 0x02,
    GEAR_DRIVE      = 0x03,
    GEAR_SPORT      = 0x04,
    GEAR_UNKNOWN    = 0xFF
} VehicleGear_t;

/* STATUS_VEHICLE_STATE Byte1 */
#define STATE_ENGINE_RUNNING    (1U << 0)
#define STATE_VEHICLE_MOVING    (1U << 1)  /* speed > 5 km/h */
#define STATE_PARK_BRAKE_ON     (1U << 2)
#define STATE_SEATBELT_OK       (1U << 3)
#define STATE_TRUNK_AJAR        (1U << 4)
#define STATE_DTC_ACTIVE        (1U << 5)
#define STATE_HAZARD_ACTIVE     (1U << 6)
#define STATE_REVERSE_ACTIVE    (1U << 7)

/* STATUS_LIGHTS_STATE Byte0 */
#define STATUS_DRL_ON           (1U << 0)
#define STATUS_HEADLIGHT_ON     (1U << 1)
#define STATUS_HIGH_BEAM_ON     (1U << 2)
#define STATUS_FOG_ON           (1U << 3)
#define STATUS_TURN_LEFT_ON     (1U << 4)
#define STATUS_TURN_RIGHT_ON    (1U << 5)
#define STATUS_HAZARD_ON        (1U << 6)
#define STATUS_BRAKE_ON         (1U << 7)

typedef enum {
    TRUNK_IDLE      = 0x00,
    TRUNK_OPENING   = 0x01,
    TRUNK_CLOSING   = 0x02,
    TRUNK_STALLED   = 0x03,
    TRUNK_UNKNOWN   = 0xFF
} TrunkMotorState_t;

/* REPORT_FRONT_STATUS Byte0 */
#define FRONT_ACT_DRL           (1U << 0)
#define FRONT_ACT_HEADLIGHT     (1U << 1)
#define FRONT_ACT_FOG           (1U << 2)
#define FRONT_ACT_WIPER         (1U << 3)
#define FRONT_ACT_WASHER        (1U << 4)
#define FRONT_ACT_LTURN         (1U << 5)
#define FRONT_ACT_RTURN         (1U << 6)

/* REPORT_REAR_STATUS Byte0 */
#define REAR_ACT_BRAKE          (1U << 0)
#define REAR_ACT_LTURN          (1U << 1)
#define REAR_ACT_RTURN          (1U << 2)
#define REAR_ACT_TRUNK_ACTIVE   (1U << 3)

/* HEARTBEAT Byte1 */
#define HB_INIT_OK              (1U << 0)
#define HB_CAN_OK               (1U << 1)
#define HB_SENSORS_OK           (1U << 2)
#define HB_DTC_ACTIVE           (1U << 3)

typedef enum {
    PARKING_CLEAR    = 0x00,  /* > 100 cm */
    PARKING_CAUTION  = 0x01,  /* 61-100 cm - yellow */
    PARKING_WARNING  = 0x02,  /* 31-60 cm  - orange */
    PARKING_CRITICAL = 0x03   /* 0-30 cm   - red + audio */
} ParkingLevel_t;

#define DIST_THRESHOLD_CAUTION_CM   100U
#define DIST_THRESHOLD_WARNING_CM    60U
#define DIST_THRESHOLD_CRITICAL_CM   30U
#define DIST_MAX_RANGE_CM           400U
#define DIST_NO_OBJECT             0xFFFFU


/* ============================================================================
 * SECTION 5: OBD-STYLE DTC FAULT CODES
 * ============================================================================ */

typedef enum {
    DTC_SEVERITY_INFO     = 0x00,  /* Blue banner   */
    DTC_SEVERITY_WARNING  = 0x01,  /* Yellow banner */
    DTC_SEVERITY_ERROR    = 0x02,  /* Red banner    */
    DTC_SEVERITY_CRITICAL = 0x03   /* Flashing red + audio */
} DTC_Severity_t;

typedef enum {
    DTC_B0000 = 0x1000, /* No body fault                   */
    DTC_B1001 = 0x1001, /* Headlight open circuit (left)   */
    DTC_B1002 = 0x1002, /* Headlight open circuit (right)  */
    DTC_B1003 = 0x1003, /* DRL circuit fault               */
    DTC_B1004 = 0x1004, /* Fog light circuit fault         */
    DTC_B1010 = 0x1010, /* Wiper motor stall / jam         */
    DTC_B1011 = 0x1011, /* Wiper position sensor fault     */
    DTC_B1012 = 0x1012, /* Washer fluid critically low     */
    DTC_B1020 = 0x1020, /* Trunk motor stall               */
    DTC_B1021 = 0x1021, /* Trunk hall sensor fault         */
    DTC_B1022 = 0x1022, /* Trunk open timeout (>5s)        */
    DTC_B1030 = 0x1030, /* Turn signal relay fault (left)  */
    DTC_B1031 = 0x1031, /* Turn signal relay fault (right) */
    DTC_B1040 = 0x1040, /* HC-SR04 ultrasonic read fail    */
    DTC_B1041 = 0x1041, /* Reverse radar out of range      */
    DTC_B1050 = 0x1050, /* Brake light circuit fault       */
} BodyDTC_t;

typedef enum {
    DTC_C0000 = 0x2000, /* No network fault                */
    DTC_C1001 = 0x2001, /* CAN bus-off                     */
    DTC_C1002 = 0x2002, /* Front BCM heartbeat timeout     */
    DTC_C1003 = 0x2003, /* Rear BCM heartbeat timeout      */
    DTC_C1004 = 0x2004, /* Translator/Qt link lost         */
    DTC_C1010 = 0x2010, /* CAN TX error counter overflow   */
    DTC_C1011 = 0x2011, /* CAN RX FIFO overflow            */
} NetworkDTC_t;


/* ============================================================================
 * SECTION 6: VALIDATION RULES & RESULT CODES (Central ECU state machine)
 *
 * TRUNK OPEN requires: gear==PARK/NEUTRAL, speed<=5km/h,
 *                      no active DTC_B1020/B1021, trunk idle
 * HIGH BEAM requires:  headlight already ON, speed<140km/h
 * HAZARD: always permitted regardless of gear or speed
 * ============================================================================ */

typedef enum {
    VALIDATION_OK                = 0x00,
    VALIDATION_ERR_BAD_GEAR      = 0x01,
    VALIDATION_ERR_SPEED         = 0x02,
    VALIDATION_ERR_DTC_ACTIVE    = 0x03,
    VALIDATION_ERR_RESOURCE_BUSY = 0x04,
    VALIDATION_ERR_UNKNOWN_CMD   = 0x05,
    VALIDATION_ERR_SAFETY_LOCK   = 0x06,
    VALIDATION_ERR_BCM_OFFLINE   = 0x07,
} ValidationResult_t;


/* ============================================================================
 * SECTION 7: COMPACT ERROR CODES (Byte4 of fault messages)
 * ============================================================================ */

typedef enum {
    ERR_NONE             = 0x00,  /* No fault            -> DTC_B0000    */
    ERR_OPEN_CIRCUIT     = 0x01,  /* Open circuit        -> DTC_B1001-04 */
    ERR_SENSOR_FAULT     = 0x02,  /* Sensor read fail    -> DTC_B1021    */
    ERR_NODE_TIMEOUT     = 0x03,  /* Heartbeat lost      -> DTC_C1002-03 */
    ERR_OVERCURRENT      = 0x04,  /* Motor overcurrent   -> DTC_B1020    */
    ERR_ACTUATOR_STALL   = 0x05,  /* Servo/motor stall   -> DTC_B1020    */
    ERR_WIPER_STALL      = 0x06,  /* Wiper jam           -> DTC_B1010    */
    ERR_RADAR_FAIL       = 0x07,  /* HC-SR04 fail        -> DTC_B1040    */
    ERR_TRUNK_TIMEOUT    = 0x08,  /* Trunk open timeout  -> DTC_B1022    */
    ERR_CAN_BUS_OFF      = 0x09,  /* CAN bus-off         -> DTC_C1001    */
} SimpleErrorCode_t;


/* ============================================================================
 * SECTION 8: HELPER MACROS
 * ============================================================================ */

#define PACK_U16(hi, lo)         ((uint16_t)(((uint16_t)(hi) << 8U) | (uint8_t)(lo)))
#define UNPACK_HIGH_BYTE(val)    ((uint8_t)(((uint16_t)(val) >> 8U) & 0xFFU))
#define UNPACK_LOW_BYTE(val)     ((uint8_t)((uint16_t)(val) & 0xFFU))

#define ENCODE_TEMP(c)           ((uint8_t)((int16_t)(c) + 40))
#define DECODE_TEMP(raw)         ((int8_t)(raw) - 40)

#define ENCODE_VOLTAGE(v)        ((uint8_t)(((float)(v) - 8.0f) * 31.875f))
#define DECODE_VOLTAGE(raw)      (8.0f + ((float)(raw) / 31.875f))

#define ENCODE_SPEED(kmh)        ((uint16_t)(kmh))
#define DECODE_SPEED(raw)        ((uint16_t)(raw))

#define CALC_PARKING_LEVEL(cm) \
    ((cm) <= DIST_THRESHOLD_CRITICAL_CM ? PARKING_CRITICAL : \
     (cm) <= DIST_THRESHOLD_WARNING_CM  ? PARKING_WARNING  : \
     (cm) <= DIST_THRESHOLD_CAUTION_CM  ? PARKING_CAUTION  : PARKING_CLEAR)


#ifdef __cplusplus
}
#endif
#endif /* CAN_MESSAGES_H */
