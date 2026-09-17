/*
 * can_receiver.h
 *
 *  Created on: Sep 14, 2026
 *      Author: MAC
 */

#ifndef INC_CAN_RECEIVER_H_
#define INC_CAN_RECEIVER_H_

#include "stm32f1xx_hal.h"
#include <stdint.h>

void CAN_Receiver_Init(CAN_HandleTypeDef *hcan);

extern volatile uint32_t rxed_can_id;
extern volatile uint8_t  rxed_dlc;
extern volatile uint8_t  rxed_data[8];
extern volatile uint32_t rx_counter;

#endif /* INC_CAN_RECEIVER_H_ */
