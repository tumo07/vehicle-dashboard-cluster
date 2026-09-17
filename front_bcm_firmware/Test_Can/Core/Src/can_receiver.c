#include "can_receiver.h"
#include "main.h"

volatile uint32_t rxed_can_id = 0;
volatile uint8_t  rxed_dlc = 0;
volatile uint8_t  rxed_data[8] = {0};
volatile uint32_t rx_counter = 0;
volatile uint8_t monitored_wiper = 0;
volatile uint8_t monitored_turn  = 0;
volatile uint8_t error_code_received = 0;
volatile uint8_t error_device_mask = 0;

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

        // Lấy dữ liệu từ FIFO0
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, temp_buf) == HAL_OK) {

            // 1. Lưu trữ thông tin gói tin chung
            rxed_can_id = RxHeader.StdId;
            rxed_dlc    = RxHeader.DLC;
            for(uint8_t i = 0; i < rxed_dlc && i < 8; i++) {
                rxed_data[i] = temp_buf[i];
            }
            rx_counter++;

            // 2. Giải mã chuẩn ID 0x200 (Front Status) do Blue Pill #1 gửi lên
            if (rxed_can_id == CAN_ID_FRONT_STATUS && rxed_dlc == 2) {
                uint8_t actuatorMask = rxed_data[0];

                // Tách trạng thái gạt mưa và xi-nhan
                monitored_wiper = (actuatorMask & CMD_FRONT_WIPER) ? 1 : 0;
                monitored_turn  = 0;
                if (actuatorMask & CMD_FRONT_L_TURN) monitored_turn = 1;
                if (actuatorMask & CMD_FRONT_R_TURN) monitored_turn = 2;
                if ((actuatorMask & CMD_FRONT_L_TURN) && (actuatorMask & CMD_FRONT_R_TURN)) monitored_turn = 3;
            }

            // 3. Giải mã chuẩn ID 0x2FF (Front Error) khi F411 giữ nút báo lỗi
            if (rxed_can_id == CAN_ID_FRONT_ERROR && rxed_dlc == 2) {
                error_code_received = rxed_data[0]; // Nhận mã lỗi (Ví dụ: 0x01)
                error_device_mask   = rxed_data[1]; // Nhận mã thiết bị (Ví dụ: 0x08)
            }

            // 4. Nháy LED PC13 báo hiệu nhận dữ liệu CAN thành công
            if (rx_counter % 2 == 0) {
                HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); // Sáng
            } else {
                HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   // Tắt
            }
        }
    }
}
