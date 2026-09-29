/*
 * read_TX_data_CAN.c
 * Logger module: Định dạng và hiển thị chi tiết các bản tin CAN lên Console
 */

#include "read_TX_data_CAN.h"
#include "can_messages.h"
#include <stdio.h>

static UART_HandleTypeDef *pDebugUart = NULL;

void CAN_Logger_Init(UART_HandleTypeDef *huart_debug) {
    pDebugUart = huart_debug;
}

void CAN_Logger_Print_Frame(uint16_t stdId, uint8_t dlc, const uint8_t *pData, HAL_StatusTypeDef status) {
    if (pDebugUart == NULL) return;

    if (status != HAL_OK) {
        printf("[CAN ERR] Gui that bai ID: 0x%03X (Status: %d)\r\n", stdId, status);
        return;
    }

    switch (stdId) {
        // Gói Heartbeat Front BCM (0x710)
        case CAN_ID_HEARTBEAT_FRONT_BCM:
            printf("[CAN TX][0x710 - HEARTBEAT] Count: %3d | Flags: 0x%02X\r\n",
                   pData[0], pData[1]);
            break;

        // Gói Trạng thái cơ cấu chấp hành phía trước (0x400)
        case CAN_ID_REPORT_FRONT_STATUS:
            printf("[CAN TX][0x400 - STATUS   ] ActFlags: 0x%02X | WiperMode: %d | Health: %d%%\r\n",
                   pData[0], pData[1], pData[2]);
            break;

        // Gói Cảm biến mưa và mực nước rửa kính (0x401)
        case CAN_ID_REPORT_FRONT_SENSORS:
            printf("[CAN TX][0x401 - SENSORS  ] Rain: %3d%% | WaterLevel: %3d%%\r\n",
                   pData[0], pData[1]);
            break;

        // Gói Báo lỗi sự cố DTC (0x500)
        case CAN_ID_FAULT_FRONT_BCM:
            printf("[CAN TX][0x500 - FAULT DTC] Sev: %d | DTC: 0x%02X%02X | Count: %d | ErrCode: %d\r\n",
                   pData[0], pData[1], pData[2], pData[3], pData[4]);
            break;

        // Các gói tin chuẩn khác nếu có
        default:
            printf("[CAN TX][0x%03X] DLC: %d | Data: ", stdId, dlc);
            for (uint8_t i = 0; i < dlc; i++) {
                printf("%02X ", pData[i]);
            }
            printf("\r\n");
            break;
    }
}
