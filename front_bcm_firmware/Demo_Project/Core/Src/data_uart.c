/*
 * data_uart.c
 *
 *  Created on: Sep 4, 2026
 *      Author: MAC
 */

#include "data_uart.h"

static uint8_t uartRxByte;
static uint8_t rxPacket[4];
static uint8_t rxIndex = 0;
static UART_HandleTypeDef *pUartHandle;


uint8_t currentCmdWiper = 0;
uint8_t currentCmdTurn = 0;

void Bridge_Init(UART_HandleTypeDef *huart) {
    pUartHandle = huart;
    // Vẫn duy trì ngắt nhận (RX) để giữ khả năng giao tiếp hai chiều khi cần
    HAL_UART_Receive_IT(pUartHandle, &uartRxByte, 1);
}

// Thêm vào data_uart.c (của F411)
void Bridge_SendError(uint8_t errorCode, uint8_t errorMask) {
    uint8_t txPacket[4];
    txPacket[0] = 0xEE;                           // Header ĐẶC BIỆT dành cho việc báo lỗi
    txPacket[1] = errorCode;                      // Ví dụ: ERR_WIPER_STALL (0x06)
    txPacket[2] = errorMask;                      // Ví dụ: CMD_FRONT_WIPER (0x04)
    txPacket[3] = (uint8_t)(errorCode + errorMask); // Checksum

    HAL_UART_Transmit(pUartHandle, txPacket, 4, 100);
}

/* Hàm truyền dữ liệu trạng thái từ MCU lên host/Mac hoặc con F103 (TX) */
void Bridge_SendStatus(uint8_t wiperMode, uint8_t turnMode) {
    uint8_t txPacket[4];
    txPacket[0] = 0x55;                           // Header nhận diện dữ liệu do STM32 gửi lên
    txPacket[1] = wiperMode;                      // Trạng thái gạt mưa thực tế
    txPacket[2] = turnMode;                       // Trạng thái xi-nhan / hazard thực tế
    txPacket[3] = (uint8_t)(wiperMode + turnMode); // Checksum đơn giản

    // Truyền gói tin 4 byte qua UART
    HAL_UART_Transmit(pUartHandle, txPacket, 4, 100);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == pUartHandle->Instance) {
        if (rxIndex == 0) {
            if (uartRxByte == 0xAA) {
                rxPacket[rxIndex++] = uartRxByte;
            }
        } else {
            rxPacket[rxIndex++] = uartRxByte;

            if (rxIndex >= 4) {
                uint8_t cmdWiper = rxPacket[1];
                uint8_t cmdTurn  = rxPacket[2];
                uint8_t checksum = rxPacket[3];

                if (checksum == (uint8_t)(cmdWiper + cmdTurn)) {
                    currentCmdWiper = cmdWiper;
                    currentCmdTurn  = cmdTurn;
                }
                rxIndex = 0;
            }
        }
        HAL_UART_Receive_IT(pUartHandle, &uartRxByte, 1);
    }
}
