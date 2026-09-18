#include "can_receiver.h"
#include "main.h"
#include "can_messages.h"

volatile uint32_t rxed_can_id = 0;
volatile uint8_t  rxed_dlc = 0;
volatile uint8_t  rxed_data[8] = {0};
volatile uint32_t rx_counter = 0;
volatile uint8_t monitored_wiper = 0;
volatile uint8_t monitored_turn  = 0;

// Các biến DTC v3.0[cite: 1]
volatile uint8_t  dtc_severity = 0;
volatile uint16_t dtc_code_received = 0;
volatile uint8_t  dtc_counter = 0;
volatile uint8_t  simple_error_code = 0;

void CAN_Receiver_Init(CAN_HandleTypeDef *hcan) {
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

    HAL_CAN_ConfigFilter(hcan, &sFilterConfig);
    HAL_CAN_Start(hcan);
    HAL_CAN_ActivateNotification(hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    if (hcan->Instance == CAN1) {
        CAN_RxHeaderTypeDef RxHeader;
        uint8_t temp_buf[8];

        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, temp_buf) == HAL_OK) {
            rxed_can_id = RxHeader.StdId;
            rxed_dlc    = RxHeader.DLC;
            for(uint8_t i = 0; i < rxed_dlc && i < 8; i++) {
                rxed_data[i] = temp_buf[i];
            }
            rx_counter++;

            // 1. Giải mã chuẩn GROUP D: CAN_ID_REPORT_FRONT_STATUS (0x400)[cite: 1]
            if (rxed_can_id == CAN_ID_REPORT_FRONT_STATUS && rxed_dlc >= 2) {
                uint8_t actuatorMask = rxed_data[0];

                monitored_wiper = (actuatorMask & FRONT_ACT_WIPER) ? 1 : 0;
                monitored_turn  = 0;
                if (actuatorMask & FRONT_ACT_LTURN) monitored_turn = 1;
                if (actuatorMask & FRONT_ACT_RTURN) monitored_turn = 2;
                if ((actuatorMask & FRONT_ACT_LTURN) && (actuatorMask & FRONT_ACT_RTURN)) monitored_turn = 3;
            }

            // 2. Giải mã chuẩn GROUP E: CAN_ID_FAULT_FRONT_BCM (0x500)[cite: 1]
            if (rxed_can_id == CAN_ID_FAULT_FRONT_BCM && rxed_dlc == 5) {
                dtc_severity      = rxed_data[0];
                dtc_code_received = PACK_U16(rxed_data[1], rxed_data[2]); //[cite: 1]
                dtc_counter       = rxed_data[3];
                simple_error_code = rxed_data[4];
            }

            // 3. Nháy LED PC13 báo hiệu nhận CAN thành công
            if (rx_counter % 2 == 0) {
                HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
            } else {
                HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
            }
        }
    }
}
