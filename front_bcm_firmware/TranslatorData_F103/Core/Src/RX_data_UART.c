/*
 * RX_data_UART.c
 * Translator Node (STM32F103)
 */

#include "RX_data_UART.h"
#include "can_messages.h"
#include "TX_data_CAN.h"

static uint8_t uartRxByte;
static UART_HandleTypeDef *pUartHandle = NULL;

volatile uint8_t f103_wiperStatus = 0;
volatile uint8_t f103_turnStatus  = 0;

static uint8_t rx_buffer[8];
static uint8_t rx_index = 0;
static uint8_t expected_len = 0;

extern CAN_HandleTypeDef hcan;

void F103_Bridge_Init(UART_HandleTypeDef *huart) {
    pUartHandle  = huart;
    rx_index     = 0;
    expected_len = 0;
    HAL_UART_Receive_IT(pUartHandle, &uartRxByte, 1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (pUartHandle != NULL && huart->Instance == pUartHandle->Instance) {
        
        static uint32_t last_rx_time = 0;
        uint32_t current_time = HAL_GetTick();
        if (current_time - last_rx_time > 25) {
            rx_index = 0; // Timeout reset (25ms) to prevent permanent desync without cutting off packets!
        }
        last_rx_time = current_time;

        if (rx_index == 0) {
            switch (uartRxByte) {
                case 0x71: // Heartbeat (3 bytes)
                    expected_len = 3;
                    rx_buffer[rx_index++] = uartRxByte;
                    break;

                case 0x40: // Status định kỳ (4 bytes)
                    expected_len = 4;
                    rx_buffer[rx_index++] = uartRxByte;
                    break;

                case 0x41: // Sensors định kỳ (3 bytes)
                    expected_len = 3;
                    rx_buffer[rx_index++] = uartRxByte;
                    break;

                case 0xEE: // Fault DTC (6 bytes)
                    expected_len = 6;
                    rx_buffer[rx_index++] = uartRxByte;
                    break;

                case 0x55: // Sự kiện nút bấm (4 bytes: 0x55 + Wiper + Turn + Cks)
                    expected_len = 4;
                    rx_buffer[rx_index++] = uartRxByte;
                    break;

                default:
                    rx_index = 0;
                    expected_len = 0;
                    break;
            }
        }
        else {
            rx_buffer[rx_index++] = uartRxByte;

            if (rx_index >= expected_len) {
                switch (rx_buffer[0]) {
                    case 0x71: { // Gói 0x710
                        uint8_t payload[2] = { rx_buffer[1], rx_buffer[2] };
                        CAN_Transmit_Direct(&hcan, CAN_ID_HEARTBEAT_FRONT_BCM, 2, payload);
                        break;
                    }

                    case 0x40: { // Gói 0x400 định kỳ
                        uint8_t payload[3] = { rx_buffer[1], rx_buffer[2], rx_buffer[3] };
                        CAN_Transmit_Direct(&hcan, CAN_ID_REPORT_FRONT_STATUS, 3, payload);
                        break;
                    }

                    case 0x41: { // Gói 0x401 định kỳ
                        uint8_t payload[2] = { rx_buffer[1], rx_buffer[2] };
                        CAN_Transmit_Direct(&hcan, CAN_ID_REPORT_FRONT_SENSORS, 2, payload);
                        break;
                    }

                    case 0xEE: { // Gói 0x500 lỗi DTC
                        uint8_t payload[5] = { rx_buffer[1], rx_buffer[2], rx_buffer[3], rx_buffer[4], rx_buffer[5] };
                        CAN_Transmit_Direct(&hcan, CAN_ID_FAULT_FRONT_BCM, 5, payload);
                        break;
                    }

                    case 0x55: { // BẮN CAN 0x400 TỨC THÌ KHI CÓ SỰ KIỆN NÚT BẤM
                        f103_wiperStatus = rx_buffer[1];
                        f103_turnStatus  = rx_buffer[2];

                        uint8_t payload[3] = {0, f103_wiperStatus, 100};
                        if (f103_turnStatus == 1)      payload[0] |= FRONT_ACT_LTURN;
                        else if (f103_turnStatus == 2) payload[0] |= FRONT_ACT_RTURN;
                        else if (f103_turnStatus == 3) payload[0] |= (FRONT_ACT_LTURN | FRONT_ACT_RTURN);
                        if (f103_wiperStatus > 0)      payload[0] |= FRONT_ACT_WIPER;

                        CAN_Transmit_Direct(&hcan, CAN_ID_REPORT_FRONT_STATUS, 3, payload);
                        break;
                    }

                    default:
                        break;
                }

                rx_index = 0;
                expected_len = 0;
            }
        }

        // Tự động giải phóng ngắt nếu dính lỗi Overrun do dữ liệu dồn về
        if (__HAL_UART_GET_FLAG(pUartHandle, UART_FLAG_ORE) != RESET) {
            __HAL_UART_CLEAR_OREFLAG(pUartHandle);
        }

        // Lắng nghe byte kế tiếp
        HAL_UART_Receive_IT(pUartHandle, &uartRxByte, 1);
    }
}

