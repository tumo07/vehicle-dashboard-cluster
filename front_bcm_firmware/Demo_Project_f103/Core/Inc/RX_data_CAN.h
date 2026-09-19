/*
 * RX_data.h
 *
 *  Created on: Sep 19, 2026
 *      Author: MAC
 */

#ifndef INC_RX_DATA_H_
#define INC_RX_DATA_H_

#include "stm32f1xx_hal.h"
#include <stdint.h>

// ==============================================================================
// CÁC BIẾN EXTERN ĐỂ MAIN.C CÓ THỂ ĐỌC ĐƯỢC LỆNH TỪ CENTRAL ECU
// ==============================================================================

// 1. Nhóm Đèn (ID: 0x200)
extern volatile uint8_t rx_cmd_light_mask;
extern volatile uint8_t rx_cmd_light_brightness;

// 2. Nhóm Gạt mưa (ID: 0x201)
extern volatile uint8_t rx_cmd_wiper_mode;
extern volatile uint8_t rx_cmd_wiper_washer;

// 3. Nhóm Xi-nhan (ID: 0x202)
extern volatile uint8_t rx_cmd_turn_arm;

// 4. Nhịp Blink Tick (ID: 0x130)
extern volatile uint8_t rx_cmd_blink_tick;

// ==============================================================================
// CỜ BÁO HIỆU (CẬP NHẬT TRẠNG THÁI)
// ==============================================================================

// Cờ báo hiệu = 1 khi có bất kỳ lệnh điều khiển CAN mới nào vừa nhận được
extern volatile uint8_t new_cmd_rx_flag;

// ==============================================================================
// PROTOTYPE HÀM BÓC TÁCH DỮ LIỆU
// ==============================================================================

void RX_Data_Parse_Command(uint32_t stdId, uint8_t *data, uint8_t dlc);

#endif /* INC_RX_DATA_H_ */
