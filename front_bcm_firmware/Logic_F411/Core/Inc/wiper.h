#ifndef __WIPER_H
#define __WIPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "can_messages.h"

typedef struct {
    CmdWiper_t currentWiper;
    CmdWiper_t tagetWiper;
    uint32_t   lastDebounceTimeWiper;
    uint8_t    lastBtnWiperState;
    uint32_t   wiperTime;
    uint16_t   cachedRainValue;
    uint32_t   lastDelayTime;
} mCrtlWiper_t;

void       Wiper_System_Init(void);
void       Wiper_Task(void);
CmdWiper_t Servo_GetWiperMode(void);

uint8_t    Get_RainSensor_Percent(void);
uint8_t    Get_WaterLevel_Status(void);
uint8_t    Get_WaterLevel_Percent(void);

#ifdef __cplusplus
}
#endif

#endif /* __WIPER_H */
