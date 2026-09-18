/*
 * servo.h
 *
 *  Created on: Sep 10, 2026
 *      Author: MAC
 */

#ifndef INC_SERVO_H_
#define INC_SERVO_H_

#include "main.h"
#include "can_messages.h"

//typedef enum{
//	Wiper_Off = 0,
//	Wiper_Int,
//	Wiper_Low,
//	Wiper_Hi,
//	Wiper_Auto,
//
//} WiperMode_t;

typedef struct {
	CmdWiper_t currentWiper;
	CmdWiper_t tagetWiper;
	uint32_t lastDebounceTimeWiper;
	uint32_t wiperTime;
	uint32_t lastDelayTime;
	uint32_t cachedRainValue;
	uint8_t lastBtnWiperState;
} mCrtlWiper_t;

void Servo_Init(void);

void Wiper_Task(void);



#endif /* INC_SERVO_H_ */
