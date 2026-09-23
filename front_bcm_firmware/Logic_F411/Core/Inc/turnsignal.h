/*
 * turnsignal.h
 *
 *  Created on: Sep 2, 2026
 *      Author: MAC
 */

#ifndef INC_TURNSIGNAL_H_
#define INC_TURNSIGNAL_H_

#include "main.h"
#include "can_messages.h"


void TurnSignal_Init(void);
void TurnSignal_Task(void);

CmdTurn_t TurnSignal_GetMode(void);


#endif /* INC_TURNSIGNAL_H_ */
