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
#include "can_messages.h"

void CAN_Receiver_Init(CAN_HandleTypeDef *hcan);

extern volatile uint32_t rxed_can_id;
extern volatile uint8_t  rxed_dlc;
extern volatile uint8_t  rxed_data[8];
extern volatile uint32_t rx_counter;
extern volatile uint8_t monitored_wiper;
extern volatile uint8_t monitored_turn;
extern volatile uint8_t error_code_received;
extern volatile uint8_t error_device_mask;

#endif /* INC_CAN_RECEIVER_H_ */
