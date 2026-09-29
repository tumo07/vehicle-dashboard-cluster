#ifndef __HEADLIGHT_H
#define __HEADLIGHT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "can_messages.h"

// =============================================================================
// PHÂN BỔ NGOẠI VI & CHÂN ĐÈN
// =============================================================================
#define HL_TIM_CHANNEL_COS       TIM_CHANNEL_2       // PB7 (TIM4_CH2) - Cos
#define HL_TIM_CHANNEL_PHA       TIM_CHANNEL_3       // PB8 (TIM4_CH3) - Pha
#define HL_PWM_MAX_PERIOD        1000U

#define HL_DIMMER_ADC_CHANNEL    ADC_CHANNEL_11      // PC1 - Biến trở Dimmer

// Đèn Fog chỉ dùng duy nhất chân PE12, không can thiệp PD15 (LD6)
#define HL_FOG_PIN               LED_FogLamp_Pin     // PE12
#define HL_FOG_PORT              LED_FogLamp_GPIO_Port

void    Headlight_Init(TIM_HandleTypeDef *htim, ADC_HandleTypeDef *hadc);
void    Headlight_Task(void);
uint8_t Headlight_Get_Actuator_Flags(void);
uint8_t Headlight_Get_Current_Brightness(void);

#ifdef __cplusplus
}
#endif

#endif /* __HEADLIGHT_H */
