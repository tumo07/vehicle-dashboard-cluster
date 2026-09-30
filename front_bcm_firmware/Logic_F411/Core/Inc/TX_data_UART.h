#ifndef INC_TX_DATA_UART_H_
#define INC_TX_DATA_UART_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>

// ==============================================================================
// KHỞI TẠO & TASK ĐỊNH KỲ (GỘP TỪ FRONT_TX_TASK)
// ==============================================================================
void Bridge_Init(UART_HandleTypeDef *huart);
void Front_BCM_Periodic_TX_Task(void);

// ==============================================================================
// CÁC HÀM ĐÓNG GÓI VÀ BẮN GÓI TIN UART SANG BRIDGE NODE
// ==============================================================================
void Bridge_SendFault(uint8_t severity, uint16_t dtc_code, uint8_t counter, uint8_t simple_err);
void Bridge_SendWiperTurnStatus(uint8_t wiperMode, uint8_t turnMode);
void Bridge_SendTurnRequest(uint8_t turnReq);
void Bridge_SendStatusReport(uint8_t actuatorFlags, uint8_t wiperMode, uint8_t motorHealth);
void Bridge_SendSensorReport(uint8_t rainPercent, uint8_t waterPercent);
void Bridge_SendHeartbeat(uint8_t counter, uint8_t hbFlags);


extern uint8_t currentCmdWiper;
extern uint8_t currentCmdTurn;

#endif /* INC_TX_DATA_UART_H_ */
