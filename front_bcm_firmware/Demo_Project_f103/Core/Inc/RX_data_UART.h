/*
 * RX_data_UART.h
 *
 *  Created on: Sep 12, 2026
 *      Author: MAC
 */

#ifndef INC_RX_DATA_UART_H_
#define INC_RX_DATA_UART_H_

#include "stm32f1xx_hal.h"
#include <stdint.h>

// ==============================================================================
// CÁC BIẾN EXTERN LƯU TRẠNG THÁI (Đọc từ F411 gửi lên)
// ==============================================================================
extern volatile uint8_t f103_wiperStatus;
extern volatile uint8_t f103_turnStatus;
extern volatile HAL_StatusTypeDef dbg_tx_status;

// ==============================================================================
// CÁC BIẾN EXTERN PHỤC VỤ DEBUG (Theo dõi bộ đệm UART)
// ==============================================================================
extern volatile uint8_t  dbg_bridge_rx_byte;
extern volatile uint32_t dbg_bridge_rx_count;
extern volatile uint8_t  dbg_bridge_packet_type;
extern volatile uint8_t  dbg_slide_buf[6];

// ==============================================================================
// PROTOTYPE HÀM KHỞI TẠO VÀ XỬ LÝ
// ==============================================================================

/**
 * @brief  Khởi tạo module nhận UART (Kích hoạt ngắt nhận byte đầu tiên)
 * @param  huart: Con trỏ trỏ tới bộ UART (ví dụ: &huart1)
 */
void F103_Bridge_Init(UART_HandleTypeDef *huart);

#endif /* INC_RX_DATA_UART_H_ */
