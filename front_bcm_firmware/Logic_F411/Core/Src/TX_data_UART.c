/*
 * TX_data_UART.c
 * Front BCM (STM32F411) - Unified Transmission & Periodic Task Module
 */

#include "TX_data_UART.h"
#include "RX_data_UART.h"
#include "can_messages.h"
#include "wiper.h"
#include "turnsignal.h"
#include "headlight.h"

static UART_HandleTypeDef *pUartHandle = NULL;

uint8_t currentCmdWiper = 0;
uint8_t currentCmdTurn  = 0;

// Khai báo đúng các hàm đọc cảm biến từ wiper.c
extern uint8_t Get_RainSensor_Percent(void);
extern uint8_t Get_WaterLevel_Percent(void);
extern uint8_t Get_WaterLevel_Status(void);

// ==============================================================================
// 1. CÁC HÀM TRUYỀN DỮ LIỆU THÔ SANG F103
// ==============================================================================
void Bridge_Init(UART_HandleTypeDef *huart) {
    pUartHandle = huart;
    Front_UART_RX_Init(huart);
}

void Bridge_SendFault(uint8_t severity, uint16_t dtc_code, uint8_t counter, uint8_t simple_err) {
    if (pUartHandle == NULL) return;
    uint8_t txPacket[6];
    txPacket[0] = 0xEE;
    txPacket[1] = severity;
    txPacket[2] = UNPACK_HIGH_BYTE(dtc_code);
    txPacket[3] = UNPACK_LOW_BYTE(dtc_code);
    txPacket[4] = counter;
    txPacket[5] = simple_err;

    HAL_UART_Transmit(pUartHandle, txPacket, 6, 20);
}

void Bridge_SendStatusReport(uint8_t actuatorFlags, uint8_t wiperMode, uint8_t motorHealth) {
    if (pUartHandle == NULL) return;
    uint8_t txPacket[4];
    txPacket[0] = 0x40;
    txPacket[1] = actuatorFlags;
    txPacket[2] = wiperMode;
    txPacket[3] = motorHealth;

    HAL_UART_Transmit(pUartHandle, txPacket, 4, 20);
}

void Bridge_SendSensorReport(uint8_t rainPercent, uint8_t waterPercent) {
    if (pUartHandle == NULL) return;
    uint8_t txPacket[3];
    txPacket[0] = 0x41;
    txPacket[1] = rainPercent;
    txPacket[2] = waterPercent;

    HAL_UART_Transmit(pUartHandle, txPacket, 3, 20);
}

void Bridge_SendHeartbeat(uint8_t counter, uint8_t hbFlags) {
    if (pUartHandle == NULL) return;
    uint8_t txPacket[3];
    txPacket[0] = 0x71;
    txPacket[1] = counter;
    txPacket[2] = hbFlags;

    HAL_UART_Transmit(pUartHandle, txPacket, 3, 20);
}

void Bridge_SendWiperTurnStatus(uint8_t wiperMode, uint8_t turnMode) {
    if (pUartHandle == NULL) return;
    uint8_t actuatorFlags = 0;
    if (wiperMode > CMD_WIPER_OFF) {
        actuatorFlags |= FRONT_ACT_WIPER;
    }
    if (turnMode == CMD_TURN_LEFT) {
        actuatorFlags |= FRONT_ACT_LTURN;
    } else if (turnMode == CMD_TURN_RIGHT) {
        actuatorFlags |= FRONT_ACT_RTURN;
    } else if (turnMode == CMD_TURN_HAZARD) {
        actuatorFlags |= (FRONT_ACT_LTURN | FRONT_ACT_RTURN);
    }
    actuatorFlags |= Headlight_Get_Actuator_Flags();

    Bridge_SendStatusReport(actuatorFlags, wiperMode, 100);
}

// ==============================================================================
// 2. TASK CHU KỲ ĐỊNH THỜI PHÁT UART (CAN v3.0 PROTOCOL MATRIX TIMING)
// ==============================================================================
void Front_BCM_Periodic_TX_Task(void) {
    uint32_t currentTick = HAL_GetTick();

    // -------------------------------------------------------------
    // Task 1: Báo cáo trạng thái cơ cấu chấp hành mỗi 200ms (CAN 0x400)
    // -------------------------------------------------------------
    static uint32_t lastStatusTime = 0;
    if (currentTick - lastStatusTime >= 200) {
        lastStatusTime = currentTick;

        uint8_t actuatorFlags = 0;
        CmdWiper_t currentWiper = Servo_GetWiperMode();
        CmdTurn_t currentTurn   = TurnSignal_GetMode();

        if (currentWiper > CMD_WIPER_OFF) {
            actuatorFlags |= FRONT_ACT_WIPER;
        }

        if ((f411_cmd_wiper_washer > 0) ||
            (currentWiper == CMD_WIPER_INTERMITTENT && Get_WaterLevel_Status() == 1)) {
            actuatorFlags |= FRONT_ACT_WASHER;
        }

        if (currentTurn == CMD_TURN_LEFT) {
            actuatorFlags |= FRONT_ACT_LTURN;
        } else if (currentTurn == CMD_TURN_RIGHT) {
            actuatorFlags |= FRONT_ACT_RTURN;
        } else if (currentTurn == CMD_TURN_HAZARD) {
            actuatorFlags |= (FRONT_ACT_LTURN | FRONT_ACT_RTURN);
        }

        actuatorFlags |= Headlight_Get_Actuator_Flags();

        Bridge_SendStatusReport(actuatorFlags, (uint8_t)currentWiper, 100);
    }

    // -------------------------------------------------------------
    // Task 2: Báo cáo cảm biến LDR và Cảm biến mưa mỗi 500ms (CAN 0x401)
    // -------------------------------------------------------------
    static uint32_t lastSensorTime = 0;
    if (currentTick - lastSensorTime >= 500) {
        lastSensorTime = currentTick;

        uint8_t rainPercent  = Get_RainSensor_Percent(); // 0-100%
        uint8_t waterLevel   = Get_WaterLevel_Percent(); // 80% or 0%

        Bridge_SendSensorReport(rainPercent, waterLevel);
    }

    // -------------------------------------------------------------
    // Task 3: Báo cáo nhịp sống Heartbeat mỗi 1000ms (CAN 0x710)
    // -------------------------------------------------------------
    static uint32_t lastHbTime = 0;
    static uint8_t  hbCounter  = 0;

    if (currentTick - lastHbTime >= 1000) {
        lastHbTime = currentTick;

        uint8_t hbFlags = (HB_INIT_OK | HB_CAN_OK | HB_SENSORS_OK);
        if (Get_WaterLevel_Status() == 0) {
            hbFlags |= HB_DTC_ACTIVE;
        }
        Bridge_SendHeartbeat(hbCounter++, hbFlags);
    }
}


