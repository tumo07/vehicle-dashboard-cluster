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

void CAN_Send_Front_Command(CAN_HandleTypeDef *hcan, uint8_t wiperStatus, uint8_t turnStatus) {
    CAN_TxHeaderTypeDef TxHeader;
    uint8_t txData[2] = {0}; // Sử dụng 2 byte dữ liệu
    uint32_t pTxMailbox;

    // Byte 0: Mã hóa riêng cho cấp độ gạt mưa (0 -> 4)
    // 0: Off, 1: Int, 2: Low, 3: Hi, 4: Auto
    txData[0] = wiperStatus;

    // Byte 1: Mã hóa riêng cho xi-nhan / hazard (0 -> 3)
    // 0: Off, 1: Left, 2: Right, 3: Hazard
    txData[1] = turnStatus;

    TxHeader.StdId = CAN_ID_FRONT_CMD;
    TxHeader.ExtId = 0x00;
    TxHeader.RTR = CAN_RTR_DATA;
    TxHeader.IDE = CAN_ID_STD;
    TxHeader.DLC = 2; // Đặt độ dài gói tin là 2 byte
    TxHeader.TransmitGlobalTime = DISABLE;

    last_can_status = HAL_CAN_AddTxMessage(hcan, &TxHeader, txData, &pTxMailbox);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    if (hcan->Instance == CAN1) {
        CAN_RxHeaderTypeDef RxHeader;
        uint8_t rxData[8];

        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, rxData) == HAL_OK) {
                    // Cập nhật giá trị vào các biến toàn cục để hiển thị trên Live Expressions
                    rxed_can_id = RxHeader.StdId;
                    rxed_dlc    = RxHeader.DLC;
                    for (uint8_t i = 0; i < rxed_dlc && i < 8; i++) {
                        rxed_data[i] = rxData[i];
                    }
                    rx_counter++; // Tăng bộ đếm để báo hiệu nhận tin thành công
                }
    }
}

