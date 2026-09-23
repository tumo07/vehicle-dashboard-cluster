/*
 * RX_data_UART.c
 * Front BCM (STM32F411) - UART RX Parser from Bridge Node
 */

#include "RX_data_UART.h"
#include "turnsignal.h"
#include "wiper.h"
#include "can_messages.h"

static uint8_t rxByte;
static uint8_t rxPacket[5];
static uint8_t rxIndex = 0;
static UART_HandleTypeDef *pRxUart;

// Các biến lưu trữ lệnh từ Central ECU
volatile uint8_t   f411_cmd_light_mask       = 0;
volatile uint8_t   f411_cmd_light_brightness = 0;
volatile CmdWiper_t f411_cmd_wiper_mode      = CMD_WIPER_OFF;
volatile uint8_t   f411_cmd_wiper_washer     = 0;
volatile CmdTurn_t  f411_cmd_turn_arm        = CMD_TURN_OFF;
volatile uint8_t   f411_cmd_blink_tick       = 0;
volatile uint8_t   f411_new_cmd_flag         = 0;

extern uint8_t currentCmdWiper;
extern uint8_t currentCmdTurn;

void Front_UART_RX_Init(UART_HandleTypeDef *huart) {
    pRxUart = huart;
    rxIndex = 0;
    HAL_UART_Receive_IT(pRxUart, &rxByte, 1);
}

void Front_UART_Parse_Packet(uint8_t *packet) {
    uint8_t cmdId    = packet[1];
    uint8_t data0    = packet[2];
    uint8_t data1    = packet[3];
    uint8_t checksum = packet[4];

    // Kiểm tra tính toàn vẹn Checksum = (CmdID + Data0 + Data1)
    if (checksum != (uint8_t)(cmdId + data0 + data1)) {
        return; // Sai Checksum -> Hủy gói tin rác
    }

    f411_new_cmd_flag = 1;

    switch (cmdId) {
        case 0x01: // Lệnh Đèn trước (CAN ID 0x200)
            f411_cmd_light_mask       = data0;
            f411_cmd_light_brightness = data1;
            break;

        case 0x02: // Lệnh Gạt mưa (CAN ID 0x201)
            f411_cmd_wiper_mode   = (CmdWiper_t)data0;
            f411_cmd_wiper_washer = data1;
            currentCmdWiper       = data0; // Cập nhật sang biến điều khiển của module wiper
            break;

        case 0x03: // Lệnh Xi-nhan (CAN ID 0x202)
            f411_cmd_turn_arm = (CmdTurn_t)data0;
            currentCmdTurn    = data0; // Cập nhật sang module turnsignal
            break;

        case 0x04: // Lệnh Blink Tick (CAN ID 0x130)
            f411_cmd_blink_tick = data0;
            break;

        default:
            break;
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (pRxUart != NULL && huart->Instance == pRxUart->Instance) {
        if (rxIndex == 0) {
            // Tìm byte mở đầu Start-of-Frame
            if (rxByte == 0xAA) {
                rxPacket[rxIndex++] = rxByte;
            }
        } else {
            rxPacket[rxIndex++] = rxByte;

            // Đã gom đủ 5 byte: [0xAA] [CmdID] [Data0] [Data1] [Checksum]
            if (rxIndex >= 5) {
                Front_UART_Parse_Packet(rxPacket);
                rxIndex = 0; // Reset bộ đệm để đón gói tiếp theo
            }
        }
        // Tiếp tục lắng nghe byte tiếp theo
        HAL_UART_Receive_IT(pRxUart, &rxByte, 1);
    }
}
