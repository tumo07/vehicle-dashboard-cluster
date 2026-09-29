/*
 * uart_protocol.h
 *
 *  Created on: Sep 21, 2026
 *      Author: admin
 */

#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UART_RX_BUFFER_SIZE          48U
#define UART_INTERBYTE_TIMEOUT_MS    100U

HAL_StatusTypeDef UART_Protocol_Init(void);

void UART_Protocol_Process(void);

void UART_Protocol_SendString(
    const char *text
);

void UART_Protocol_SendCanFrame(
    uint16_t std_id,
    uint8_t dlc,
    const uint8_t *data
);

void UART_Protocol_RxCallback(
    UART_HandleTypeDef *huart
);

#ifdef __cplusplus
}
#endif

#endif /* UART_PROTOCOL_H */
