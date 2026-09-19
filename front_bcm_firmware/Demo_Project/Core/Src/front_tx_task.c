/*
 * front_tx_task.c
 * Front BCM - Periodic TX Tasks Implementation
 */

#include <TX_data_UART.h>
#include "front_tx_task.h"
#include "can_messages.h"
#include "wiper.h"

// Khai báo các hàm ngoại vi lấy dữ liệu từ module khác
extern uint8_t Get_RainSensor_Percent(void);
extern uint8_t Get_WaterLevel_Percent(void);
extern CmdWiper_t Servo_GetWiperMode(void);

void Front_TX_Task_Init(void) {
    // Khởi tạo ban đầu nếu cần
}

void Front_BCM_Periodic_TX_Task(void) {
    uint32_t currentTick = HAL_GetTick();

    // 1. Heartbeat Task (Chạy mỗi 100ms) - CAN ID 0x710U
    static uint32_t lastHbTime = 0;
    static uint8_t hbCounter = 0;
    if (currentTick - lastHbTime >= 100) {
        lastHbTime = currentTick;
        // Cờ trạng thái: Init OK, CAN OK, Sensors OK
        uint8_t hbFlags = HB_INIT_OK | HB_CAN_OK | HB_SENSORS_OK;
        Bridge_SendHeartbeat(hbCounter++, hbFlags);
    }

    // 2. Front Status Report Task (Chạy mỗi 200ms) - CAN ID 0x400U
    static uint32_t lastStatusTime = 0;
    if (currentTick - lastStatusTime >= 200) {
        lastStatusTime = currentTick;

        uint8_t actuatorFlags = 0; // Cờ trạng thái thực tế các thiết bị (FRONT_ACT_...)
        uint8_t wiperMode = (uint8_t)Servo_GetWiperMode();
        uint8_t motorHealth = 100;  // 100% sức khỏe motor gạt mưa

        Bridge_SendStatusReport(actuatorFlags, wiperMode, motorHealth);
    }

    // 3. Front Sensors Report Task (Chạy mỗi 500ms) - CAN ID 0x401U
    static uint32_t lastSensorTime = 0;
    if (currentTick - lastSensorTime >= 500) {
        lastSensorTime = currentTick;

        uint8_t rainPercent = Get_RainSensor_Percent();   // Byte 0: % Lượng mưa
        uint8_t waterPercent = Get_WaterLevel_Percent(); // Byte 1: % Mức nước rửa kính từ chân PE9

        Bridge_SendSensorReport(rainPercent, waterPercent);
    }
}
