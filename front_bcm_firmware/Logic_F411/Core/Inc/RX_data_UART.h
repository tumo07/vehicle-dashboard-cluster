/*
 * RX_data_UART.h
 * Front BCM (STM32F411) - Header
 */

#ifndef INC_RX_DATA_UART_H_
#define INC_RX_DATA_UART_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include "can_messages.h"

// ==============================================================================
// CÁC BIẾN LƯU LỆNH TỪ CENTRAL ECU (DO BRIDGE CHUYỂN TIẾP XUỐNG)
// ==============================================================================
extern volatile uint8_t    f411_cmd_light_mask;
extern volatile uint8_t    f411_cmd_light_brightness;
extern volatile CmdWiper_t f411_cmd_wiper_mode;
extern volatile uint8_t    f411_cmd_wiper_washer;
extern volatile CmdTurn_t  f411_cmd_turn_arm;
extern volatile uint8_t    f411_cmd_blink_tick;
extern volatile uint8_t    f411_new_cmd_flag;

// ==============================================================================
// PROTOTYPE HÀM KHỞI TẠO VÀ XỬ LÝ
// ==============================================================================
void Front_UART_RX_Init(UART_HandleTypeDef *huart);
void Front_UART_Parse_Packet(uint8_t *packet);

#endif /* INC_RX_DATA_UART_H_ */
