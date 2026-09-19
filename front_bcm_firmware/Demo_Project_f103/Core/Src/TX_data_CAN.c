/*
 * data_can.c
 *
 *  Created on: Sep 12, 2026
 *      Author: MAC
 */

#include "TX_data_CAN.h"
#include "main.h"
#include "can_messages.h"

volatile uint32_t rxed_can_id = 0;
volatile uint8_t  rxed_dlc = 0;
volatile uint8_t  rxed_data[8] = {0};
volatile uint32_t rx_counter = 0;

extern UART_HandleTypeDef huart1;
volatile HAL_StatusTypeDef last_can_status = 0xFF;

volatile AckStatus_t ack_status = ACK_STATUS_APPROVED;
volatile uint32_t last_req_tick = 0;

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

void CAN_Send_Heartbeat(CAN_HandleTypeDef *hcan, uint8_t status, uint8_t counter) {
    CAN_TxHeaderTypeDef TxHeader;
    uint8_t txData[2];
    uint32_t pTxMailbox;

    // Đóng gói 2 bytes dữ liệu cho Heartbeat (ví dụ: Byte 0 là trạng thái node, Byte 1 là bộ đếm nhịp)
    txData[0] = status;  // Trạng thái hiện tại (VD: node_state)
    txData[1] = counter; // Biến đếm tăng dần mỗi lần gửi để báo hiệu sống

    TxHeader.StdId = 0x710U;                    // CAN_ID_HEARTBEAT_FRONT_BCM
    TxHeader.RTR = CAN_RTR_DATA;
    TxHeader.IDE = CAN_ID_STD;
    TxHeader.DLC = 2;                             // Độ dài 2 bytes đúng yêu cầu
    TxHeader.TransmitGlobalTime = DISABLE;

    uint32_t timeout = HAL_GetTick();
	while(HAL_CAN_GetTxMailboxesFreeLevel(hcan) == 0) {
		if (HAL_GetTick() - timeout > 2) return; // Timeout sau 2ms nếu bus kẹt
        }
    // Gửi bản tin lên mạng CAN bus
    if (HAL_CAN_AddTxMessage(hcan, &TxHeader, txData, &pTxMailbox) != HAL_OK) {
        // Xử lý lỗi truyền nếu cần
    }
}

void CAN_Send_Front_Status(CAN_HandleTypeDef *hcan, uint8_t wiperStatus, uint8_t turnStatus) {
    CAN_TxHeaderTypeDef TxHeader;
    uint8_t txData[3] = {0, 0, 0};
    uint32_t pTxMailbox;

    // Ánh xạ trạng thái theo chuẩn REPORT_FRONT_STATUS (GROUP D - 0x400)[cite: 1]
    if (turnStatus == 1) {        // Left
        txData[0] |= FRONT_ACT_LTURN;
    } else if (turnStatus == 2) { // Right
        txData[0] |= FRONT_ACT_RTURN;
    } else if (turnStatus == 3) { // Hazard
        txData[0] |= (FRONT_ACT_LTURN | FRONT_ACT_RTURN);
    }

    if (wiperStatus > 0) {
        txData[0] |= FRONT_ACT_WIPER;
    }

    txData[1] = wiperStatus; // Byte 1: CmdWiper_t current mode[cite: 1]
    txData[2] = 100;         // Byte 2: Wiper motor health 0-100%[cite: 1]

    TxHeader.StdId = CAN_ID_REPORT_FRONT_STATUS; // Sử dụng đúng chuẩn 0x400[cite: 1]
    TxHeader.ExtId = 0x00;
    TxHeader.RTR = CAN_RTR_DATA;
    TxHeader.IDE = CAN_ID_STD;
    TxHeader.DLC = 3;                             // DLC = 3 theo chuẩn v3.0[cite: 1]
    TxHeader.TransmitGlobalTime = DISABLE;

    uint32_t timeout = HAL_GetTick();
    while(HAL_CAN_GetTxMailboxesFreeLevel(hcan) == 0) {
        if (HAL_GetTick() - timeout > 2) return;
    }

    last_can_status = HAL_CAN_AddTxMessage(hcan, &TxHeader, txData, &pTxMailbox);
}

void CAN_Send_Join_Request(CAN_HandleTypeDef *hcan) {
    CAN_TxHeaderTypeDef TxHeader;
    uint8_t txData[1];
    uint32_t pTxMailbox;

    txData[0] = 0x01U; // Payload báo hiệu: "Tôi là Node Front BCM muốn kết nối"

    TxHeader.StdId = 0x02U; // ID xin phép theo đúng yêu cầu của bạn
    TxHeader.RTR = CAN_RTR_DATA;
    TxHeader.IDE = CAN_ID_STD;
    TxHeader.DLC = 1;
    TxHeader.TransmitGlobalTime = DISABLE;

    HAL_CAN_AddTxMessage(hcan, &TxHeader, txData, &pTxMailbox);
}

