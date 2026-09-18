/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  * @project        : Smart Vehicle Dashboard Cluster
  * @node           : Central ECU (Blue Pill STM32F103C8T6)
  * @version        : CAN v3.0 — Full Coordinator Architecture
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN Private defines */

/* --- CAN Pins (AFIO remap 2 - PB8/PB9) ------------------------------------- */
#define CAN_RX_PIN          GPIO_PIN_8
#define CAN_RX_PORT         GPIOB
#define CAN_TX_PIN          GPIO_PIN_9
#define CAN_TX_PORT         GPIOB

/* --- UART1 Pins ------------------------------------------------------------- */
#define UART1_TX_PIN        GPIO_PIN_9
#define UART1_TX_PORT       GPIOA
#define UART1_RX_PIN        GPIO_PIN_10
#define UART1_RX_PORT       GPIOA

/* --- ADC Analog Input Pins -------------------------------------------------- */
#define ADC_SPEED_PIN       GPIO_PIN_0     /* PA0 - ADC1 CH0 - speed potentiometer */
#define ADC_SPEED_PORT      GPIOA
#define ADC_FUEL_PIN        GPIO_PIN_1     /* PA1 - ADC1 CH1 - fuel potentiometer  */
#define ADC_FUEL_PORT       GPIOA

/* --- Headlight Toggle Button ------------------------------------------------ */
#define BTN_HEADLIGHT_PIN   GPIO_PIN_2     /* PA2 - active HIGH, pull-down */
#define BTN_HEADLIGHT_PORT  GPIOA

/* --- Onboard LED (CAN TX activity indicator, active LOW) ------------------- */
#define LED_CAN_TX_PIN      GPIO_PIN_13   /* PC13 */
#define LED_CAN_TX_PORT     GPIOC

/* --- Speed-zone LEDs ------------------------------------------------------- */
#define LED_GREEN_PIN       GPIO_PIN_0    /* PB0  - speed < 60 km/h  */
#define LED_GREEN_PORT      GPIOB
#define LED_ORANGE_PIN      GPIO_PIN_1    /* PB1  - 60..99 km/h      */
#define LED_ORANGE_PORT     GPIOB
#define LED_RED_PIN         GPIO_PIN_12   /* PB12 - speed >= 100 km/h */
#define LED_RED_PORT        GPIOB

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
