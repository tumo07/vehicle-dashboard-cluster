/*
 * uart_protocol.c
 *
 *  Created on: Sep 21, 2026
 *      Author: admin
 */

#include "uart_protocol.h"
#include "can_gateway.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

extern UART_HandleTypeDef huart1;

static uint8_t uart_rx_byte;
static char uart_rx_buffer[UART_RX_BUFFER_SIZE];

static volatile uint8_t uart_rx_index = 0U;
static volatile uint8_t uart_command_ready = 0U;
static volatile uint32_t uart_last_byte_tick = 0U;

static int HexCharToValue(char character);
static uint8_t ParseHexByte(
    const char *text,
    uint8_t *value
);

static void UART_ProcessCommand(char *command);
static void UART_ProcessTimeout(void);

void UART_Protocol_SendString(const char *text)
{
    if (text == NULL)
    {
        return;
    }

    (void)HAL_UART_Transmit(
        &huart1,
        (uint8_t *)text,
        (uint16_t)strlen(text),
        HAL_MAX_DELAY
    );
}

HAL_StatusTypeDef UART_Protocol_Init(void)
{
    uart_rx_index = 0U;
    uart_command_ready = 0U;
    uart_last_byte_tick = HAL_GetTick();

    return HAL_UART_Receive_IT(
        &huart1,
        &uart_rx_byte,
        1U
    );
}

static int HexCharToValue(char character)
{
    if ((character >= '0') &&
        (character <= '9'))
    {
        return character - '0';
    }

    character = (char)toupper(
        (unsigned char)character
    );

    if ((character >= 'A') &&
        (character <= 'F'))
    {
        return character - 'A' + 10;
    }

    return -1;
}

static uint8_t ParseHexByte(
    const char *text,
    uint8_t *value)
{
    int high_nibble;
    int low_nibble;

    if ((text == NULL) || (value == NULL))
    {
        return 0U;
    }

    high_nibble = HexCharToValue(text[0]);
    low_nibble = HexCharToValue(text[1]);

    if ((high_nibble < 0) ||
        (low_nibble < 0))
    {
        return 0U;
    }

    *value = (uint8_t)(
        ((uint8_t)high_nibble << 4U) |
        (uint8_t)low_nibble
    );

    return 1U;
}

static void UART_ProcessCommand(char *command)
{
    char *token;
    char *data_text;
    char *end_pointer;

    unsigned long parsed_id;
    unsigned long parsed_dlc;

    uint16_t can_id;
    uint8_t dlc;
    uint8_t data[8];

    HAL_StatusTypeDef status;

    if (command == NULL)
    {
        return;
    }

    token = strtok(command, ":");

    if ((token == NULL) ||
        (strcmp(token, "TX") != 0))
    {
        UART_Protocol_SendString(
            "NACK:FORMAT\r\n"
        );
        return;
    }

    token = strtok(NULL, ":");

    if (token == NULL)
    {
        UART_Protocol_SendString("NACK:ID\r\n");
        return;
    }

    end_pointer = NULL;
    parsed_id = strtoul(token, &end_pointer, 16);

    if ((end_pointer == token) ||
        (*end_pointer != '\0') ||
        (parsed_id > 0x07FFUL))
    {
        UART_Protocol_SendString("NACK:ID\r\n");
        return;
    }

    can_id = (uint16_t)parsed_id;

    token = strtok(NULL, ":");

    if (token == NULL)
    {
        UART_Protocol_SendString("NACK:DLC\r\n");
        return;
    }

    end_pointer = NULL;
    parsed_dlc = strtoul(token, &end_pointer, 10);

    if ((end_pointer == token) ||
        (*end_pointer != '\0') ||
        (parsed_dlc == 0UL) ||
        (parsed_dlc > 8UL))
    {
        UART_Protocol_SendString("NACK:DLC\r\n");
        return;
    }

    dlc = (uint8_t)parsed_dlc;

    data_text = strtok(NULL, ":");

    if (data_text == NULL)
    {
        UART_Protocol_SendString("NACK:DATA\r\n");
        return;
    }

    if (strtok(NULL, ":") != NULL)
    {
        UART_Protocol_SendString(
            "NACK:EXTRA_FIELD\r\n"
        );
        return;
    }

    if (strlen(data_text) !=
        ((size_t)dlc * 2U))
    {
        UART_Protocol_SendString(
            "NACK:LENGTH\r\n"
        );
        return;
    }

    for (uint8_t index = 0U;
         index < dlc;
         index++)
    {
        if (ParseHexByte(
                &data_text[index * 2U],
                &data[index]) == 0U)
        {
            UART_Protocol_SendString(
                "NACK:HEX\r\n"
            );
            return;
        }
    }

    status = CAN_Gateway_SendCommand(
        can_id,
        data,
        dlc
    );

    if (status == HAL_OK)
    {
        UART_Protocol_SendString("ACK:TX\r\n");
    }
    else if (status == HAL_BUSY)
    {
        UART_Protocol_SendString("NACK:BUSY\r\n");
    }
    else
    {
        UART_Protocol_SendString(
            "NACK:COMMAND\r\n"
        );
    }
}

static void UART_ProcessTimeout(void)
{
    if ((uart_command_ready == 0U) &&
        (uart_rx_index > 0U) &&
        ((HAL_GetTick() - uart_last_byte_tick) >
         UART_INTERBYTE_TIMEOUT_MS))
    {
        uart_rx_index = 0U;

        UART_Protocol_SendString(
            "NACK:UART_TIMEOUT\r\n"
        );
    }
}

void UART_Protocol_Process(void)
{
    if (uart_command_ready != 0U)
    {
        UART_ProcessCommand(uart_rx_buffer);

        uart_rx_index = 0U;
        uart_command_ready = 0U;
    }

    UART_ProcessTimeout();
}

void UART_Protocol_RxCallback(
    UART_HandleTypeDef *huart)
{
    if ((huart == NULL) ||
        (huart->Instance != USART1))
    {
        return;
    }

    uart_last_byte_tick = HAL_GetTick();

    if ((uart_rx_byte == '\r') ||
        (uart_rx_byte == '\n'))
    {
        if ((uart_rx_index > 0U) &&
            (uart_command_ready == 0U))
        {
            uart_rx_buffer[uart_rx_index] = '\0';
            uart_command_ready = 1U;
        }
    }
    else
    {
        if ((uart_command_ready == 0U) &&
            (uart_rx_index <
             (UART_RX_BUFFER_SIZE - 1U)))
        {
            uart_rx_buffer[uart_rx_index] =
                (char)uart_rx_byte;

            uart_rx_index++;
        }
        else if (uart_command_ready == 0U)
        {
            uart_rx_index = 0U;
        }
    }

    if (HAL_UART_Receive_IT(
            &huart1,
            &uart_rx_byte,
            1U) != HAL_OK)
    {
        uart_rx_index = 0U;
    }
}

void UART_Protocol_SendCanFrame(
    uint16_t std_id,
    uint8_t dlc,
    const uint8_t *data)
{
    char text[64];
    int length;

    if ((data == NULL) || (dlc > 8U))
    {
        return;
    }

    length = snprintf(
        text,
        sizeof(text),
        "CAN:%03X:%u:",
        (unsigned int)std_id,
        (unsigned int)dlc
    );

    if ((length < 0) ||
        ((size_t)length >= sizeof(text)))
    {
        return;
    }

    for (uint8_t index = 0U;
         index < dlc;
         index++)
    {
        int written;

        written = snprintf(
            &text[length],
            sizeof(text) - (size_t)length,
            "%02X",
            data[index]
        );

        if ((written < 0) ||
            ((size_t)written >=
             (sizeof(text) - (size_t)length)))
        {
            return;
        }

        length += written;
    }

    if (((size_t)length + 2U) >= sizeof(text))
    {
        return;
    }

    text[length++] = '\r';
    text[length++] = '\n';

    (void)HAL_UART_Transmit(
        &huart1,
        (uint8_t *)text,
        (uint16_t)length,
        100U
    );
}
