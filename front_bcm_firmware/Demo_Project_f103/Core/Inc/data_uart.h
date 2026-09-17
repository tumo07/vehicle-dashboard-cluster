/*
 * data_uart.h
 *
 *  Created on: Sep 12, 2026
 *      Author: MAC
 */

#ifndef INC_DATA_UART_H_
#define INC_DATA_UART_H_

#include "stm32f1xx_hal.h"
#include <stdint.h>

void F103_Bridge_Init(UART_HandleTypeDef *huart);

extern volatile uint8_t f103_wiperStatus;
extern volatile uint8_t f103_turnStatus;

#endif /* INC_DATA_UART_H_ */
