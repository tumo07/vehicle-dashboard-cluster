/*
 * data_can.c
 *
 *  Created on: Sep 12, 2026
 *      Author: MAC
 */

#include "data_can.h"
#include "main.h"
#include "can_messages.h"

volatile uint32_t rxed_can_id = 0;
volatile uint8_t  rxed_dlc = 0;
volatile uint8_t  rxed_data[8] = {0};
volatile uint32_t rx_counter = 0;

extern UART_HandleTypeDef huart1; // Khai báo extern huart1 để callback CAN có thể gọi truyền UART nếu cần
volatile HAL_StatusTypeDef last_can_status = 0xFF;

void CAN_Init_Filter_And_Start(CAN_HandleTypeDef *hcan) {
    CAN_FilterTypeDef sFilterConfig;
    sFilterConfig.FilterBank = 0;
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
    sFilterConfig.FilterIdHigh = 0x0000;
    sFilterConfig.FilterIdLow = 0x0000;
    sFilterConfig.FilterMaskIdHigh = 0x0000;
    sFilterConfig.FilterMaskIdLow = 0x0000;
    sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
    sFilterConfig.FilterActivation = ENABLE;
    sFilterConfig.SlaveStartFilterBank = 14;

    if (HAL_CAN_ConfigFilter(hcan, &sFilterConfig) != HAL_OK) {
        Error_Handler();
    }

    if (HAL_CAN_Start(hcan) != HAL_OK) {
        Error_Handler();
    }

    if (HAL_CAN_ActivateNotification(hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
        Error_Handler();
    }
}

void CAN_Send_Front_Status(CAN_HandleTypeDef *hcan, uint8_t wiperStatus, uint8_t turnStatus) {
    CAN_TxHeaderTypeDef TxHeader;
    uint8_t txData[2] = {0, 0};
    uint32_t pTxMailbox;

	// --- Byte 0: Actuator mask (Ghép bit dựa theo các define trong header) ---
	// Xử lý Xi-nhan
	if (turnStatus == 1) {        // Left
		txData[0] |= CMD_FRONT_L_TURN;
	} else if (turnStatus == 2) { // Right
		txData[0] |= CMD_FRONT_R_TURN;
	} else if (turnStatus == 3) { // Hazard
		txData[0] |= (CMD_FRONT_L_TURN | CMD_FRONT_R_TURN);
	}

// Xử lý Gạt mưa
	if (wiperStatus > 0) {
		txData[0] |= CMD_FRONT_WIPER;
	}
// --- Byte 1: Wiper flag ---
	// Báo cáo chi tiết cấp độ gạt mưa (0: Off, 1: Int, 2: Low, 3: Hi, 4: Auto) để UI hiển thị
	txData[1] = wiperStatus;

// --- Cấu hình Header ---
	TxHeader.StdId = CAN_ID_FRONT_STATUS; // Sử dụng đúng ID 0x200 báo cáo trạng thái
	TxHeader.ExtId = 0x00;
	TxHeader.RTR = CAN_RTR_DATA;
	TxHeader.IDE = CAN_ID_STD;
	TxHeader.DLC = 2;                     // DLC là 2
	TxHeader.TransmitGlobalTime = DISABLE;

// Kỹ thuật an toàn: Đợi mailbox trống trước khi gửi (tránh treo CAN)
	uint32_t timeout = HAL_GetTick();
	while(HAL_CAN_GetTxMailboxesFreeLevel(hcan) == 0) {
		if (HAL_GetTick() - timeout > 2) return;
	}

    last_can_status = HAL_CAN_AddTxMessage(hcan, &TxHeader, txData, &pTxMailbox);
}

void CAN_Send_Front_Error(CAN_HandleTypeDef *hcan, uint8_t errorCode, uint8_t errorMask) {
    CAN_TxHeaderTypeDef TxHeader;
    uint8_t txData[2];
    uint32_t pTxMailbox;

    txData[0] = errorCode;
    txData[1] = errorMask;

    TxHeader.StdId = CAN_ID_FRONT_ERROR; // ID 0x2FF
    TxHeader.ExtId = 0x00;
    TxHeader.RTR = CAN_RTR_DATA;
    TxHeader.IDE = CAN_ID_STD;
    TxHeader.DLC = 2; // Độ dài 2 byte: [Mã lỗi, Thiết bị lỗi]
    TxHeader.TransmitGlobalTime = DISABLE;

    if(HAL_CAN_GetTxMailboxesFreeLevel(hcan) > 0) {
        HAL_CAN_AddTxMessage(hcan, &TxHeader, txData, &pTxMailbox);
    }
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    if (hcan->Instance == CAN1) {
        CAN_RxHeaderTypeDef RxHeader;
        uint8_t rxData[8];

        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, rxData) == HAL_OK) {

            // Nếu nhận được lệnh ĐIỀU KHIỂN từ MCU Tổng (0x110)
            if (RxHeader.StdId == CAN_ID_FRONT_CMD && RxHeader.DLC == 1) {
                uint8_t rxMask = rxData[0];
                uint8_t cmdWiper = 0;
                uint8_t cmdTurn = 0;

                // 1. Dịch Bitmask sang số nguyên cho F411
                if (rxMask & CMD_FRONT_WIPER) {
                    cmdWiper = 4; // Ví dụ: Bật gạt mưa thì gán mức 4 (Auto)
                }

                if ((rxMask & CMD_FRONT_L_TURN) && (rxMask & CMD_FRONT_R_TURN)) {
                    cmdTurn = 3; // Hazard
                } else if (rxMask & CMD_FRONT_L_TURN) {
                    cmdTurn = 1; // Left
                } else if (rxMask & CMD_FRONT_R_TURN) {
                    cmdTurn = 2; // Right
                }

                // 2. Đóng gói UART và gửi xuống F411
                uint8_t txUart[4];
                txUart[0] = 0xAA; // F411 đang chờ header 0xAA để nhận lệnh
                txUart[1] = cmdWiper;
                txUart[2] = cmdTurn;
                txUart[3] = (uint8_t)(cmdWiper + cmdTurn);

                HAL_UART_Transmit(&huart1, txUart, 4, 10);
            }
        }
    }
}

