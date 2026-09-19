/*
 * front_tx_task.h
 * Front BCM - Periodic TX Tasks Module
 */

#ifndef INC_FRONT_TX_TASK_H_
#define INC_FRONT_TX_TASK_H_

#include "stm32f4xx_hal.h"

// Khởi tạo các biến/thông số cần thiết cho TX Task nếu có
void Front_TX_Task_Init(void);

// Hàm task chính chạy định kỳ trong while(1) của main.c
void Front_BCM_Periodic_TX_Task(void);

#endif /* INC_FRONT_TX_TASK_H_ */
