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

extern volatile uint8_t f103_wiperStatus;
extern volatile uint8_t f103_turnStatus;

void F103_Bridge_Init(UART_HandleTypeDef *huart);

#endif /* INC_RX_DATA_UART_H_ */
