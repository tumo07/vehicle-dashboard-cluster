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
volatile HAL_StatusTypeDef dbg_tx_status;

/* Khai báo biến debug quá trình nhận UART trên Blue Pill 1 */
volatile uint8_t  dbg_bridge_rx_byte = 0;       // Byte vừa nhận được trong ngắt
volatile uint32_t dbg_bridge_rx_count = 0;      // Đếm tổng số byte UART đã nhận
volatile uint8_t  dbg_bridge_packet_type = 0;   // Lưu header vừa bắt được (0x55 hoặc 0xEE)
volatile uint8_t  dbg_slide_buf[6] = {0};       // Đã sửa thành dbg_slide_buf chuẩn xác

extern CAN_HandleTypeDef hcan;

void F103_Bridge_Init(UART_HandleTypeDef *huart) {
    pUartHandle = huart;
    HAL_UART_Receive_IT(pUartHandle, &uartRxByte, 1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == pUartHandle->Instance) {
        static uint8_t slide_idx = 0;

        dbg_bridge_rx_byte = uartRxByte;
        dbg_bridge_rx_count++;

        if (slide_idx == 0) {
			dbg_bridge_packet_type = uartRxByte; // Lưu lại byte header đầu tiên vào đây!
		}

        // Nếu buffer đầy mà chưa khớp header thì dịch trái mảng để tìm header mới
        if (slide_idx >= 6) {
            for (int i = 0; i < 5; i++) {
            	dbg_slide_buf[i] = dbg_slide_buf[i + 1];
            }
            slide_idx = 5;
        }

        dbg_slide_buf[slide_idx++] = uartRxByte;

        // 1. Kiểm tra nếu byte đầu không phải Header hợp lệ (0x55 hoặc 0xEE) thì bỏ qua
        if (slide_idx == 1 && dbg_slide_buf[0] != 0x55 && dbg_slide_buf[0] != 0xEE) {
            slide_idx = 0;
        }
        // 2. Xử lý gói tin STATUS (Header 0x55, đủ 4 bytes)
        else if (dbg_slide_buf[0] == 0x55 && slide_idx >= 4) {
			CAN_TxHeaderTypeDef TxHeader;
			uint8_t can_data[3];
			uint32_t TxMailbox;

			f103_wiperStatus = dbg_slide_buf[1];
			f103_turnStatus  = dbg_slide_buf[2];
			uint8_t checksum = dbg_slide_buf[3];

			if (checksum == (uint8_t)(f103_wiperStatus + f103_turnStatus)) {
				can_data[0] = f103_wiperStatus;
				can_data[1] = f103_turnStatus;
				can_data[2] = 0;

				TxHeader.StdId = CAN_ID_REPORT_FRONT_STATUS;
				TxHeader.RTR   = CAN_RTR_DATA;
				TxHeader.IDE   = CAN_ID_STD;
				TxHeader.DLC   = 3;
				TxHeader.TransmitGlobalTime = DISABLE;

				dbg_tx_status = HAL_CAN_AddTxMessage(&hcan, &TxHeader, can_data, &TxMailbox);
			}

			for(int i=0; i<4; i++) dbg_slide_buf[i] = 0;
			slide_idx = 0;
		}
        // 3. Xử lý gói tin FAULT / DTC (Header 0xEE, đủ 6 bytes)
        else if (dbg_slide_buf[0] == 0xEE && slide_idx >= 6) {
            CAN_TxHeaderTypeDef TxHeader;
            uint8_t can_data[5];
            uint32_t TxMailbox;

            can_data[0] = dbg_slide_buf[1];
            can_data[1] = dbg_slide_buf[2];
            can_data[2] = dbg_slide_buf[3];
            can_data[3] = dbg_slide_buf[4];
            can_data[4] = dbg_slide_buf[5];

            TxHeader.StdId = CAN_ID_FAULT_FRONT_BCM;
            TxHeader.RTR   = CAN_RTR_DATA;
            TxHeader.IDE   = CAN_ID_STD;
            TxHeader.DLC   = 5;
            TxHeader.TransmitGlobalTime = DISABLE;

            dbg_tx_status = HAL_CAN_AddTxMessage(&hcan, &TxHeader, can_data, &TxMailbox);

            for(int i=0; i<6; i++) dbg_slide_buf[i] = 0;
            slide_idx = 0;
        }

        HAL_UART_Receive_IT(pUartHandle, &uartRxByte, 1);
    }
}
