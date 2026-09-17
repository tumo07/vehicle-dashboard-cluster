/*
 * turnsignal.h
 *
 *  Created on: Sep 2, 2026
 *      Author: MAC
 */

#ifndef INC_TURNSIGNAL_H_
#define INC_TURNSIGNAL_H_

#include "main.h"

typedef enum {
	Off = 0	,
	Left	,
	Right	,
	Hazard	,
} SignalMode_t ;


void TurnSignal_Init(void);
void TurnSignal_Task(void);

SignalMode_t TurnSignal_GetMode(void);


#endif /* INC_TURNSIGNAL_H_ */
