/*
 * data_uart.c
 *
 *  Created on: Sep 12, 2026
 *      Author: MAC
 */

#include "RX_data_CAN.h"
#include "can_messages.h"
#include "stdio.h"
#include "stdio.h"
#include "TX_data_CAN.h"

static uint8_t uartRxByte;
static UART_HandleTypeDef *pUartHandle;

volatile uint8_t f103_wiperStatus = 0;
volatile uint8_t f103_turnStatus  = 0;
volatile HAL_StatusTypeDef dbg_tx_status;

/* Khai báo biến debug quá trình nhận UART trên Bridge */
volatile uint8_t  dbg_bridge_rx_byte     = 0;  // Byte vừa nhận được trong ngắt
volatile uint32_t dbg_bridge_rx_count    = 0;  // Đếm tổng số byte UART đã nhận
volatile uint8_t  dbg_bridge_packet_type = 0;  // Lưu header vừa bắt được
volatile uint8_t  dbg_slide_buf[6]       = {0};// Buffer dịch để tìm và lọc gói tin

extern CAN_HandleTypeDef hcan;
extern UART_HandleTypeDef huart2;// Khai báo huart2 để in log ra USB-Serial (nếu chưa có)


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
            dbg_bridge_packet_type = uartRxByte; // Lưu lại byte header đầu tiên
        }

        // Nếu buffer đầy mà chưa khớp gói tin thì dịch trái mảng để tìm header mới
        if (slide_idx >= 6) {
            for (int i = 0; i < 5; i++) {
                dbg_slide_buf[i] = dbg_slide_buf[i + 1];
            }
            slide_idx = 5;
        }

        dbg_slide_buf[slide_idx++] = uartRxByte;

        // 1. Kiểm tra nếu byte đầu không phải Header hợp lệ (0x71, 0x41, 0x40, 0x55, 0xEE) thì bỏ qua
        if (slide_idx == 1 && dbg_slide_buf[0] != 0x71 &&
                              dbg_slide_buf[0] != 0x41 &&
                              dbg_slide_buf[0] != 0x40 &&
                              dbg_slide_buf[0] != 0x55 &&
                              dbg_slide_buf[0] != 0xEE) {
            slide_idx = 0;
        }
        // 2. Xử lý gói tin HEARTBEAT (Header 0x71, đủ 3 bytes: Header + Counter + Flags)
        else if (dbg_slide_buf[0] == 0x71 && slide_idx >= 3) {
            CAN_TxHeaderTypeDef TxHeader;
            uint8_t can_data[2];
            uint32_t TxMailbox;

            can_data[0] = dbg_slide_buf[1]; // Counter
            can_data[1] = dbg_slide_buf[2]; // HB Flags

            TxHeader.StdId              = 0x710U; // CAN_ID_HEARTBEAT_FRONT_BCM
            TxHeader.RTR                = CAN_RTR_DATA;
            TxHeader.IDE                = CAN_ID_STD;
            TxHeader.DLC                = 2;
            TxHeader.TransmitGlobalTime = DISABLE;

            dbg_tx_status = HAL_CAN_AddTxMessage(&hcan, &TxHeader, can_data, &TxMailbox);
            // ---> IN LOG RA MÁY TÍNH QUA USB-SERIAL <---
			char log_msg[64];
			int len = snprintf(log_msg, sizeof(log_msg), "[RX OK] Heartbeat (0x71): Counter=%d, Flags=0x%02X\r\n", can_data[0], can_data[1]);
			HAL_UART_Transmit(&huart2, (uint8_t *)log_msg, len, 10);

            for (int i = 0; i < 3; i++) dbg_slide_buf[i] = 0;
            slide_idx = 0;
        }
        // 3. Xử lý gói tin SENSOR REPORT (Header 0x41, đủ 3 bytes: Header + Rain + Water)
        else if (dbg_slide_buf[0] == 0x41 && slide_idx >= 3) {
            CAN_TxHeaderTypeDef TxHeader;
            uint8_t can_data[2];
            uint32_t TxMailbox;

            can_data[0] = dbg_slide_buf[1]; // Rain percent
            can_data[1] = dbg_slide_buf[2]; // Water level percent

            TxHeader.StdId              = CAN_ID_REPORT_FRONT_SENSORS; // 0x401
            TxHeader.RTR                = CAN_RTR_DATA;
            TxHeader.IDE                = CAN_ID_STD;
            TxHeader.DLC                = 2;
            TxHeader.TransmitGlobalTime = DISABLE;

            dbg_tx_status = HAL_CAN_AddTxMessage(&hcan, &TxHeader, can_data, &TxMailbox);
            char log_msg[64];
			int len = snprintf(log_msg, sizeof(log_msg), "[RX OK] Sensor (0x41): Rain=%d%%, Water=%d%%\r\n", can_data[0], can_data[1]);
			HAL_UART_Transmit(&huart2, (uint8_t *)log_msg, len, 10);

            for (int i = 0; i < 3; i++) dbg_slide_buf[i] = 0;
            slide_idx = 0;
        }
        // 4. Xử lý gói tin STATUS REPORT (Header 0x40, đủ 4 bytes: Header + Actuator + Wiper + Health)
        else if (dbg_slide_buf[0] == 0x40 && slide_idx >= 4) {
            CAN_TxHeaderTypeDef TxHeader;
            uint8_t can_data[3];
            uint32_t TxMailbox;

            can_data[0] = dbg_slide_buf[1]; // Actuator flags
            can_data[1] = dbg_slide_buf[2]; // Wiper mode
            can_data[2] = dbg_slide_buf[3]; // Motor health

            TxHeader.StdId              = CAN_ID_REPORT_FRONT_STATUS; // 0x400
            TxHeader.RTR                = CAN_RTR_DATA;
            TxHeader.IDE                = CAN_ID_STD;
            TxHeader.DLC                = 3;
            TxHeader.TransmitGlobalTime = DISABLE;

            dbg_tx_status = HAL_CAN_AddTxMessage(&hcan, &TxHeader, can_data, &TxMailbox);
            char log_msg[64];
			int len = snprintf(log_msg, sizeof(log_msg), "[RX OK] Status (0x40): Act=0x%02X, Wiper=%d, Health=%d\r\n", can_data[0], can_data[1], can_data[2]);
			HAL_UART_Transmit(&huart2, (uint8_t *)log_msg, len, 10);

            for (int i = 0; i < 4; i++) dbg_slide_buf[i] = 0;
            slide_idx = 0;
        }
        // 5. Xử lý gói tin EVENT STATUS (Header 0x55, đủ 4 bytes: Header + Wiper + Turn + Checksum)
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

                TxHeader.StdId              = CAN_ID_REPORT_FRONT_STATUS;
                TxHeader.RTR                = CAN_RTR_DATA;
                TxHeader.IDE                = CAN_ID_STD;
                TxHeader.DLC                = 3;
                TxHeader.TransmitGlobalTime = DISABLE;

                dbg_tx_status = HAL_CAN_AddTxMessage(&hcan, &TxHeader, can_data, &TxMailbox);
                // ---> BỔ SUNG ĐOẠN IN LOG NÀY ĐỂ HIỂN THỊ LÊN CONSOLE <---
				char log_msg[64];
				int len = snprintf(log_msg, sizeof(log_msg), "[RX OK] Event Status (0x55): Wiper=%d, Turn=%d\r\n", can_data[0], can_data[1]);
				HAL_UART_Transmit(&huart2, (uint8_t *)log_msg, len, 10);
            }

            for (int i = 0; i < 4; i++) dbg_slide_buf[i] = 0;
            slide_idx = 0;
        }
        // 6. Xử lý gói tin FAULT / DTC (Header 0xEE, đủ 6 bytes)
        else if (dbg_slide_buf[0] == 0xEE && slide_idx >= 6) {
            CAN_TxHeaderTypeDef TxHeader;
            uint8_t can_data[5];
            uint32_t TxMailbox;

            can_data[0] = dbg_slide_buf[1];
            can_data[1] = dbg_slide_buf[2];
            can_data[2] = dbg_slide_buf[3];
            can_data[3] = dbg_slide_buf[4];
            can_data[4] = dbg_slide_buf[5];

            TxHeader.StdId              = CAN_ID_FAULT_FRONT_BCM;
            TxHeader.RTR                = CAN_RTR_DATA;
            TxHeader.IDE                = CAN_ID_STD;
            TxHeader.DLC                = 5;
            TxHeader.TransmitGlobalTime = DISABLE;

            dbg_tx_status = HAL_CAN_AddTxMessage(&hcan, &TxHeader, can_data, &TxMailbox);
            char log_msg[64];
			int len = snprintf(log_msg, sizeof(log_msg), "[RX OK] Fault (0xEE): Severity=%d, DTC=0x%04X\r\n", can_data[0], (can_data[1]<<8)|can_data[2]);
			HAL_UART_Transmit(&huart2, (uint8_t *)log_msg, len, 10);

            for (int i = 0; i < 6; i++) dbg_slide_buf[i] = 0;
            slide_idx = 0;
        }

        // Tiếp tục lắng nghe byte UART tiếp theo
        HAL_UART_Receive_IT(pUartHandle, &uartRxByte, 1);
    }
}
