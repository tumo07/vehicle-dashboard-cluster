/*
 * can_gateway.h
 *
 *  Created on: Sep 21, 2026
 *      Author: admin
 */

#ifndef CAN_GATEWAY_H
#define CAN_GATEWAY_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

HAL_StatusTypeDef CAN_Gateway_Init(void);

HAL_StatusTypeDef CAN_Gateway_SendCommand(
    uint16_t std_id,
    const uint8_t *data,
    uint8_t dlc
);

void CAN_Gateway_Process(void);

void CAN_Gateway_RxCallback(
    CAN_HandleTypeDef *phcan
);

void CAN_Gateway_ErrorCallback(
    CAN_HandleTypeDef *phcan
);

#ifdef __cplusplus
}
#endif

#endif /* _CAN_GATEWAY_H_ */
