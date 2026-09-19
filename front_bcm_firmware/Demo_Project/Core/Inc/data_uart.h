#ifndef INC_DATA_UART_H_
#define INC_DATA_UART_H_

#include "stm32f4xx_hal.h"

void Bridge_Init(UART_HandleTypeDef *huart);
void Bridge_SendFault(uint8_t severity, uint16_t dtc_code, uint8_t counter, uint8_t simple_err);
void Bridge_SendWiperTurnStatus(uint8_t wiperMode, uint8_t turnMode);
void Bridge_SendStatusReport(uint8_t actuatorFlags, uint8_t wiperMode, uint8_t motorHealth);
void Bridge_SendSensorReport(uint8_t rainPercent, uint8_t waterPercent);
void Bridge_SendHeartbeat(uint8_t counter, uint8_t hbFlags);
extern uint8_t currentCmdWiper;
extern uint8_t currentCmdTurn;

#endif /* INC_DATA_UART_H_ */
