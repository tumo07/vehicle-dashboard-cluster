#include <TX_data_UART.h>
#include "turnsignal.h"
#include "wiper.h" // Thay thế cho servo.h cũ do đã gộp

static CmdTurn_t     currentTurnMode    = CMD_TURN_OFF;
static uint32_t     lastFlashTime       = 0;
static uint32_t     lastDebounceTime    = 0;
static uint8_t      flashState          = 0;

static uint8_t      lastLeftBtnState    = 1;
static uint8_t      lastRightBtnState   = 1;
static uint8_t      lastHazardBtnState  = 1;

extern void Bridge_SendWiperTurnStatus(uint8_t wiperMode, uint8_t turnMode);

void TurnSignal_Init(void){
    currentTurnMode = CMD_TURN_OFF;
    lastFlashTime = HAL_GetTick();
    lastDebounceTime = HAL_GetTick();
    flashState = 0;
}

void TurnSignal_Task(void){
    uint32_t currentTime = HAL_GetTick();

    // 1. Nhận lệnh phân luồng xi-nhan từ Central ECU (0x202 CAN_ID_EXEC_FRONT_TURN)
    static uint8_t lastCmdTurn = 0xFF;
    if (currentCmdTurn != lastCmdTurn) {
        lastCmdTurn = currentCmdTurn;
        if (currentCmdTurn & EXEC_TURN_HAZARD_ARM) {
            currentTurnMode = CMD_TURN_HAZARD;
        } else if (currentCmdTurn & EXEC_TURN_LEFT_ARM) {
            currentTurnMode = CMD_TURN_LEFT;
        } else if (currentCmdTurn & EXEC_TURN_RIGHT_ARM) {
            currentTurnMode = CMD_TURN_RIGHT;
        } else {
            currentTurnMode = CMD_TURN_OFF;
        }
    }

    // 2. Quét nút bấm cục bộ (để hỗ trợ chuyển tiếp sự kiện yêu cầu lên Coordinator)
    if (currentTime - lastDebounceTime >= 50) {
        uint8_t currentLeftState   = HAL_GPIO_ReadPin(GPIOB, BTN_TurnLeft_Pin);
        uint8_t currentRightState  = HAL_GPIO_ReadPin(GPIOB, BTN_TurnRight_Pin);
        uint8_t currentHazardState = HAL_GPIO_ReadPin(GPIOB, BTN_Hazard_Pin);

        if (currentLeftState == GPIO_PIN_RESET && lastLeftBtnState == GPIO_PIN_SET){
            CmdTurn_t req = (currentTurnMode == CMD_TURN_LEFT) ? CMD_TURN_OFF : CMD_TURN_LEFT;
            currentTurnMode = req;
            Bridge_SendWiperTurnStatus((uint8_t)Servo_GetWiperMode(), (uint8_t)currentTurnMode);
        }
        if (currentRightState == GPIO_PIN_RESET && lastRightBtnState == GPIO_PIN_SET){
            CmdTurn_t req = (currentTurnMode == CMD_TURN_RIGHT) ? CMD_TURN_OFF : CMD_TURN_RIGHT;
            currentTurnMode = req;
            Bridge_SendWiperTurnStatus((uint8_t)Servo_GetWiperMode(), (uint8_t)currentTurnMode);
        }
        if (currentHazardState == GPIO_PIN_RESET && lastHazardBtnState == GPIO_PIN_SET){
            CmdTurn_t req = (currentTurnMode == CMD_TURN_HAZARD) ? CMD_TURN_OFF : CMD_TURN_HAZARD;
            currentTurnMode = req;
            Bridge_SendWiperTurnStatus((uint8_t)Servo_GetWiperMode(), (uint8_t)currentTurnMode);
        }

        lastLeftBtnState    = currentLeftState;
        lastRightBtnState   = currentRightState;
        lastHazardBtnState  = currentHazardState;
        lastDebounceTime    = currentTime;
    }

    // 3. ĐỒNG BỘ NHỊP TIM TOÀN CỤC CHUẨN FMVSS 108 / ECE R48 (0x130 BLINK TICK)
    // Front BCM đảo pha trực tiếp theo nhịp 0x130 của Central ECU (< 1.2ms skew)
    extern volatile uint8_t  f411_cmd_blink_tick;
    extern volatile uint32_t f411_last_can_cmd_tick;
    extern volatile uint8_t  f411_can_connected;

    if (currentTurnMode == CMD_TURN_OFF) {
        flashState = 0;
        lastFlashTime = currentTime;
    } else if (f411_can_connected && (currentTime - f411_last_can_cmd_tick < 1500)) {
        // Đồng bộ 100% với nhịp phát của Central ECU
        flashState = (f411_cmd_blink_tick & 0x01);
    } else {
        // Dự phòng (Fail-Safe fallback) khi mất kết nối bus CAN
        if (currentTime - lastFlashTime >= 500) {
            flashState = !flashState;
            lastFlashTime = currentTime;
        }
    }

    // 4. Xuất điều khiển phần cứng LED xi-nhan
    GPIO_PinState leftLED  = GPIO_PIN_RESET;
    GPIO_PinState rightLED = GPIO_PIN_RESET;

    switch(currentTurnMode){
        case CMD_TURN_OFF:
            leftLED  = GPIO_PIN_RESET;
            rightLED = GPIO_PIN_RESET;
            break;
        case CMD_TURN_LEFT:
            leftLED  = flashState ? GPIO_PIN_SET : GPIO_PIN_RESET;
            rightLED = GPIO_PIN_RESET;
            break;
        case CMD_TURN_RIGHT:
            leftLED  = GPIO_PIN_RESET;
            rightLED = flashState ? GPIO_PIN_SET : GPIO_PIN_RESET;
            break;
        case CMD_TURN_HAZARD:
            leftLED  = flashState ? GPIO_PIN_SET : GPIO_PIN_RESET;
            rightLED = flashState ? GPIO_PIN_SET : GPIO_PIN_RESET;
            break;
    }
    HAL_GPIO_WritePin(GPIOC, LED_SignalLeft_Pin, leftLED);
    HAL_GPIO_WritePin(GPIOC, LED_SignalRight_Pin, rightLED);
}

CmdTurn_t TurnSignal_GetMode(void) {
    return currentTurnMode;
}
