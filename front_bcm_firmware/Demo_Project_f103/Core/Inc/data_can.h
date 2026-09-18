/*
 * data_can.h
 *
 *  Created on: Sep 12, 2026
 *      Author: MAC
 */

#ifndef INC_DATA_CAN_H_
#define INC_DATA_CAN_H_

#include "stm32f1xx_hal.h"
#include "can_messages.h"


// 2. SAU ĐÓ MỚI ĐẾN KHAI BÁO BIẾN EXTERN (Lúc này trình biên dịch đã biết NodeState_t là gì)
extern volatile AckStatus_t ack_status;
extern volatile uint32_t last_req_tick;


void CAN_Init_Filter_And_Start(CAN_HandleTypeDef *hcan);
void CAN_Send_Front_Command(CAN_HandleTypeDef *hcan, uint8_t wiperStatus, uint8_t turnStatus);
void CAN_Send_Front_Error(CAN_HandleTypeDef *hcan, uint8_t errorCode, uint8_t errorMask);
void CAN_Send_Join_Request(CAN_HandleTypeDef *hcan);

#endif /* INC_DATA_CAN_H_ */
