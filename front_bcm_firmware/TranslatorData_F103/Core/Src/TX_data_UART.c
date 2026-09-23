/*
 * TX_data_UART.c
 *
 *  Created on: Sep 20, 2026
 *      Author: MAC
 */

#include "TX_data_UART.h"
#include "RX_data_CAN.h" // Include để lấy các biến rx_cmd_... và cờ new_cmd_rx_flag


/**
 * @brief Đóng gói và gửi các lệnh điều khiển xuống Front BCM (F411) qua UART
 * @param huart: Con trỏ tới ngoại vi UART (ví dụ: &huart1)
 */
void TX_UART_Forward_Commands(UART_HandleTypeDef *huart) {

    // Kiểm tra xem có lệnh CAN nào mới vừa được bóc tách xong chưa
    if (new_cmd_rx_flag == 1) {
        new_cmd_rx_flag = 0; // Xóa cờ ngay để sẵn sàng đón lệnh tiếp theo từ ngắt CAN

        // Khởi tạo mảng UART chuẩn 5 byte: [Header] [Lệnh] [Data0] [Data1] [Checksum]
        uint8_t txUart[5] = {0xAA, 0x00, 0x00, 0x00, 0x00};

        // --- 1. GỬI LỆNH ĐÈN TRƯỚC (Lệnh 0x01) ---
        txUart[1] = 0x01;
        txUart[2] = rx_cmd_light_mask;
        txUart[3] = rx_cmd_light_brightness;
        txUart[4] = (uint8_t)(txUart[1] + txUart[2] + txUart[3]); // Tính Checksum
        HAL_UART_Transmit(huart, txUart, 5, 10);

        // --- 2. GỬI LỆNH GẠT MƯA (Lệnh 0x02) ---
        txUart[1] = 0x02;
        txUart[2] = rx_cmd_wiper_mode;
        txUart[3] = rx_cmd_wiper_washer;
        txUart[4] = (uint8_t)(txUart[1] + txUart[2] + txUart[3]);
        HAL_UART_Transmit(huart, txUart, 5, 10);

        // --- 3. GỬI LỆNH XI-NHAN (Lệnh 0x03) ---
        txUart[1] = 0x03;
        txUart[2] = rx_cmd_turn_arm;
        txUart[3] = 0x00; // Byte này chưa dùng cho xi-nhan nên để 0
        txUart[4] = (uint8_t)(txUart[1] + txUart[2] + txUart[3]);
        HAL_UART_Transmit(huart, txUart, 5, 10);

        // --- 4. GOI LENH BLINK TICK ---
        txUart[1] = 0x04;
        txUart[2] = rx_cmd_blink_tick;
        txUart[3] = 0x00;
        txUart[4] = (uint8_t)(txUart[1] + txUart[2] + txUart[3]);
        HAL_UART_Transmit(huart, txUart, 5, 10);
    }
}

