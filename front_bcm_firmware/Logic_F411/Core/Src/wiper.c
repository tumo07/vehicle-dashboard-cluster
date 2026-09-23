/*
 * wiper.c
 * Front BCM - Wiper, Washer & Sensor Management Module
 * Standardized with can_messages.h v3.0
 * Water Sensor (PE9): Pre-check & Latched Fault, Servo INT runs independently
 */

#include "wiper.h"
#include "TX_data_UART.h"
#include "RX_data_UART.h"
#include "turnsignal.h"
#include "can_messages.h"

extern TIM_HandleTypeDef htim2; // TIM2_CH2: PA1 (PWM Servo gạt mưa)
extern TIM_HandleTypeDef htim3; // TIM3_CH1: PC6 (PWM Bơm nước rửa kính)
extern ADC_HandleTypeDef hadc1; // ADC1_IN0: PA0 (Cảm biến mưa)
extern uint16_t adc_dma_buffer[2];

extern void Bridge_SendWiperTurnStatus(uint8_t wiperMode, uint8_t turnMode);
extern void Bridge_SendFault(uint8_t severity, uint16_t dtc_code, uint8_t counter, uint8_t simple_err);
extern CmdTurn_t TurnSignal_GetMode(void);

static uint32_t lastCycleTime = 0;

// Cờ khóa lỗi cạn nước (Latch Fault): Giữ nguyên trạng thái ngắt cho đến khi nhấn RESET board
static uint8_t  is_water_fault_latched = 0;
static uint32_t emptyDebounceTick      = 0;
static uint8_t  faultReportSent        = 0;

static mCrtlWiper_t mWiper = {
    .currentWiper          = CMD_WIPER_OFF,
    .tagetWiper            = CMD_WIPER_OFF,
    .lastDebounceTimeWiper = 0,
    .lastBtnWiperState     = GPIO_PIN_SET,
    .wiperTime             = 0,
    .cachedRainValue       = 3500,
    .lastDelayTime         = 0,
};

// =============================================================================
// 1. SENSOR READING TASKS & FAULT LATCHING
// =============================================================================
static void WasherFluid_Process(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t rawPin = HAL_GPIO_ReadPin(GPIOE, Sensor_WaterLevel_Pin);

    // =========================================================================
    // 1. CẬP NHẬT ĐÈN TỨC THỜI LIÊN TỤC (KHÔNG BỊ ẢNH HƯỞNG BỞI LATCH FAULT)
    // Cảm biến ON (HIGH) -> LED BẬT | Cảm biến OFF (LOW) -> LED TẮT
    // =========================================================================
    if (rawPin == GPIO_PIN_SET) {
        HAL_GPIO_WritePin(GPIOE, LED_WaterLevel_Pin, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(GPIOE, LED_WaterLevel_Pin, GPIO_PIN_SET);
    }

    // =========================================================================
    // 2. LOGIC KHÓA LỖI BẢO VỆ BƠM NƯỚC (LATCHED FAULT)
    // =========================================================================
    // Nếu đã từng bị cạn nước và khóa lỗi trước đó, dừng xử lý logic bảo vệ (chờ Reset board)
    if (is_water_fault_latched) {
        return;
    }

    // Khi cảm biến OFF (PE9 = RESET: Hết nước)
    if (rawPin == GPIO_PIN_RESET) {
        if (emptyDebounceTick == 0) {
            emptyDebounceTick = now;
        } else if (now - emptyDebounceTick >= 300) { // Lọc chống nhiễu 300ms
            // CHÍNH THỨC KHÓA LỖI TRONG RAM CHO ĐẾN KHI BẤM RESET MCU
            is_water_fault_latched = 1;

            // Dập tắt mô-tơ bơm nước ngay lập tức
            __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
            HAL_GPIO_WritePin(GPIOD, Signal_Left_Pin | Signal_Right_Pin, GPIO_PIN_RESET);

            // Bắn bản tin DTC B1012 lên Central ECU
            if (!faultReportSent) {
                Bridge_SendFault(DTC_SEVERITY_WARNING, DTC_B1012, 1, ERR_SENSOR_FAULT);
                #ifdef LED_Error_Pin
                HAL_GPIO_WritePin(LED_Error_GPIO_Port, LED_Error_Pin, GPIO_PIN_SET);
                #endif
                faultReportSent = 1;
            }
        }
    } else {
        emptyDebounceTick = 0;
    }
}

uint8_t Get_WaterLevel_Status(void) {
    if (is_water_fault_latched) return 0;
    return (HAL_GPIO_ReadPin(GPIOE, Sensor_WaterLevel_Pin) == GPIO_PIN_SET) ? 1 : 0;
}

static uint16_t Read_RainSensor_Raw(void) {
    // Channel 0 (Rank 1 - PA0) được lưu ở phần tử 0
    return adc_dma_buffer[0];
}

// Hàm tính % Ánh sáng môi trường dựa theo các mức độ mưa
uint8_t Get_RainSensor_Percent(void) {
    uint16_t rainVal = mWiper.cachedRainValue;

    if (rainVal > 3000) {
        return 0; // Dry: 0% rain
    } else if (rainVal > 2400) {
        return 30; // Light rain: 30%
    } else if (rainVal > 1800) {
        return 60; // Medium rain: 60%
    } else {
        return 100; // Heavy rain: 100%
    }
}

// Hàm tính % Mức nước rửa kính: còn nước thì 80%, hết nước thì 0%
uint8_t Get_WaterLevel_Percent(void) {
    if (Get_WaterLevel_Status() == 1) {
        return 80; // Còn nước: cố định 80%
    } else {
        return 0;  // Hết nước: 0%
    }
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

    // TIỀN KIỂM TRA: Cảm biến OFF hoặc đã bị khóa cạn nước -> TẮT BƠM
    if (Get_WaterLevel_Status() == 0) {
        lastCycleTime = currentTick;
        Motor_SetPWM(0);
        HAL_GPIO_WritePin(GPIOD, Signal_Left_Pin | Signal_Right_Pin, GPIO_PIN_RESET);
        return;
    }

    // Khi cảm biến ON: Cho phép bơm chạy ngắt quãng
    if (isWashActive) {
        if (currentTick - lastCycleTime >= 3000) {
            lastCycleTime = currentTick;
        }
        uint32_t elapsed = currentTick - lastCycleTime;

        if (elapsed < 1000) {
            HAL_GPIO_WritePin(GPIOD, Signal_Left_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOD, Signal_Right_Pin, GPIO_PIN_RESET);
            Motor_SetPWM(800);
        } else {
            Motor_SetPWM(0);
            HAL_GPIO_WritePin(GPIOD, Signal_Left_Pin | Signal_Right_Pin, GPIO_PIN_RESET);
        }
    } else {
        lastCycleTime = currentTick;
        Motor_SetPWM(0);
        HAL_GPIO_WritePin(GPIOD, Signal_Left_Pin | Signal_Right_Pin, GPIO_PIN_RESET);
    }
}

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
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    Motor_SetPWM(0);
    HAL_GPIO_WritePin(GPIOD, Signal_Left_Pin | Signal_Right_Pin, GPIO_PIN_RESET);
    Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET);

    is_water_fault_latched = 0;
    emptyDebounceTick      = 0;
    faultReportSent        = 0;

    // Cập nhật đèn LED_WaterLevel theo trạng thái chân cảm biến lúc khởi động
	uint8_t rawWater = HAL_GPIO_ReadPin(GPIOE, Sensor_WaterLevel_Pin);
	HAL_GPIO_WritePin(GPIOE, LED_WaterLevel_Pin, (rawWater == GPIO_PIN_SET) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

CmdWiper_t Servo_GetWiperMode(void) {
    return mWiper.currentWiper;
}

// =============================================================================
// 3. TASK CHÍNH: QUẢN LÝ CHU TRÌNH GẠT MƯA
// =============================================================================
void Wiper_Task(void) {
    uint32_t currentTick = HAL_GetTick();

    // 1. Quét cảm biến mức nước PE9
    WasherFluid_Process();

    // 2. Nhận lệnh từ Central ECU qua UART
    static uint8_t lastCmdWiper = 0xFF;
    if (currentCmdWiper != lastCmdWiper) {
        lastCmdWiper = currentCmdWiper;
        if (currentCmdWiper <= (uint8_t)CMD_WIPER_AUTO) {
            mWiper.tagetWiper   = (CmdWiper_t)currentCmdWiper;
            mWiper.currentWiper = mWiper.tagetWiper;
            mWiper.wiperTime    = currentTick;
        }
    }

    // 3. Quét nút bấm PE8 luân chuyển chế độ
    uint8_t currentBtnWiperState = HAL_GPIO_ReadPin(GPIOE, BTN_WiperMode_Pin);

    if (currentBtnWiperState == GPIO_PIN_RESET && mWiper.lastBtnWiperState == GPIO_PIN_SET) {
        if (currentTick - mWiper.lastDebounceTimeWiper >= 200) {
            if (mWiper.tagetWiper == CMD_WIPER_OFF) {
                mWiper.tagetWiper = CMD_WIPER_INTERMITTENT;
            } else if (mWiper.tagetWiper == CMD_WIPER_INTERMITTENT) {
                mWiper.tagetWiper = CMD_WIPER_SLOW;
            } else if (mWiper.tagetWiper == CMD_WIPER_SLOW) {
                mWiper.tagetWiper = CMD_WIPER_NORMAL;
            } else if (mWiper.tagetWiper == CMD_WIPER_NORMAL) {
                mWiper.tagetWiper = CMD_WIPER_FAST;
            } else if (mWiper.tagetWiper == CMD_WIPER_FAST) {
                mWiper.tagetWiper = CMD_WIPER_AUTO;
            } else {
                mWiper.tagetWiper = CMD_WIPER_OFF;
            }

            mWiper.currentWiper          = mWiper.tagetWiper;
            mWiper.wiperTime             = currentTick;
            mWiper.lastDebounceTimeWiper = currentTick;
            Bridge_SendWiperTurnStatus((uint8_t)mWiper.currentWiper, (uint8_t)TurnSignal_GetMode());
        }
    }
    mWiper.lastBtnWiperState = currentBtnWiperState;
    mWiper.currentWiper      = mWiper.tagetWiper;

    // Đọc cảm biến mưa định kỳ mỗi 500ms
    if (currentTick - mWiper.lastDelayTime >= 500) {
        mWiper.cachedRainValue = Read_RainSensor_Raw();
        mWiper.lastDelayTime   = currentTick;
    }

    // 4. Bơm nước rửa kính (Chỉ chạy ở INT và tự tắt ngay nếu cạn nước)
    uint8_t isWashActive = (mWiper.currentWiper == CMD_WIPER_INTERMITTENT) ? 1 : 0;
    Motor_Wash_Task(isWashActive);

    // 5. Điều khiển Servo & LED (Servo INT chạy độc lập với motor bơm)
    switch (mWiper.currentWiper) {
        case CMD_WIPER_OFF:
            Servo_SetAngle(1000);
            Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET);
            break;

        case CMD_WIPER_INTERMITTENT:
            Wiper_UpdateOutputs(GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET);
            if (currentTick - mWiper.wiperTime < 600) {
                Servo_SetAngle(2000);
            } else if (currentTick - mWiper.wiperTime < 1200) {
                Servo_SetAngle(1000);
            } else if (currentTick - mWiper.wiperTime < 4000) {
                Servo_SetAngle(1000);
            } else {
                mWiper.wiperTime = currentTick;
            }
            break;

        case CMD_WIPER_SLOW:
            Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_RESET);
            if (currentTick - mWiper.wiperTime < 900) {
                Servo_SetAngle(2000);
            } else if (currentTick - mWiper.wiperTime < 1800) {
                Servo_SetAngle(1000);
            } else {
                mWiper.wiperTime = currentTick;
            }
            break;

        case CMD_WIPER_NORMAL:
            Wiper_UpdateOutputs(GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_RESET);
            if (currentTick - mWiper.wiperTime < 600) {
                Servo_SetAngle(2000);
            } else if (currentTick - mWiper.wiperTime < 1200) {
                Servo_SetAngle(1000);
            } else {
                mWiper.wiperTime = currentTick;
            }
            break;

        case CMD_WIPER_FAST:
            Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_RESET);
            if (currentTick - mWiper.wiperTime < 300) {
                Servo_SetAngle(2000);
            } else if (currentTick - mWiper.wiperTime < 600) {
                Servo_SetAngle(1000);
            } else {
                mWiper.wiperTime = currentTick;
            }
            break;

        case CMD_WIPER_AUTO:
        {
            uint16_t rainValue = mWiper.cachedRainValue;

            if (rainValue > 3000) {
                Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET);
                Servo_SetAngle(1000);
            } else if (rainValue > 2400) {
                Wiper_UpdateOutputs(GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET);
                if (currentTick - mWiper.wiperTime < 900) {
                    Servo_SetAngle(2000);
                } else if (currentTick - mWiper.wiperTime < 1800) {
                    Servo_SetAngle(1000);
                } else {
                    mWiper.wiperTime = currentTick;
                }
            } else if (rainValue > 1800) {
                Wiper_UpdateOutputs(GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_SET);
                if (currentTick - mWiper.wiperTime < 600) {
                    Servo_SetAngle(2000);
                } else if (currentTick - mWiper.wiperTime < 1200) {
                    Servo_SetAngle(1000);
                } else {
                    mWiper.wiperTime = currentTick;
                }
            } else {
                Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_SET);
                if (currentTick - mWiper.wiperTime < 300) {
                    Servo_SetAngle(2000);
                } else if (currentTick - mWiper.wiperTime < 600) {
                    Servo_SetAngle(1000);
                } else {
                    mWiper.wiperTime = currentTick;
                }
            }
            break;
        }

        default:
            Servo_SetAngle(1000);
            Wiper_UpdateOutputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET);
            break;
    }
}
