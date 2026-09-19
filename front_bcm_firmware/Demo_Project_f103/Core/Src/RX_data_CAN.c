/*
 * RX_data.c
 *
 *  Created on: Sep 19, 2026
 *      Author: MAC
 */

#include <RX_data_CAN.h>
#include <TX_data_CAN.h>       // Include để nhận diện biến ack_status và kiểu AckStatus_t
#include "can_messages.h"   // Include thư viện chứa các Macro ID (nếu có)

// --- Lấy biến trạng thái kết nối từ file data_can.c sang ---
extern volatile AckStatus_t ack_status;

// --- Khởi tạo giá trị mặc định cho các biến lưu trữ lệnh ---
volatile uint8_t rx_cmd_light_mask = 0;
volatile uint8_t rx_cmd_light_brightness = 0;

volatile uint8_t rx_cmd_wiper_mode = 0;
volatile uint8_t rx_cmd_wiper_washer = 0;

volatile uint8_t rx_cmd_turn_arm = 0;

volatile uint8_t rx_cmd_blink_tick = 0;

// Cờ báo hiệu có dữ liệu điều khiển mới
volatile uint8_t new_cmd_rx_flag = 0;

/**
 * @brief  Hàm bóc tách và phân loại các gói tin lệnh điều khiển
 * @param  stdId: Mã ID của gói tin CAN
 * @param  data:  Con trỏ mảng dữ liệu 8 byte
 * @param  dlc:   Độ dài dữ liệu
 */
void RX_Data_Parse_Command(uint32_t stdId, uint8_t *data, uint8_t dlc)
{
    // 1. Lọc lệnh điều khiển ĐÈN TRƯỚC (ID: 0x200)
    if (stdId == 0x200U && dlc >= 2) {
        rx_cmd_light_mask = data[0];
        rx_cmd_light_brightness = data[1];
        new_cmd_rx_flag = 1;
    }
    // 2. Lọc lệnh điều khiển GẠT MƯA (ID: 0x201)
    else if (stdId == 0x201U && dlc >= 2) {
        rx_cmd_wiper_mode = data[0];
        rx_cmd_wiper_washer = data[1];
        new_cmd_rx_flag = 1;
    }
    // 3. Lọc lệnh điều khiển XI-NHAN (ID: 0x202)
    else if (stdId == 0x202U && dlc >= 1) {
        rx_cmd_turn_arm = data[0];
        new_cmd_rx_flag = 1;
    }
    // 4. Lọc lệnh nhịp đồng hồ nháy XI-NHAN (ID: 0x130)
    else if (stdId == 0x130U && dlc >= 1) {
        rx_cmd_blink_tick = data[0];
        new_cmd_rx_flag = 1;
    }
}

/**
 * @brief  Hàm ngắt nhận CAN - Được gọi tự động bởi phần cứng khi có gói tin tới FIFO0
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    if (hcan->Instance == CAN1) {
        CAN_RxHeaderTypeDef RxHeader;
        uint8_t rxData[8];

        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, rxData) == HAL_OK) {

            // --- ƯU TIÊN 1: XỬ LÝ NHẬN TÍN HIỆU ACK KHI ĐANG CHỜ KHỞI TẠO ---
            // Nếu Bridge chưa được Central ECU duyệt gia nhập mạng
            if (ack_status == ACK_STATUS_REJECTED) {
                // Kiểm tra xem có phải gói ACK (0x101) và payload = 0x01 không
                if (RxHeader.StdId == 0x101U && rxData[0] == 0x01U) {
                    ack_status = ACK_STATUS_PENDING; // Được ECU duyệt, chuyển trạng thái sẵn sàng!
                }
                return; // Thoát luôn, chưa xử lý các lệnh điều khiển khi chưa vào mạng
            }

            // --- ƯU TIÊN 2: KHI ĐÃ ĐƯỢC DUYỆT, BÓC TÁCH LỆNH ĐIỀU KHIỂN XUỐNG CÁC BIẾN ---
            RX_Data_Parse_Command(RxHeader.StdId, rxData, RxHeader.DLC);
        }
    }
}
