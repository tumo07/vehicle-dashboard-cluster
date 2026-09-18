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
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    if (hcan->Instance == CAN1) {
        CAN_RxHeaderTypeDef RxHeader;
        uint8_t rxData[8];

        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, rxData) == HAL_OK) {
//
            // --- ƯU TIÊN 1: XỬ LÝ NHẬN TÍN HIỆU ACK KHI ĐANG CHỜ KHỞI TẠO ---
            if (ack_status == ACK_STATUS_REJECTED) {
                if (RxHeader.StdId == 0x101U && rxData[0] == 0x01U) {
                	ack_status = ACK_STATUS_PENDING; // Được ECU duyệt, chuyển sang trạng thái sẵn sàng!
                }
                return; // Thoát luôn, chưa xử lý lệnh khác khi chưa được cấp phép
            }
//
//            // --- ƯU TIÊN 2: KHI ĐÃ ĐƯỢC DUYỆT (READY), XỬ LÝ CÁC LỆNH ĐIỀU KHIỂN TỪ CAN XUỐNG ---
//            if (node_state == NODE_STATE_READY) {
//
//                // Lưu vết debug để soi trên Live Expressions
//                rxed_can_id = RxHeader.StdId;
//                rxed_dlc    = RxHeader.DLC;
//                for(int i = 0; i < RxHeader.DLC && i < 8; i++) {
//                    rxed_data[i] = rxData[i];
//                }
//                rx_counter++;
//
//                // 1. Xử lý lệnh điều khiển GẠT MƯA (ID: 0x201)
//                if (RxHeader.StdId == CAN_ID_EXEC_FRONT_WIPERS && RxHeader.DLC >= 2) {
//                    uint8_t cmdWiper = rxData[0];
//                    uint8_t cmdSpray = rxData[1];
//
//                    // Đóng gói gửi xuống F411 qua UART
//                    uint8_t txUart[4];
//                    txUart[0] = 0xAA;
//                    txUart[1] = cmdWiper;
//                    txUart[2] = 0;
//                    txUart[3] = (uint8_t)(txUart[1] + txUart[2]); // Checksum
//
//                    HAL_UART_Transmit(&huart1, txUart, 4, 10);
//                }
//
//                // 2. Xử lý lệnh điều khiển XI-NHAN (ID: 0x202)
//                else if (RxHeader.StdId == CAN_ID_EXEC_FRONT_TURN && RxHeader.DLC >= 1) {
//                    uint8_t execTurnMask = rxData[0];
//                    uint8_t cmdTurn = 0;
//
//                    if ((execTurnMask & EXEC_TURN_LEFT_ARM) && (execTurnMask & EXEC_TURN_RIGHT_ARM)) {
//                        cmdTurn = 3; // Hazard
//                    } else if (execTurnMask & EXEC_TURN_LEFT_ARM) {
//                        cmdTurn = 1; // Left
//                    } else if (execTurnMask & EXEC_TURN_RIGHT_ARM) {
//                        cmdTurn = 2; // Right
//                    }
//
//                    // Đóng gói gửi xuống F411 qua UART
//                    uint8_t txUart[4];
//                    txUart[0] = 0xAA;
//                    txUart[1] = 0;
//                    txUart[2] = cmdTurn;
//                    txUart[3] = (uint8_t)(txUart[1] + txUart[2]); // Checksum
//
//                    HAL_UART_Transmit(&huart1, txUart, 4, 10);
//                }
//            }
        }
    }
}
