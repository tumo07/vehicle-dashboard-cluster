#include "TX_data_UART.h"
#include "turnsignal.h"
#include "wiper.h"

static CmdTurn_t    currentTurnMode      = CMD_TURN_OFF;
static uint32_t     lastFlashTime        = 0;
static uint32_t     lastDebounceTime     = 0;
static uint8_t      flashState           = 0;
static uint8_t      local_failsafe_flash = 0;

static uint8_t      lastLeftBtnState     = 1;
static uint8_t      lastRightBtnState    = 1;
static uint8_t      lastHazardBtnState   = 1;

void TurnSignal_Init(void) {
    currentTurnMode      = CMD_TURN_OFF;
    lastFlashTime        = HAL_GetTick();
    lastDebounceTime     = HAL_GetTick();
    flashState           = 0;
    local_failsafe_flash = 0;
    lastLeftBtnState     = 1;
    lastRightBtnState    = 1;
    lastHazardBtnState   = 1;
}

void TurnSignal_Task(void) {
    uint32_t currentTime = HAL_GetTick();

    // 1. Quét nút bấm cục bộ - Gửi lệnh yêu cầu lên Coordinator, KHÔNG tự ý đổi trạng thái tại chỗ
    if (currentTime - lastDebounceTime >= 50) {
        uint8_t currentLeftState   = HAL_GPIO_ReadPin(GPIOB, BTN_TurnLeft_Pin);
        uint8_t currentRightState  = HAL_GPIO_ReadPin(GPIOB, BTN_TurnRight_Pin);
        uint8_t currentHazardState = HAL_GPIO_ReadPin(GPIOB, BTN_Hazard_Pin);

        if (currentLeftState == GPIO_PIN_RESET && lastLeftBtnState == GPIO_PIN_SET) {
            Bridge_SendTurnRequest(CMD_TURN_LEFT);
        }
        if (currentRightState == GPIO_PIN_RESET && lastRightBtnState == GPIO_PIN_SET) {
            Bridge_SendTurnRequest(CMD_TURN_RIGHT);
        }
        if (currentHazardState == GPIO_PIN_RESET && lastHazardBtnState == GPIO_PIN_SET) {
            Bridge_SendTurnRequest(CMD_TURN_HAZARD);
        }

        lastLeftBtnState   = currentLeftState;
        lastRightBtnState  = currentRightState;
        lastHazardBtnState = currentHazardState;
        lastDebounceTime   = currentTime;
    }

    // 2. Nhận cờ vũ trang (arming mask) từ Central ECU qua CAN 0x202 (CAN_ID_EXEC_FRONT_TURN)
    uint8_t left_active  = (currentCmdTurn & EXEC_TURN_LEFT_ARM)  || (currentCmdTurn & EXEC_TURN_HAZARD_ARM);
    uint8_t right_active = (currentCmdTurn & EXEC_TURN_RIGHT_ARM) || (currentCmdTurn & EXEC_TURN_HAZARD_ARM);

    // Cập nhật currentTurnMode để phục vụ TurnSignal_GetMode() & báo cáo telemetry
    if (currentCmdTurn & EXEC_TURN_HAZARD_ARM) {
        currentTurnMode = CMD_TURN_HAZARD;
    } else if (currentCmdTurn & EXEC_TURN_LEFT_ARM) {
        currentTurnMode = CMD_TURN_LEFT;
    } else if (currentCmdTurn & EXEC_TURN_RIGHT_ARM) {
        currentTurnMode = CMD_TURN_RIGHT;
    } else {
        currentTurnMode = CMD_TURN_OFF;
    }

    // 3. ĐỒNG BỘ NHỊP CHỚP TOÀN CỤC CHUẨN FMVSS 108 / ECE R48 (0x130 BLINK TICK)
    // Front BCM đảo pha trực tiếp theo nhịp 0x130 của Central ECU (< 1.2ms skew)
    extern volatile uint8_t  f411_cmd_blink_tick;
    extern volatile uint32_t f411_last_can_cmd_tick;
    extern volatile uint8_t  f411_can_connected;

    if (left_active || right_active) {
        if (f411_can_connected && (currentTime - f411_last_can_cmd_tick < 1500)) {
            // Đồng bộ 100% với nhịp phát 0x130 của Central ECU
            flashState = (f411_cmd_blink_tick & 0x01);
        } else {
            // Fail-safe 500ms khi mất kết nối CAN
            if (currentTime - lastFlashTime >= 500) {
                local_failsafe_flash = !local_failsafe_flash;
                lastFlashTime = currentTime;
            }
            flashState = local_failsafe_flash;
        }
    } else {
        flashState = 0;
        lastFlashTime = currentTime;
    }

    // 4. Xuất điều khiển phần cứng LED xi-nhan phía trước (FLASH ONLY)
    HAL_GPIO_WritePin(GPIOC, LED_SignalLeft_Pin,  (left_active  && flashState) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, LED_SignalRight_Pin, (right_active && flashState) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

CmdTurn_t TurnSignal_GetMode(void) {
    return currentTurnMode;
}
