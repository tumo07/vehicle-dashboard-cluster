/*
 * data_uart.c
 *
 *  Created on: Sep 12, 2026
 *      Author: MAC
 */

#include "data_uart.h"



static uint8_t uartRxByte;
//static uint8_t rxPacket[4];
//static uint8_t rxIndex = 0;
static UART_HandleTypeDef *pUartHandle;

volatile uint8_t f103_wiperStatus = 0;
volatile uint8_t f103_turnStatus = 0;

extern CAN_HandleTypeDef hcan;
void CAN_Send_Front_Command(CAN_HandleTypeDef *hcan, uint8_t wiperStatus, uint8_t turnStatus);

void F103_Bridge_Init(UART_HandleTypeDef *huart) {
    pUartHandle = huart;
    HAL_UART_Receive_IT(pUartHandle, &uartRxByte, 1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == pUartHandle->Instance) {
        static uint8_t slide_buf[4];
        static uint8_t slide_idx = 0;

        slide_buf[slide_idx++] = uartRxByte;

        // Nếu byte đầu tiên không phải 0x55, dịch mảng sang trái để tìm kiếm header đúng
        if (slide_idx == 1 && slide_buf[0] != 0x55) {
            slide_idx = 0;
        }
        // Khi đã gom đủ 4 byte theo đúng khung truyền
        else if (slide_idx >= 4) {
            uint8_t w = slide_buf[1];
            uint8_t t = slide_buf[2];
            uint8_t cs = slide_buf[3];

            // Kiểm tra checksum khớp hoàn toàn
            if (cs == (uint8_t)(w + t)) {
                f103_wiperStatus = w;
                f103_turnStatus  = t;

                // Gửi dữ liệu ra mạng CAN ngay khi nhận chuẩn
                CAN_Send_Front_Command(&hcan, w, t);
            }

            // LUÔN RESET LẠI INDEX ĐỂ ĐÓN GÓI TIN TIẾP THEO (KHÔNG BỊ KẸT)
            slide_idx = 0;
        }

        // BẮT BUỘC: Tiếp tục lắng nghe byte tiếp theo qua ngắt UART
        HAL_UART_Receive_IT(pUartHandle, &uartRxByte, 1);
    }
}
