/*
 * RX_data_CAN.c
 * Translator Node (Bluepill #1) - CAN v3.0 Coordinator Architecture
 *
 * DESIGN: All CAN RX processing is interrupt-driven.
 *   - Known EXEC IDs (0x200/0x201/0x202/0x130) are forwarded to F411 via UART
 *     ONLY when the payload value has changed (change-detection shadow copies).
 *   - All unknown IDs (0x610, 0x300, 0x700, etc.) are silently dropped here.
 *   - The main loop no longer needs to poll TX_UART_Forward_Commands at all.
 */

#include "RX_data_CAN.h"
#include "can_messages.h"
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart1; /* → F411 command channel (USART1) */
extern UART_HandleTypeDef huart2; /* → PC log channel      (USART2) */

/* ── Exported status variables (still available for backward compat) ────── */
volatile uint8_t rx_cmd_light_mask       = 0;
volatile uint8_t rx_cmd_light_brightness = 0;
volatile uint8_t rx_cmd_wiper_mode       = 0;
volatile uint8_t rx_cmd_wiper_washer     = 0;
volatile uint8_t rx_cmd_turn_arm         = 0;
volatile uint8_t rx_cmd_blink_tick       = 0;
volatile uint8_t new_cmd_rx_flag         = 0;

/* ── Shadow copies for change detection (init 0xFF = "never sent") ─────── */
static uint8_t prev_light_mask       = 0xFF;
static uint8_t prev_light_brightness = 0xFF;
static uint8_t prev_wiper_mode       = 0xFF;
static uint8_t prev_wiper_washer     = 0xFF;
static uint8_t prev_turn_arm         = 0xFF;
static uint8_t prev_blink_tick       = 0xFF;

/* ── Internal helpers ───────────────────────────────────────────────────── */
static void Log_To_PC(uint32_t id, uint8_t dlc, uint8_t *data)
{
    char msg[64];
    int len = snprintf(msg, sizeof(msg),
                       "[CAN RX] ID:0x%03X DLC:%d Data:",
                       (unsigned)id, (int)dlc);
    for (int i = 0; i < dlc && i < 8; i++)
        len += snprintf(msg + len, sizeof(msg) - len, " %02X", data[i]);
    snprintf(msg + len, sizeof(msg) - len, "\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), 2);
}

/**
 * @brief  Forward a 5-byte UART packet to F411 only when payload changed.
 *         Format: [0xAA][cmdId][d0][d1][checksum]
 */
static void Forward_If_Changed(uint8_t cmdId, uint8_t d0, uint8_t d1,
                                uint8_t *prev0, uint8_t *prev1)
{
    if (d0 == *prev0 && d1 == *prev1) return; /* identical to last — skip */
    *prev0 = d0;
    *prev1 = d1;

    uint8_t pkt[5];
    pkt[0] = 0xAA;
    pkt[1] = cmdId;
    pkt[2] = d0;
    pkt[3] = d1;
    pkt[4] = (uint8_t)(cmdId + d0 + d1);
    HAL_UART_Transmit(&huart1, pkt, 5, 10);
}

/* ── Legacy parse function (kept for reference, not used in ISR path) ───── */
void RX_Data_Parse_Command(uint32_t stdId, uint8_t *data, uint8_t dlc)
{
    (void)stdId; (void)data; (void)dlc; /* No-op: logic moved into ISR */
}

/* ── CAN RX FIFO0 ISR ─────────────────────────────────────────────────── */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan->Instance != CAN1) return;

    CAN_RxHeaderTypeDef RxHeader;
    uint8_t rxData[8] = {0};

    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, rxData) != HAL_OK) return;
    if (RxHeader.IDE != CAN_ID_STD || RxHeader.RTR != CAN_RTR_DATA) return;

    uint32_t id  = RxHeader.StdId;
    uint8_t  dlc = (uint8_t)RxHeader.DLC;

    /* ── EXEC_FRONT_LIGHTS (0x200): Đèn trước ─────────────────────────── */
    if (id == CAN_ID_EXEC_FRONT_LIGHTS && dlc >= 2) {
        rx_cmd_light_mask       = rxData[0];
        rx_cmd_light_brightness = rxData[1];
        Log_To_PC(id, dlc, rxData);
        Forward_If_Changed(0x01, rxData[0], rxData[1],
                           &prev_light_mask, &prev_light_brightness);
        return;
    }

    /* ── EXEC_FRONT_WIPERS (0x201): Gạt mưa ──────────────────────────── */
    if (id == CAN_ID_EXEC_FRONT_WIPERS && dlc >= 2) {
        rx_cmd_wiper_mode   = rxData[0];
        rx_cmd_wiper_washer = rxData[1];
        Log_To_PC(id, dlc, rxData);
        Forward_If_Changed(0x02, rxData[0], rxData[1],
                           &prev_wiper_mode, &prev_wiper_washer);
        return;
    }

    /* ── EXEC_FRONT_TURN (0x202): Xi-nhan ────────────────────────────── */
    if (id == CAN_ID_EXEC_FRONT_TURN && dlc >= 1) {
        rx_cmd_turn_arm = rxData[0];
        Log_To_PC(id, dlc, rxData);
        uint8_t dummy = 0x00;
        Forward_If_Changed(0x03, rxData[0], 0x00, &prev_turn_arm, &dummy);
        return;
    }

    /* ── BLINK_TICK (0x130): Nhịp nháy xi-nhan (Always forward sync tick) ── */
    if (id == CAN_ID_BLINK_TICK && dlc >= 1) {
        rx_cmd_blink_tick = rxData[0];
        uint8_t pkt[5];
        pkt[0] = 0xAA;
        pkt[1] = 0x04;
        pkt[2] = rxData[0];
        pkt[3] = 0x00;
        pkt[4] = (uint8_t)(0x04 + rxData[0] + 0x00);
        HAL_UART_Transmit(&huart1, pkt, 5, 5);
        return;
    }

    /* ── ALL OTHER IDs (0x610, 0x300, 0x700, etc.): silently drop ──────── */
}
