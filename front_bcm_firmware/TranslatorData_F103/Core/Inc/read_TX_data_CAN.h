/*
 * read_TX_data_CAN.h
 * Logger module: In dữ liệu CAN TX ra Serial Console qua USART2
 */

#ifndef INC_READ_TX_DATA_CAN_H_
#define INC_READ_TX_DATA_CAN_H_

#include "stm32f1xx_hal.h"
#include <stdint.h>

// Khởi tạo cổng UART dùng để in Log (truyền &huart2)
void CAN_Logger_Init(UART_HandleTypeDef *huart_debug);

// Hàm in dữ liệu frame CAN ra Console theo định dạng trực quan
void CAN_Logger_Print_Frame(uint16_t stdId, uint8_t dlc, const uint8_t *pData, HAL_StatusTypeDef status);

#endif /* INC_READ_TX_DATA_CAN_H_ */
