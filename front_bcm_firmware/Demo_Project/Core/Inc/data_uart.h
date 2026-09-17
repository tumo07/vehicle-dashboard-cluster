#ifndef INC_DATA_UART_H_
#define INC_DATA_UART_H_

#include "stm32f4xx_hal.h"

void Bridge_Init(UART_HandleTypeDef *huart);
void Bridge_SendStatus(uint8_t wiperMode, uint8_t turnMode);
extern uint8_t currentCmdWiper;
extern uint8_t currentCmdTurn;

#endif /* INC_DATA_UART_H_ */
