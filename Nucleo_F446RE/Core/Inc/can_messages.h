/* =============================================================================
 * can_messages.h — CAN Message Definitions
 * Project : Smart Vehicle Dashboard Cluster
 * Node    : Nucleo-F446RE (Vehicle Node)
 *
 * CAN Frame Layout:
 *   ID 0x100  Speed      [HIGH_BYTE][LOW_BYTE] 2-byte uint16 km/h (0-999)
 *   ID 0x101  Fuel       [PERCENT]             1-byte uint8  % (0-100)
 *   ID 0x102  Headlight  [STATE]               1-byte 0x00=OFF 0x01=ON
 * ============================================================================= */

#ifndef CAN_MESSAGES_H
#define CAN_MESSAGES_H

#include <stdint.h>

/* ---------------------------------------------------------------------------
 * CAN Standard IDs
 * --------------------------------------------------------------------------- */
#define CAN_ID_SPEED        0x100U
#define CAN_ID_FUEL         0x101U
#define CAN_ID_HEADLIGHT    0x102U

/* ---------------------------------------------------------------------------
 * DLC (Data Length Code) for each frame
 * --------------------------------------------------------------------------- */
#define CAN_DLC_SPEED       2U
#define CAN_DLC_FUEL        1U
#define CAN_DLC_HEADLIGHT   1U

/* ---------------------------------------------------------------------------
 * Speed zone thresholds (km/h)
 * --------------------------------------------------------------------------- */
#define SPEED_SAFE_MAX      60U    /* 0  – 59  km/h → Green LED  */
#define SPEED_MEDIUM_MAX    100U   /* 60 – 100 km/h → Yellow LED */
                                   /* >100 km/h      → Red LED   */

/* ---------------------------------------------------------------------------
 * Headlight states (Byte 0 of frame 0x102)
 * --------------------------------------------------------------------------- */
#define HEADLIGHT_OFF       0x00U
#define HEADLIGHT_ON        0x01U

/* ---------------------------------------------------------------------------
 * Pack helpers — write sensor values into the 8-byte CAN data buffer
 * --------------------------------------------------------------------------- */

/**
 * @brief Pack speed into CAN data buffer (big-endian 16-bit).
 * @param buf   Pointer to 8-byte CAN data array.
 * @param speed Speed value in km/h (0-999).
 */
static inline void CAN_PackSpeed(uint8_t *buf, uint16_t speed)
{
    buf[0] = (uint8_t)((speed >> 8) & 0xFF);
    buf[1] = (uint8_t)(speed & 0xFF);
}

/**
 * @brief Pack fuel level into CAN data buffer.
 * @param buf  Pointer to 8-byte CAN data array.
 * @param fuel Fuel level 0-100 %.
 */
static inline void CAN_PackFuel(uint8_t *buf, uint8_t fuel)
{
    buf[0] = fuel;
}

/**
 * @brief Pack headlight state into CAN data buffer.
 * @param buf   Pointer to 8-byte CAN data array.
 * @param state HEADLIGHT_ON or HEADLIGHT_OFF.
 */
static inline void CAN_PackHeadlight(uint8_t *buf, uint8_t state)
{
    buf[0] = state;
}

/* ---------------------------------------------------------------------------
 * Unpack helpers — read values from received CAN data buffer
 * --------------------------------------------------------------------------- */

/**
 * @brief Unpack speed from received CAN data buffer.
 * @param buf Pointer to received data bytes.
 * @return Speed in km/h.
 */
static inline uint16_t CAN_UnpackSpeed(const uint8_t *buf)
{
    return (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);
}

/**
 * @brief Unpack fuel from received CAN data buffer.
 * @param buf Pointer to received data bytes.
 * @return Fuel percentage 0-100.
 */
static inline uint8_t CAN_UnpackFuel(const uint8_t *buf)
{
    return buf[0];
}

/**
 * @brief Unpack headlight state from received CAN data buffer.
 * @param buf Pointer to received data bytes.
 * @return HEADLIGHT_ON (0x01) or HEADLIGHT_OFF (0x00).
 */
static inline uint8_t CAN_UnpackHeadlight(const uint8_t *buf)
{
    return buf[0];
}

#endif /* CAN_MESSAGES_H */
