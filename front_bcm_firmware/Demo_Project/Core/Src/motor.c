/*
 * motor.c
 *
 *  Created on: Sep 10, 2026
 *      Author: MAC
 */

#include "motor.h"
#include "stdint.h"

extern TIM_HandleTypeDef htim3;

static uint32_t lastCycleTime = 0;

static void Motor_SetPWM(uint16_t speed) {
    if (speed > 1000) speed = 1000;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, speed);
}

void Motor_Wash_Init(void) {

    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);


    Motor_SetPWM(0);
    HAL_GPIO_WritePin(GPIOD, Signal_Left_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOD, Signal_Right_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOE, LED_Moto_Pin, GPIO_PIN_RESET);
}

void Motor_Wash_Task(uint8_t isWashActive) {
    uint32_t currentTick = HAL_GetTick();

    if (isWashActive) {

        if (currentTick - lastCycleTime >= 3000) {
            lastCycleTime = currentTick;
        }

        uint32_t elapsed = currentTick - lastCycleTime;


        if (elapsed < 1000) {
            HAL_GPIO_WritePin(GPIOD, Signal_Left_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOD, Signal_Right_Pin, GPIO_PIN_RESET);
            Motor_SetPWM(800);
            HAL_GPIO_WritePin(GPIOE, LED_Moto_Pin, GPIO_PIN_SET);
        }

        else {
            Motor_SetPWM(0);
            HAL_GPIO_WritePin(GPIOD, Signal_Left_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOD, Signal_Right_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOE, LED_Moto_Pin, GPIO_PIN_RESET);
        }
    } else {

        lastCycleTime = currentTick;
        Motor_SetPWM(0);
        HAL_GPIO_WritePin(GPIOD, Signal_Left_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOD, Signal_Right_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOE, LED_Moto_Pin, GPIO_PIN_RESET);
    }
}
