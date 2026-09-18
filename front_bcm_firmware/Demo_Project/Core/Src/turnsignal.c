#include "turnsignal.h"
#include "data_uart.h"
#include "servo.h"

static CmdTurn_t     currentTurnMode    = CMD_TURN_OFF;
static uint32_t     lastFlashTime       = 0;
static uint32_t     lastDebounceTime    = 0;
static uint8_t      flashState          = 0;

static uint8_t      lastLeftBtnState    = 1;
static uint8_t      lastRightBtnState   = 1;
static uint8_t      lastHazardBtnState  = 1;

extern void Bridge_SendStatus(uint8_t wiperMode, uint8_t turnMode);
extern CmdWiper_t Servo_GetWiperMode(void);

void TurnSignal_Init(void){
    currentTurnMode = CMD_TURN_OFF;
    lastFlashTime = HAL_GetTick();
    lastDebounceTime = HAL_GetTick();
    flashState = 0;
}

void TurnSignal_Task(void){
    uint32_t currentTime = HAL_GetTick();

    static uint8_t lastCmdTurn = 0;
    if (currentCmdTurn != lastCmdTurn) {
        lastCmdTurn = currentCmdTurn;
        if (currentCmdTurn <= CMD_TURN_HAZARD) { // 0: Off, 1: Left, 2: Right, 3: Hazard
            currentTurnMode = (CmdTurn_t)currentCmdTurn;
            flashState = 1;
            lastFlashTime = currentTime;
        }
    }

    if (currentTime - lastDebounceTime >= 50) {
        uint8_t currentLeftState = HAL_GPIO_ReadPin(GPIOB, BTN_TurnLeft_Pin);
        uint8_t currentRightState = HAL_GPIO_ReadPin(GPIOB, BTN_TurnRight_Pin);
        uint8_t currentHazardState = HAL_GPIO_ReadPin(GPIOB, BTN_Hazard_Pin);

        if (currentLeftState == GPIO_PIN_RESET && lastLeftBtnState == GPIO_PIN_SET){
            if(currentTurnMode == CMD_TURN_LEFT){
                currentTurnMode = CMD_TURN_OFF;
            }
            else{
                currentTurnMode = CMD_TURN_LEFT;
                flashState = 1;
                lastFlashTime = currentTime;
            }
            Bridge_SendStatus((uint8_t)Servo_GetWiperMode(), (uint8_t)currentTurnMode);
        }
        if (currentRightState == GPIO_PIN_RESET && lastRightBtnState == GPIO_PIN_SET){
            if(currentTurnMode == CMD_TURN_RIGHT){
                currentTurnMode = CMD_TURN_OFF;
            }
            else{
                currentTurnMode = CMD_TURN_RIGHT;
                flashState = 1;
                lastFlashTime = currentTime;
            }
            Bridge_SendStatus((uint8_t)Servo_GetWiperMode(), (uint8_t)currentTurnMode);
        }
        if (currentHazardState == GPIO_PIN_RESET && lastHazardBtnState == GPIO_PIN_SET){
            if(currentTurnMode == CMD_TURN_HAZARD){
                currentTurnMode = CMD_TURN_OFF;
            }
            else{
                currentTurnMode = CMD_TURN_HAZARD;
                flashState = 1;
                lastFlashTime = currentTime;
            }
            Bridge_SendStatus((uint8_t)Servo_GetWiperMode(), (uint8_t)currentTurnMode);
        }

        lastLeftBtnState    = currentLeftState;
        lastRightBtnState   = currentRightState;
        lastHazardBtnState  = currentHazardState;
        lastDebounceTime    = currentTime;
    }

    if(currentTime - lastFlashTime >= 500){
        flashState = !flashState;
        lastFlashTime = currentTime;
    }

    GPIO_PinState leftLED = GPIO_PIN_RESET;
    GPIO_PinState rightLED = GPIO_PIN_RESET;

    switch(currentTurnMode){
        case CMD_TURN_OFF:
            leftLED = GPIO_PIN_RESET;
            rightLED = GPIO_PIN_RESET;
            break;
        case CMD_TURN_LEFT:
            leftLED = flashState ? GPIO_PIN_SET : GPIO_PIN_RESET;
            rightLED = GPIO_PIN_RESET;
            break;
        case CMD_TURN_RIGHT:
            leftLED = GPIO_PIN_RESET;
            rightLED = flashState ? GPIO_PIN_SET : GPIO_PIN_RESET;
            break;
        case CMD_TURN_HAZARD:
            leftLED = flashState ? GPIO_PIN_SET : GPIO_PIN_RESET;
            rightLED = flashState ? GPIO_PIN_SET : GPIO_PIN_RESET;
            break;
    }
    HAL_GPIO_WritePin(GPIOC, LED_SignalLeft_Pin, leftLED);
    HAL_GPIO_WritePin(GPIOC, LED_SignalRight_Pin, rightLED);
}

CmdTurn_t TurnSignal_GetMode(void) {
    return currentTurnMode;
}
