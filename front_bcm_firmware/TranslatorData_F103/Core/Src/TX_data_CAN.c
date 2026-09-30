/*
 * TX_data_CAN.c
 * Module điều khiển truyền CAN trên F103
 */

#include "TX_data_CAN.h"
#include "main.h"
#include "read_TX_data_CAN.h"
#include <stdio.h>
#include <string.h>

// Bắt buộc đặt khai báo extern này ở phạm vi toàn cục (ngoài tất cả các hàm)
extern UART_HandleTypeDef huart2;

void CAN_Init_Filter_And_Start(CAN_HandleTypeDef *hcan) {
    CAN_FilterTypeDef sFilterConfig;
    sFilterConfig.FilterBank           = 0;
    sFilterConfig.FilterMode           = CAN_FILTERMODE_IDMASK;
    sFilterConfig.FilterScale          = CAN_FILTERSCALE_32BIT;
    sFilterConfig.FilterIdHigh         = 0x0000;
    sFilterConfig.FilterIdLow          = 0x0000;
    sFilterConfig.FilterMaskIdHigh     = 0x0000;
    sFilterConfig.FilterMaskIdLow      = 0x0000;
    sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
    sFilterConfig.FilterActivation     = ENABLE;
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

HAL_StatusTypeDef CAN_Transmit_Direct(CAN_HandleTypeDef *hcan, uint16_t stdId, uint8_t dlc, const uint8_t *pData) {
    CAN_TxHeaderTypeDef TxHeader;
    uint32_t TxMailbox;

    TxHeader.StdId              = stdId;
    TxHeader.RTR                = CAN_RTR_DATA;
    TxHeader.IDE                = CAN_ID_STD;
    TxHeader.DLC                = dlc;
    TxHeader.TransmitGlobalTime = DISABLE;

    // Ghi dữ liệu log ra cổng USART2 cho script Python (timeout rút ngắn xuống 2ms)
    char msg[64];
    int len = snprintf(msg, sizeof(msg), "[CAN TX] ID:0x%03X DLC:%d Data:", stdId, dlc);
    for (int i = 0; i < dlc; i++) {
        len += snprintf(msg + len, sizeof(msg) - len, " %02X", pData[i]);
    }
    snprintf(msg + len, sizeof(msg) - len, "\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), 10);

    // Nếu Mailbox đầy (chưa cắm bus CAN ngoài), thoát ngay không block CPU
    if (HAL_CAN_GetTxMailboxesFreeLevel(hcan) == 0) {
        return HAL_BUSY;
    }

    HAL_GPIO_TogglePin(LED_CAN_DEBUG_GPIO_Port, LED_CAN_DEBUG_Pin);
    return HAL_CAN_AddTxMessage(hcan, &TxHeader, (uint8_t *)pData, &TxMailbox);
}

void CAN_Send_Heartbeat(CAN_HandleTypeDef *hcan, uint8_t counter, uint8_t flags) {
    CAN_TxHeaderTypeDef TxHeader;
    uint8_t txData[2] = { counter, flags };
    uint32_t pTxMailbox;

    TxHeader.StdId              = CAN_ID_HEARTBEAT_FRONT_BCM;
    TxHeader.RTR                = CAN_RTR_DATA;
    TxHeader.IDE                = CAN_ID_STD;
    TxHeader.DLC                = 2;
    TxHeader.TransmitGlobalTime = DISABLE;

    uint32_t timeout = HAL_GetTick();
    while (HAL_CAN_GetTxMailboxesFreeLevel(hcan) == 0) {
        if (HAL_GetTick() - timeout > 2) return;
    }
    HAL_GPIO_TogglePin(LED_CAN_DEBUG_GPIO_Port, LED_CAN_DEBUG_Pin);
    HAL_CAN_AddTxMessage(hcan, &TxHeader, txData, &pTxMailbox);
}

void CAN_Send_Front_Status(CAN_HandleTypeDef *hcan, uint8_t wiperStatus, uint8_t turnStatus) {
    CAN_TxHeaderTypeDef TxHeader;
    uint8_t txData[3] = {0, 0, 100};
    uint32_t pTxMailbox;

    if (turnStatus == 1)      txData[0] |= FRONT_ACT_LTURN;
    else if (turnStatus == 2) txData[0] |= FRONT_ACT_RTURN;
    else if (turnStatus == 3) txData[0] |= (FRONT_ACT_LTURN | FRONT_ACT_RTURN);

    if (wiperStatus > 0)      txData[0] |= FRONT_ACT_WIPER;
    txData[1] = wiperStatus;

    TxHeader.StdId              = CAN_ID_REPORT_FRONT_STATUS;
    TxHeader.RTR                = CAN_RTR_DATA;
    TxHeader.IDE                = CAN_ID_STD;
    TxHeader.DLC                = 3;
    TxHeader.TransmitGlobalTime = DISABLE;

    uint32_t timeout = HAL_GetTick();
    while (HAL_CAN_GetTxMailboxesFreeLevel(hcan) == 0) {
        if (HAL_GetTick() - timeout > 2) return;
    }
    HAL_CAN_AddTxMessage(hcan, &TxHeader, txData, &pTxMailbox);
}

