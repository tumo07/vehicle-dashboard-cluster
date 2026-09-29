/*
 * can_gateway.c
 *
 *  Created on: Sep 21, 2026
 *      Author: admin
 */

#include "can_gateway.h"
#include "can_messages.h"
#include "uart_protocol.h"

#include <stdio.h>

extern CAN_HandleTypeDef hcan;

static volatile uint32_t can_error_flags = 0U;
static volatile uint8_t can_error_pending = 0U;

static HAL_StatusTypeDef CAN_ConfigExactStdIdFilter(
    uint32_t filter_bank,
    uint16_t std_id
);

static HAL_StatusTypeDef CAN_ConfigGatewayFilters(void);

static uint8_t CAN_IsAllowedCommandId(
    uint16_t std_id
);

static uint8_t CAN_ValidateCommand(
    uint16_t std_id,
    const uint8_t *data,
    uint8_t dlc
);

static HAL_StatusTypeDef CAN_ConfigExactStdIdFilter(
    uint32_t filter_bank,
    uint16_t std_id)
{
    CAN_FilterTypeDef filter = {0};

    if (filter_bank > 13U)
    {
        return HAL_ERROR;
    }

    std_id &= 0x07FFU;

    filter.FilterBank = filter_bank;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;

    filter.FilterIdHigh =
        (uint16_t)(std_id << 5);

    filter.FilterIdLow = 0x0000U;

    filter.FilterMaskIdHigh =
        (uint16_t)(0x07FFU << 5);

    /*
     * IDE = 0: Standard frame.
     * RTR = 0: Data frame.
     */
    filter.FilterMaskIdLow = 0x0006U;

    filter.FilterFIFOAssignment =
        CAN_FILTER_FIFO0;

    filter.FilterActivation = ENABLE;

    return HAL_CAN_ConfigFilter(
        &hcan,
        &filter
    );
}

static HAL_StatusTypeDef CAN_ConfigGatewayFilters(void)
{
    static const uint16_t accepted_ids[] =
    {
        CAN_ID_CMD_ACK,

        CAN_ID_STATUS_VEHICLE_STATE,
        CAN_ID_STATUS_LIGHTS_STATE,
        CAN_ID_STATUS_TURN_BLINK,
        CAN_ID_STATUS_TRUNK_STATE,
        CAN_ID_STATUS_REVERSE_RADAR,
        CAN_ID_STATUS_DIAG_RESPONSE,

        CAN_ID_BANNER_FAULT,
        CAN_ID_BANNER_CLEAR,
        CAN_ID_WATCHDOG_ALERT,

        CAN_ID_HEARTBEAT_CENTRAL,
        CAN_ID_HEARTBEAT_FRONT_BCM,
        CAN_ID_HEARTBEAT_REAR_BCM
    };

    const uint32_t id_count =
        sizeof(accepted_ids) /
        sizeof(accepted_ids[0]);

    for (uint32_t index = 0U;
         index < id_count;
         index++)
    {
        if (CAN_ConfigExactStdIdFilter(
                index,
                accepted_ids[index]) != HAL_OK)
        {
            return HAL_ERROR;
        }
    }

    return HAL_OK;
}

static uint8_t CAN_IsAllowedCommandId(
    uint16_t std_id)
{
    switch (std_id)
    {
        case CAN_ID_CMD_LIGHT_CONTROL:
        case CAN_ID_CMD_WIPER_CONTROL:
        case CAN_ID_CMD_TURN_SIGNAL:
        case CAN_ID_CMD_TRUNK_CONTROL:
        case CAN_ID_CMD_DIAGNOSTIC:
            return 1U;

        default:
            return 0U;
    }
}

static uint8_t CAN_ValidateCommand(
    uint16_t std_id,
    const uint8_t *data,
    uint8_t dlc)
{
    if (data == NULL)
    {
        return 0U;
    }

    switch (std_id)
    {
        case CAN_ID_CMD_LIGHT_CONTROL:

            if (dlc != 2U)
            {
                return 0U;
            }

            if (data[0] > CMD_LIGHT_AUTO_MODE)
            {
                return 0U;
            }

            if (data[1] > 100U)
            {
                return 0U;
            }

            return 1U;

        case CAN_ID_CMD_WIPER_CONTROL:

            if (dlc != 2U)
            {
                return 0U;
            }

            if (data[0] > CMD_WIPER_AUTO)
            {
                return 0U;
            }

            if (data[1] > 1U)
            {
                return 0U;
            }

            return 1U;

        case CAN_ID_CMD_TURN_SIGNAL:

            if (dlc != 1U)
            {
                return 0U;
            }

            if (data[0] > CMD_TURN_HAZARD)
            {
                return 0U;
            }

            return 1U;

        case CAN_ID_CMD_TRUNK_CONTROL:

            if (dlc != 1U)
            {
                return 0U;
            }

            if (data[0] > CMD_TRUNK_STOP)
            {
                return 0U;
            }

            return 1U;

        case CAN_ID_CMD_DIAGNOSTIC:

            if ((dlc < 2U) || (dlc > 8U))
            {
                return 0U;
            }

            if ((data[0] < DIAG_CMD_READ_ALL_DTCS) ||
                (data[0] > DIAG_CMD_NODE_STATUS))
            {
                return 0U;
            }

            return 1U;

        default:
            return 0U;
    }
}

HAL_StatusTypeDef CAN_Gateway_Init(void)
{
    if (CAN_ConfigGatewayFilters() != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (HAL_CAN_Start(&hcan) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (HAL_CAN_ActivateNotification(
            &hcan,
            CAN_IT_RX_FIFO0_MSG_PENDING |
            CAN_IT_RX_FIFO0_OVERRUN |
            CAN_IT_ERROR |
            CAN_IT_BUSOFF |
            CAN_IT_ERROR_WARNING |
            CAN_IT_ERROR_PASSIVE) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

HAL_StatusTypeDef CAN_Gateway_SendCommand(
    uint16_t std_id,
    const uint8_t *data,
    uint8_t dlc)
{
    CAN_TxHeaderTypeDef tx_header = {0};
    uint32_t tx_mailbox;

    if (CAN_IsAllowedCommandId(std_id) == 0U)
    {
        return HAL_ERROR;
    }

    if (CAN_ValidateCommand(
            std_id,
            data,
            dlc) == 0U)
    {
        return HAL_ERROR;
    }

    if (HAL_CAN_GetTxMailboxesFreeLevel(
            &hcan) == 0U)
    {
        return HAL_BUSY;
    }

    tx_header.StdId = std_id;
    tx_header.ExtId = 0U;
    tx_header.IDE = CAN_ID_STD;
    tx_header.RTR = CAN_RTR_DATA;
    tx_header.DLC = dlc;
    tx_header.TransmitGlobalTime = DISABLE;

    return HAL_CAN_AddTxMessage(
        &hcan,
        &tx_header,
        (uint8_t *)data,
        &tx_mailbox
    );
}

void CAN_Gateway_RxCallback(
    CAN_HandleTypeDef *phcan)
{
    CAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[8];

    if ((phcan == NULL) ||
        (phcan->Instance != CAN1))
    {
        return;
    }

    while (HAL_CAN_GetRxFifoFillLevel(
               phcan,
               CAN_RX_FIFO0) > 0U)
    {
        if (HAL_CAN_GetRxMessage(
                phcan,
                CAN_RX_FIFO0,
                &rx_header,
                rx_data) != HAL_OK)
        {
            break;
        }

        if ((rx_header.IDE != CAN_ID_STD) ||
            (rx_header.RTR != CAN_RTR_DATA) ||
            (rx_header.DLC > 8U))
        {
            continue;
        }

        UART_Protocol_SendCanFrame(
            (uint16_t)rx_header.StdId,
            (uint8_t)rx_header.DLC,
            rx_data
        );
    }
}

void CAN_Gateway_ErrorCallback(
    CAN_HandleTypeDef *phcan)
{
    if ((phcan == NULL) ||
        (phcan->Instance != CAN1))
    {
        return;
    }

    can_error_flags |= HAL_CAN_GetError(phcan);
    can_error_pending = 1U;
}

void CAN_Gateway_Process(void)
{
    uint32_t error_code;
    char text[48];
    int length;

    if (can_error_pending == 0U)
    {
        return;
    }

    __disable_irq();

    error_code = can_error_flags;
    can_error_flags = 0U;
    can_error_pending = 0U;

    __enable_irq();

    length = snprintf(
        text,
        sizeof(text),
        "ERR:CAN:%08lX\r\n",
        error_code
    );

    if ((length > 0) &&
        ((size_t)length < sizeof(text)))
    {
        UART_Protocol_SendString(text);
    }
}
