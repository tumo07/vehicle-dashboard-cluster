/*
 * TX_data_CAN.h
 * Module điều khiển truyền CAN trên F103
 */

#ifndef INC_TX_DATA_CAN_H_
#define INC_TX_DATA_CAN_H_

#include "stm32f1xx_hal.h"
#include "can_messages.h"

void CAN_Init_Filter_And_Start(CAN_HandleTypeDef *hcan);
HAL_StatusTypeDef CAN_Transmit_Direct(CAN_HandleTypeDef *hcan, uint16_t stdId, uint8_t dlc, const uint8_t *pData);

#endif /* INC_TX_DATA_CAN_H_ */
