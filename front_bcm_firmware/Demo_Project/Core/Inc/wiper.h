/*
 * wiper.h
 *
 *  Created on: Sep 19, 2026
 *      Author: MAC
 */

#ifndef INC_WIPER_H_
#define INC_WIPER_H_

#include "main.h"
#include "can_messages.h"

typedef struct {
	CmdWiper_t currentWiper;
	CmdWiper_t tagetWiper;
	uint32_t lastDebounceTimeWiper;
	uint32_t wiperTime;
	uint32_t lastDelayTime;
	uint32_t cachedRainValue;
	uint8_t lastBtnWiperState;
} mCrtlWiper_t;

// Khởi tạo chung cho Servo gạt mưa và Motor bơm nước
void Wiper_System_Init(void);

// Task chính xử lý logic gạt mưa, đọc cảm biến mưa và điều khiển motor/servo
void Wiper_Task(void);

// Lấy chế độ gạt mưa hiện tại
CmdWiper_t Servo_GetWiperMode(void);

// Điều khiển tốc độ Motor bơm nước (0 - 1000)
void Motor_SetPWM(uint16_t speed);

// Task điều khiển chu kỳ bơm nước (phun rửa kính)
void Motor_Wash_Task(uint8_t isWashActive);

#endif /* INC_WIPER_H_ */
