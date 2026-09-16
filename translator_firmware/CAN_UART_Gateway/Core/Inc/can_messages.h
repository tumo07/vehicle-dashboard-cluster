#ifndef CAN_MESSAGES_H
#define CAN_MESSAGES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==============================================================================
// 1. CAN MESSAGE IDs
// ==============================================================================

// Group A: Main ECU -> Bus (Sensor Data, 100ms)
#define CAN_ID_SPEED            0x100  // DLC: 2 (High byte, Low byte, uint16)
#define CAN_ID_FUEL             0x101  // DLC: 1 (0-100%, uint8)
#define CAN_ID_HEADLIGHT_STATE  0x102  // DLC: 1 (0x00=OFF, 0x01=ON)

// Group B: Main ECU -> Bus (Sync Tick, 500ms)
#define CAN_ID_BLINK_TICK       0x130  // DLC: 1 (0x01 = Sync pulse)

// Group C & D: Qt (via Translator) -> BCMs (Commands)
#define CAN_ID_FRONT_CMD        0x110  // DLC: 1 (Bitmask)
#define CAN_ID_REAR_CMD         0x120  // DLC: 1 (Bitmask)

// Group E & F: BCMs -> Bus (Status & Sensor)
#define CAN_ID_FRONT_STATUS     0x200  // DLC: 2 (Byte0: Actuator mask, Byte1: Wiper flag)
#define CAN_ID_REAR_STATUS      0x300  // DLC: 2 (Byte0: Actuator mask, Byte1: Trunk pos)
#define CAN_ID_REAR_DISTANCE    0x301  // DLC: 2 (Byte0: High, Byte1: Low - HC-SR04 cm)

// Group G: Errors
#define CAN_ID_FRONT_ERROR      0x2FF  // DLC: 2 (Byte0: Code, Byte1: Mask)
#define CAN_ID_REAR_ERROR       0x3FF  // DLC: 2 (Byte0: Code, Byte1: Mask)
#define CAN_ID_SYSTEM_ERROR     0x700  // DLC: 3 (Byte0: Node, Byte1: Code, Byte2: Mask)


// ==============================================================================
// 2. COMMAND BITMASKS
// ==============================================================================

// Front Command Bitmask (CAN_ID_FRONT_CMD)
#define CMD_FRONT_HEADLIGHT     (1 << 0)  // 0x01
#define CMD_FRONT_WASHER        (1 << 1)  // 0x02
#define CMD_FRONT_WIPER         (1 << 2)  // 0x04
#define CMD_FRONT_L_TURN        (1 << 3)  // 0x08
#define CMD_FRONT_R_TURN        (1 << 4)  // 0x10

// Rear Command Bitmask (CAN_ID_REAR_CMD)
#define CMD_REAR_L_TURN         (1 << 0)  // 0x01
#define CMD_REAR_R_TURN         (1 << 1)  // 0x02
#define CMD_REAR_TRUNK_OPEN     (1 << 2)  // 0x04
#define CMD_REAR_TRUNK_CLOSE    (1 << 3)  // 0x08


// ==============================================================================
// 3. ERROR CODES
// ==============================================================================

typedef enum {
    ERR_NONE                = 0x00,
    ERR_ACTUATOR_FAULT      = 0x01,  // Relay or LED fault
    ERR_TRUNK_SENSOR        = 0x02,  // Ultrasonic read fail
    ERR_NODE_TIMEOUT        = 0x03,  // BCM offline (No 0x200/0x300)
    ERR_OVERCURRENT         = 0x04,  // Motor current too high
    ERR_TRUNK_STALL         = 0x05,  // Servo position error
    ERR_WIPER_STALL         = 0x06,  // Wiper motor jammed
    ERR_HEADLIGHT_OPEN      = 0x07   // Headlight open circuit
} SystemErrorCode_t;

// Node Identifiers for 0x700 Error Message
#define NODE_ID_MAIN_ECU        0x01
#define NODE_ID_FRONT_BCM       0x02
#define NODE_ID_REAR_BCM        0x03


// ==============================================================================
// 4. HELPER MACROS
// ==============================================================================

// Combine two bytes into a 16-bit unsigned integer (e.g., Speed, Distance)
#define PACK_U16(high, low)     ((uint16_t)(((uint16_t)(high) << 8) | (low)))

// Extract bytes from a 16-bit integer
#define UNPACK_HIGH_BYTE(val)   ((uint8_t)(((uint16_t)(val) >> 8) & 0xFF))
#define UNPACK_LOW_BYTE(val)    ((uint8_t)((uint16_t)(val) & 0xFF))

#ifdef __cplusplus
}
#endif

#endif // CAN_MESSAGES_H
