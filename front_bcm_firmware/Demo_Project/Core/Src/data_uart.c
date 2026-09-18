/*
 * data_uart.c
 *
 *  Created on: Sep 4, 2026
 *      Author: MAC
 */

#include "data_uart.h"
#include "can_messages.h"

static uint8_t uartRxByte;
static uint8_t rxPacket[4];
static uint8_t rxIndex = 0;
static UART_HandleTypeDef *pUartHandle;

/* Khai báo biến toàn cục dùng để debug UART trên Live Expressions */
volatile uint8_t dbg_uart_tx_buf[6]; // Chứa tối đa 6 bytes của gói tin gửi đi
volatile uint8_t dbg_uart_tx_len = 0;   // Chiều dài gói tin (4 hoặc 6 bytes)
volatile uint32_t dbg_uart_tx_count = 0; // Biến đếm số lần gọi hàm gửi UART


uint8_t currentCmdWiper = 0;
uint8_t currentCmdTurn = 0;

void Bridge_Init(UART_HandleTypeDef *huart) {
    pUartHandle = huart;
    // Vẫn duy trì ngắt nhận (RX) để giữ khả năng giao tiếp hai chiều khi cần
    HAL_UART_Receive_IT(pUartHandle, &uartRxByte, 1);
}

// Hàm gửi lỗi chuẩn v3.0 (6 bytes: Header 0xEE + 5 bytes payload GROUP E 0x500)
void Bridge_SendFault(uint8_t severity, uint16_t dtc_code, uint8_t counter, uint8_t simple_err) {
    uint8_t txPacket[6];
    txPacket[0] = 0xEE;                           // Header nhận diện lỗi
    txPacket[1] = severity;                       // Byte 0: DTC_Severity_t[cite: 1]
    txPacket[2] = UNPACK_HIGH_BYTE(dtc_code);     // Byte 1: DTC Code High[cite: 1]
    txPacket[3] = UNPACK_LOW_BYTE(dtc_code);      // Byte 2: DTC Code Low[cite: 1]
    txPacket[4] = counter;                        // Byte 3: Occurrence counter[cite: 1]
    txPacket[5] = simple_err;                     // Byte 4: SimpleErrorCode_t[cite: 1]

    for(int i = 0; i < 6; i++) {
            dbg_uart_tx_buf[i] = txPacket[i];
        }
        dbg_uart_tx_len = 6;
        dbg_uart_tx_count++;

    HAL_UART_Transmit(pUartHandle, txPacket, 6, 100);
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

// Hàm gửi trạng thái định kỳ gạt mưa và xi-nhan qua UART (Header 0x55)
void Bridge_SendStatus(uint8_t wiperMode, uint8_t turnMode) {
    uint8_t txPacket[4];
    txPacket[0] = 0x55;                       // Header nhận diện Status
    txPacket[1] = wiperMode;                  // Trạng thái gạt mưa
    txPacket[2] = turnMode;                   // Trạng thái xi-nhan
    txPacket[3] = (uint8_t)(wiperMode + turnMode); // Checksum

    for(int i = 0; i < 4; i++) {
            dbg_uart_tx_buf[i] = txPacket[i];
        }
        dbg_uart_tx_len = 4;
        dbg_uart_tx_count++;

    HAL_UART_Transmit(pUartHandle, txPacket, 4, 100);
}
