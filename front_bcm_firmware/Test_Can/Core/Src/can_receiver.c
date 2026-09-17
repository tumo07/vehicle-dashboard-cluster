#include "can_receiver.h"
#include "main.h"

volatile uint32_t rxed_can_id = 0;
volatile uint8_t  rxed_dlc = 0;
volatile uint8_t  rxed_data[8] = {0};
volatile uint32_t rx_counter = 0;

void CAN_Receiver_Init(CAN_HandleTypeDef *hcan) {
    CAN_FilterTypeDef sFilterConfig;
    sFilterConfig.FilterBank = 0;
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;

    // Mask = 0 -> Hốt toàn bộ ID trên bus để debug
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
            for(uint8_t i=0; i<rxed_dlc && i<8; i++) {
                rxed_data[i] = temp_buf[i];
            }
            rx_counter++;
            if (rx_counter % 2 == 0) {
					HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); // Sáng
				} else {
					HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   // Tắt
				}
        }
    }
}
