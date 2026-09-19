/*
 * wiper.c
 * Front BCM - Wiper, Washer & Sensor Management Module
 * Compliant with Central ECU Coordinator Architecture
 */

#include <TX_data_UART.h>
#include "wiper.h"
#include "turnsignal.h"

extern TIM_HandleTypeDef htim2; // Cho Servo gạt mưa
extern TIM_HandleTypeDef htim3; // Cho Motor bơm nước
extern ADC_HandleTypeDef hadc1; // Cho Cảm biến mưa (ADC Channel)

extern void Bridge_SendWiperTurnStatus(uint8_t wiperMode, uint8_t turnMode);
extern CmdTurn_t TurnSignal_GetMode(void);

static uint32_t lastCycleTime = 0;

static mCrtlWiper_t mWiper = {
    .currentWiper          = CMD_WIPER_OFF,
    .tagetWiper            = CMD_WIPER_OFF,
    .lastDebounceTimeWiper = 0,
    .lastBtnWiperState     = GPIO_PIN_SET,
    .wiperTime             = 0,
    .cachedRainValue       = 0,
    .lastDelayTime         = 0,
};

// =============================================================================
// 1. SENSOR READING TASKS (Workflow Step 1: Thu thập tín hiệu phần cứng)
// =============================================================================

// Đọc giá trị cảm biến mưa thô (ADC 12-bit: 0 - 4095)
static uint16_t Read_RainSensor_Raw(void) {
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
        uint16_t val = HAL_ADC_GetValue(&hadc1);
        HAL_ADC_Stop(&hadc1);
        return val;
    }
    HAL_ADC_Stop(&hadc1);
    return 0;
}

// Chuyển đổi giá trị cảm biến mưa ra phần trăm (0 - 100%) để đóng gói vào CAN ID 0x401 (Byte 0)
uint8_t Get_RainSensor_Percent(void) {
    uint16_t rawRain = mWiper.cachedRainValue;

    // Ngưỡng 1: Nếu giá trị ADC >= 3100 -> Hoàn toàn khô ráo (0%)
    if (rawRain >= 3100) {
        return 0;
    }
    // Ngưỡng 2: Nếu giá trị ADC <= 2000 -> Mưa rất lớn / ngập cảm biến (100%)
    if (rawRain <= 2000) {
        return 100;
    }
    // Khoảng giữa: Quy đổi tuyến tính từ 2000 (100%) đến 3100 (0%)
    // Công thức: ((3100 - rawRain) * 100) / (3100 - 2000)
    return (uint8_t)((3100UL - rawRain) * 100 / 1100);
}

// Đọc cảm biến mức nước hồng ngoại (ISR trên chân PE9)
// Trả về: 1 nếu còn nước (HIGH), 0 nếu hết nước (LOW)
uint8_t Get_WaterLevel_Status(void) {
    return (HAL_GPIO_ReadPin(GPIOE, Sensor_WaterLevel_Pin) == GPIO_PIN_SET) ? 1 : 0;
}

// Quy đổi mức nước ra phần trăm (0 - 100%) để đóng gói vào CAN ID 0x401 (Byte 1)
uint8_t Get_WaterLevel_Percent(void) {
    return Get_WaterLevel_Status() ? 100 : 0;
}

// =============================================================================
// 2. ACTUATOR & WASHER MOTOR CONTROL
// =============================================================================

void Motor_SetPWM(uint16_t speed) {
    if (speed > 1000) speed = 1000;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, speed);
}

void Motor_Wash_Task(uint8_t isWashActive) {
    uint32_t currentTick = HAL_GetTick();

    // KIỂM TRA AN TOÀN: Chỉ bơm nước khi có lệnh bật VÀ cảm biến mực nước còn (PE9 = 1)
    if (isWashActive && Get_WaterLevel_Status()) {
        if (currentTick - lastCycleTime >= 3000) {
            lastCycleTime = currentTick;
        }
        uint32_t elapsed = currentTick - lastCycleTime;

        if (elapsed < 1000) {
            HAL_GPIO_WritePin(GPIOD, Signal_Left_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOD, Signal_Right_Pin, GPIO_PIN_RESET);
            Motor_SetPWM(800);
            HAL_GPIO_WritePin(GPIOE, LED_Moto_Pin, GPIO_PIN_SET);
        } else {
            Motor_SetPWM(0);
            HAL_GPIO_WritePin(GPIOD, Signal_Left_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOD, Signal_Right_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOE, LED_Moto_Pin, GPIO_PIN_RESET);
        }
    } else {
        // Hết nước (PE9 = 0) hoặc không kích hoạt -> Dừng bơm hoàn toàn để bảo vệ phần cứng
        lastCycleTime = currentTick;
        Motor_SetPWM(0);
        HAL_GPIO_WritePin(GPIOD, Signal_Left_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOD, Signal_Right_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOE, LED_Moto_Pin, GPIO_PIN_RESET);
    }
}

// =============================================================================
// 3. SYSTEM INIT & WIPER TASK
// =============================================================================

static void Servo_SetAngle(uint16_t pulseWidth) {
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, pulseWidth);
}

static void Wiper_UpdateOutputs(GPIO_PinState ld3, GPIO_PinState ld4, GPIO_PinState ld5, GPIO_PinState ld6) {
    HAL_GPIO_WritePin(GPIOD, LD3_Pin, ld3);
    HAL_GPIO_WritePin(GPIOD, LD4_Pin, ld4);
    HAL_GPIO_WritePin(GPIOD, LD5_Pin, ld5);
    HAL_GPIO_WritePin(GPIOD, LD6_Pin, ld6);
}

void Wiper_System_Init(void) {
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2); // Khởi động PWM Servo gạt mưa
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1); // Khởi động PWM Motor bơm nước
    Motor_SetPWM(0);
    HAL_GPIO_WritePin(GPIOD, Signal_Left_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOD, Signal_Right_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOE, LED_Moto_Pin, GPIO_PIN_RESET);
}

CmdWiper_t Servo_GetWiperMode(void) {
    return mWiper.currentWiper;
}

void Wiper_Task(void) {
    static uint8_t lastCmdWiper = 0;
    if (currentCmdWiper != lastCmdWiper) {
        lastCmdWiper = currentCmdWiper;
        if (currentCmdWiper <= CMD_WIPER_AUTO) {
            mWiper.tagetWiper  = (CmdWiper_t)currentCmdWiper;
            mWiper.currentWiper = mWiper.tagetWiper;
            mWiper.wiperTime   = HAL_GetTick();
        }
    }

    uint8_t currentBtnWiperState = HAL_GPIO_ReadPin(GPIOE, BTN_WiperMode_Pin);

    if (currentBtnWiperState == GPIO_PIN_RESET && mWiper.lastBtnWiperState == GPIO_PIN_SET) {
        if (HAL_GetTick() - mWiper.lastDebounceTimeWiper >= 200) {
            mWiper.tagetWiper++;
            if (mWiper.tagetWiper > CMD_WIPER_AUTO) {
                mWiper.tagetWiper = CMD_WIPER_OFF;
            }
            mWiper.currentWiper          = mWiper.tagetWiper;
            mWiper.wiperTime             = HAL_GetTick();
            mWiper.lastDebounceTimeWiper = HAL_GetTick();
            Bridge_SendWiperTurnStatus((uint8_t)mWiper.currentWiper, (uint8_t)TurnSignal_GetMode());
        }
    }
    mWiper.lastBtnWiperState = currentBtnWiperState;
    mWiper.currentWiper      = mWiper.tagetWiper;

    // Cập nhật giá trị cảm biến mưa định kỳ mỗi 3 giây
    if (HAL_GetTick() - mWiper.lastDelayTime >= 3000) {
        mWiper.cachedRainValue = Read_RainSensor_Raw();
        mWiper.lastDelayTime   = HAL_GetTick();
    }

    switch (mWiper.currentWiper) {
        case CMD_WIPER_OFF:
            Servo_SetAngle(1000);
            Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET);
            Motor_Wash_Task(0);
            break;

        case CMD_WIPER_INTERMITTENT:
            Motor_Wash_Task(1);
            if (HAL_GetTick() - mWiper.wiperTime < 1000) {
                Servo_SetAngle(2000);
                Wiper_UpdateOutputs(GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET);
            } else if (HAL_GetTick() - mWiper.wiperTime < 4000) {
                Servo_SetAngle(1000);
                Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET);
            } else {
                mWiper.wiperTime = HAL_GetTick();
            }
            break;

        case CMD_WIPER_SLOW:
            Motor_Wash_Task(0);
            if (HAL_GetTick() - mWiper.wiperTime < 1000) {
                Servo_SetAngle(2000);
                Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_RESET);
            } else if (HAL_GetTick() - mWiper.wiperTime < 2500) {
                Servo_SetAngle(1000);
                Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_RESET);
            } else {
                mWiper.wiperTime = HAL_GetTick();
            }
            break;

        case CMD_WIPER_NORMAL:
        case CMD_WIPER_FAST:
            Motor_Wash_Task(0);
            if (HAL_GetTick() - mWiper.wiperTime < 300) {
                Servo_SetAngle(2000);
                Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_RESET);
            } else if (HAL_GetTick() - mWiper.wiperTime < 700) {
                Servo_SetAngle(1000);
                Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_RESET);
            } else {
                mWiper.wiperTime = HAL_GetTick();
            }
            break;

        case CMD_WIPER_AUTO:
        {
            uint16_t rainValue           = mWiper.cachedRainValue;
            uint8_t  currentAutoSubState = 4;

            if (rainValue > 3100) {
                Servo_SetAngle(1000);
                Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET);
                Motor_Wash_Task(0);
                currentAutoSubState = 4;
            } else if (rainValue > 2500) {
                Motor_Wash_Task(1);
                if (HAL_GetTick() - mWiper.wiperTime < 1000) {
                    Servo_SetAngle(1500);
                    Wiper_UpdateOutputs(GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET);
                } else if (HAL_GetTick() - mWiper.wiperTime < 4000) {
                    Servo_SetAngle(1000);
                    Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET);
                } else {
                    mWiper.wiperTime = HAL_GetTick();
                }
                currentAutoSubState = 5;
            } else if (rainValue > 2000) {
                Motor_Wash_Task(0);
                if (HAL_GetTick() - mWiper.wiperTime < 1000) {
                    Servo_SetAngle(2000);
                    Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_SET);
                } else if (HAL_GetTick() - mWiper.wiperTime < 2500) {
                    Servo_SetAngle(1000);
                    Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_SET);
                } else {
                    mWiper.wiperTime = HAL_GetTick();
                }
                currentAutoSubState = 6;
            } else {
                Motor_Wash_Task(0);
                if (HAL_GetTick() - mWiper.wiperTime < 300) {
                    Servo_SetAngle(2000);
                    Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_SET);
                } else if (HAL_GetTick() - mWiper.wiperTime < 700) {
                    Servo_SetAngle(1000);
                    Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_SET);
                } else {
                    mWiper.wiperTime = HAL_GetTick();
                }
                currentAutoSubState = 7;
            }

            static uint8_t lastSentAutoState = 0xFF;
            if (currentAutoSubState != lastSentAutoState) {
                lastSentAutoState = currentAutoSubState;
                Bridge_SendWiperTurnStatus(currentAutoSubState, (uint8_t)TurnSignal_GetMode());
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
