/*
 * data_can.h
 *
 *  Created on: Sep 12, 2026
 *      Author: MAC
 */

#ifndef INC_DATA_CAN_H_
#define INC_DATA_CAN_H_

#include "stm32f1xx_hal.h"

void CAN_Init_Filter_And_Start(CAN_HandleTypeDef *hcan);
void CAN_Send_Front_Command(CAN_HandleTypeDef *hcan, uint8_t wiperStatus, uint8_t turnStatus);

#endif /* INC_DATA_CAN_H_ */
