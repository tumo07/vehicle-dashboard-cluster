/*
 * data_uart.c
 *
 *  Created on: Sep 12, 2026
 *      Author: MAC
 */

#include "data_uart.h"
#include "can_messages.h"
#include "data_can.h"

static uint8_t uartRxByte;
static UART_HandleTypeDef *pUartHandle;

volatile uint8_t f103_wiperStatus = 0;
volatile uint8_t f103_turnStatus = 0;

extern CAN_HandleTypeDef hcan;
void CAN_Send_Front_Status(CAN_HandleTypeDef *hcan, uint8_t actuatorMask, uint8_t wiperFlag);

void F103_Bridge_Init(UART_HandleTypeDef *huart) {
    pUartHandle = huart;
    HAL_UART_Receive_IT(pUartHandle, &uartRxByte, 1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == pUartHandle->Instance) {
        static uint8_t slide_buf[4];
        static uint8_t slide_idx = 0;

        slide_buf[slide_idx++] = uartRxByte;

        // Nếu byte đầu không phải 0x55 (Status) và cũng không phải 0xEE (Error) -> Dịch mảng
        if (slide_idx == 1 && slide_buf[0] != 0x55 && slide_buf[0] != 0xEE) {
            slide_idx = 0;
        }
        // Khi gom đủ 4 byte
        else if (slide_idx >= 4) {
            uint8_t byte1 = slide_buf[1];
            uint8_t byte2 = slide_buf[2];
            uint8_t cs    = slide_buf[3];

            // Kiểm tra checksum
            if (cs == (uint8_t)(byte1 + byte2)) {

                // Rẽ nhánh tùy theo Header từ F411 truyền lên
                if (slide_buf[0] == 0x55) {
                    // Dữ liệu trạng thái bình thường -> Gửi CAN 0x200
                    f103_wiperStatus = byte1;
                    f103_turnStatus  = byte2;
                    CAN_Send_Front_Status(&hcan, byte1, byte2);
                }
                else if (slide_buf[0] == 0xEE) {
                    // Cảnh báo lỗi phần cứng -> Gửi CAN 0x2FF
                    CAN_Send_Front_Error(&hcan, byte1, byte2);
                }
            }
            slide_idx = 0;
        }

        HAL_UART_Receive_IT(pUartHandle, &uartRxByte, 1);
    }
}
