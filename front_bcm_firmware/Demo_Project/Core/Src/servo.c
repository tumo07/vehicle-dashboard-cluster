/*
 * servo.c
 *
 *  Created on: Sep 10, 2026
 *      Author: MAC
 */

#include "servo.h"
#include "motor.h"
#include "data_uart.h"
#include "turnsignal.h"


extern TIM_HandleTypeDef htim2;
extern ADC_HandleTypeDef hadc1;
extern void Bridge_SendStatus(uint8_t wiperMode, uint8_t turnMode);
extern SignalMode_t TurnSignal_GetMode(void);


static mCrtlWiper_t mWiper = {
		.currentWiper = Wiper_Off,
		.tagetWiper = Wiper_Off,
		.lastDebounceTimeWiper = 0,
		.lastBtnWiperState = GPIO_PIN_SET,
		.wiperTime = 0,
		.cachedRainValue = 0,
		.lastDelayTime = 0,
};

static uint16_t Read_ISR(void){
    HAL_ADC_Start(&hadc1);
    if(HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
        uint16_t val = HAL_ADC_GetValue(&hadc1);
        HAL_ADC_Stop(&hadc1);
        return val;
    }
    HAL_ADC_Stop(&hadc1);
    return 0;
}

static void Servo_SetAngle(uint16_t pulseWidth){
	__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, pulseWidth);
}

static void Wiper_UpdateOutputs(GPIO_PinState ld3, GPIO_PinState ld4, GPIO_PinState ld5, GPIO_PinState ld6) {
	HAL_GPIO_WritePin(GPIOD, LD3_Pin, ld3);
	HAL_GPIO_WritePin(GPIOD, LD4_Pin, ld4);
	HAL_GPIO_WritePin(GPIOD, LD5_Pin, ld5);
	HAL_GPIO_WritePin(GPIOD, LD6_Pin, ld6);
}

void Servo_Init(void) {
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
}

WiperMode_t Servo_GetWiperMode(void) {
    return mWiper.currentWiper;
}

void Wiper_Task(void){

	static uint8_t lastCmdWiper = 0;
	    if (currentCmdWiper != lastCmdWiper) {
	        lastCmdWiper = currentCmdWiper;
	        if (currentCmdWiper <= Wiper_Auto) {
	            mWiper.tagetWiper = (WiperMode_t)currentCmdWiper;
	            mWiper.currentWiper = mWiper.tagetWiper;
	            mWiper.wiperTime = HAL_GetTick();
	        }
	    }

	uint8_t currentBtnWiperState = HAL_GPIO_ReadPin(GPIOE, BTN_WiperMode_Pin);

	if (currentBtnWiperState == GPIO_PIN_RESET && mWiper.lastBtnWiperState == GPIO_PIN_SET) {
		if (HAL_GetTick() - mWiper.lastDebounceTimeWiper >= 200) {
			mWiper.tagetWiper++;
			if (mWiper.tagetWiper > Wiper_Auto) {
				mWiper.tagetWiper = Wiper_Off;
			}
			mWiper.currentWiper = mWiper.tagetWiper;
			mWiper.wiperTime = HAL_GetTick();
			mWiper.lastDebounceTimeWiper = HAL_GetTick();
			Bridge_SendStatus((uint8_t)mWiper.currentWiper, (uint8_t)TurnSignal_GetMode());
		}
	}
	mWiper.lastBtnWiperState = currentBtnWiperState;
	mWiper.currentWiper = mWiper.tagetWiper;

	if (HAL_GetTick() - mWiper.lastDelayTime >= 3000) {
		mWiper.cachedRainValue = Read_ISR();
		mWiper.lastDelayTime = HAL_GetTick();
	}

	switch (mWiper.currentWiper){
		case Wiper_Off:
			Servo_SetAngle(1000);
			Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET);
			Motor_Wash_Task(0); // Tắt bơm nước
			break;

		case Wiper_Int:
			Motor_Wash_Task(1); // Bật chu kỳ bơm nước (3 giây xịt 1 lần)
			if(HAL_GetTick() - mWiper.wiperTime < 1000 ){
				Servo_SetAngle(2000);
				Wiper_UpdateOutputs(GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET);
			}
			else if(HAL_GetTick() - mWiper.wiperTime < 4000 ){
				Servo_SetAngle(1000);
				Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET);
			}
			else {
				mWiper.wiperTime = HAL_GetTick();
			}
			break;

		case Wiper_Low:
			Motor_Wash_Task(0); // Tắt bơm nước
			if (HAL_GetTick() - mWiper.wiperTime < 1000) {
				Servo_SetAngle(2000);
				Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_RESET); // (giữ nguyên logic LED)
			}
			else if (HAL_GetTick() - mWiper.wiperTime < 2500) {
				Servo_SetAngle(1000);
				Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_RESET);
			}
			else {
				mWiper.wiperTime = HAL_GetTick();
			}
			break;

		case Wiper_Hi:
			Motor_Wash_Task(0); // Tắt bơm nước
			if (HAL_GetTick() - mWiper.wiperTime < 300) {
				Servo_SetAngle(2000);
				Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_RESET);
			}
			else if (HAL_GetTick() - mWiper.wiperTime < 700) {
				Servo_SetAngle(1000);
				Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_RESET);
			}
			else {
				mWiper.wiperTime = HAL_GetTick();
			}
			break;

		case Wiper_Auto:
				{
					uint16_t rainValue = mWiper.cachedRainValue;
					uint8_t currentAutoSubState = 4; // Mặc định là 4 (Auto chờ)

					if (rainValue > 3100){
						Servo_SetAngle(1000);
						Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET);
						Motor_Wash_Task(0);
						currentAutoSubState = 4; // Auto chờ (Khô ráo)
					}
					else if(rainValue > 2500){
						Motor_Wash_Task(1); // Bật bơm nước định kỳ
						if(HAL_GetTick() - mWiper.wiperTime < 1000 ){
							Servo_SetAngle(1500);
							Wiper_UpdateOutputs(GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET);
						}
						else if(HAL_GetTick() - mWiper.wiperTime < 4000 ){
							Servo_SetAngle(1000);
							Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET);
						}
						else {
							mWiper.wiperTime = HAL_GetTick();
						}
						currentAutoSubState = 5; // Auto Int (Mưa nhẹ)
					}
					else if(rainValue > 2000 ){
						Motor_Wash_Task(0);
						if (HAL_GetTick() - mWiper.wiperTime < 1000) {
							Servo_SetAngle(2000);
							Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_SET);
						}
						else if (HAL_GetTick() - mWiper.wiperTime < 2500) {
							Servo_SetAngle(1000);
							Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_SET);
						}
						else {
							mWiper.wiperTime = HAL_GetTick();
						}
						currentAutoSubState = 6; // Auto Low (Mưa vừa)
					}
					else {
						Motor_Wash_Task(0);
						if (HAL_GetTick() - mWiper.wiperTime < 300) {
							Servo_SetAngle(2000);
							Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_SET);
						}
						else if (HAL_GetTick() - mWiper.wiperTime < 700) {
							Servo_SetAngle(1000);
							Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_SET);
						}
						else {
							mWiper.wiperTime = HAL_GetTick();
						}
						currentAutoSubState = 7; // Auto High (Mưa lớn)
					}

					// TỰ ĐỘNG BẮN DỮ LIỆU KHI CẤP ĐỘ THAY ĐỔI NGẦM (KHÔNG CẦN BẤM NÚT)
								static uint8_t lastSentAutoState = 0xFF;
								if (currentAutoSubState != lastSentAutoState) {
									lastSentAutoState = currentAutoSubState;
									Bridge_SendStatus(currentAutoSubState, (uint8_t)TurnSignal_GetMode());
								}
					break;
				}

		default:
			Servo_SetAngle(1000);
			Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET);
			Motor_Wash_Task(0);
			break;
	}
}
