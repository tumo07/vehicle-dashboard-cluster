/*
 * TX_data_UART.h
 *
 *  Created on: Sep 20, 2026
 *      Author: MAC
 */

#ifndef INC_TX_DATA_UART_H_
#define INC_TX_DATA_UART_H_

#include "stm32f1xx_hal.h"

// Prototype hàm chuyển tiếp lệnh CAN xuống UART
void TX_UART_Forward_Commands(UART_HandleTypeDef *huart);

#endif /* INC_TX_DATA_UART_H_ */
