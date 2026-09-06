/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <can_messages.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

CAN_HandleTypeDef hcan;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CAN_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_ADC1_Init(void);
/* USER CODE BEGIN PFP */
// Gửi một chuỗi ký tự qua UART1.
static void UART_SendString(const char *text);
//Cấu hình một CAN filter để chỉ nhận đúng một Standard ID.
static HAL_StatusTypeDef CAN_ConfigExactStdIdFilter(uint32_t filter_bank, uint16_t std_id);
//Cấu hình toàn bộ các CAN filter dùng cho gateway.
static HAL_StatusTypeDef CAN_ConfigGatewayFilters(void);
//Gửi một CAN Standard Data Frame.
static HAL_StatusTypeDef CAN_SendStandardFrame(uint16_t std_id, const uint8_t *data, uint8_t dlc);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void UART_SendString(const char *text)
{
	/* Bảo vệ chương trình khỏi việc truy cập con trỏ NULL. */
	if (text == NULL)
		{
			return;
		}

	/*
	* Gửi toàn bộ chuỗi qua UART1.
	* Ép kiểu sang uint8_t * vì HAL_UART_Transmit() nhận vùng dữ liệu byte.
	* Giá trị trả về hiện không được kiểm tra vì hàm này chỉ được dùng
	* để xuất thông báo trạng thái và lỗi đơn giản.
	*/
    HAL_UART_Transmit(
        &huart1,
        (uint8_t *)text,
        (uint16_t)strlen(text),
        HAL_MAX_DELAY
    );
}

/**
* @brief Cấu hình filter để nhận chính xác một CAN Standard Data Frame ID.
* bxCAN sử dụng thanh ghi filter 32 bit. Đối với Standard ID:
* - 11 bit ID được đặt từ bit 15 đến bit 5 của nửa thanh ghi cao.
* - IDE nằm tại bit 2 của nửa thanh ghi thấp.
* - RTR nằm tại bit 1 của nửa thanh ghi thấp.
* 0x07FF << 5 để so sánh đầy đủ cả 11 bit ID.
* 0x0006 để kiểm tra IDE và RTR đều phải bằng 0.
* @param filter_bank Filter bank được sử dụng.
* @param std_id Standard ID cần nhận.
* @return Trạng thái trả về từ HAL_CAN_ConfigFilter().
*/
static HAL_StatusTypeDef CAN_ConfigExactStdIdFilter(uint32_t filter_bank, uint16_t std_id)
{
	/* Khởi tạo toàn bộ cấu trúc filter về 0. */
	CAN_FilterTypeDef filter = {0};

	/*
	* CAN Standard ID chỉ có 11 bit.
	* AND loại bỏ các bit không hợp lệ phía trên.
	*/
	std_id &= 0x07FFU;

	/* Chọn filter bank cần cấu hình. */
	filter.FilterBank = filter_bank;
	/* Phần FilterMaskId xác định các bit phải được kiểm tra. */
	filter.FilterMode = CAN_FILTERMODE_IDMASK;
	/* Sử dụng một filter 32 bit thay vì hai filter 16 bit. */
	filter.FilterScale = CAN_FILTERSCALE_32BIT;

	/*
	* Đưa 11 bit Standard ID vào đúng vị trí của thanh ghi filter.
	* Standard ID được dịch trái 5 bit theo định dạng bxCAN.
	*/
	filter.FilterIdHigh = (uint16_t)(std_id << 5);
	filter.FilterIdLow = 0x0000U;

	/*
	* So sánh toàn bộ 11 bit Standard ID.
	* Bit mask bằng 1 nghĩa là bit tương ứng phải khớp.
	*/
	filter.FilterMaskIdHigh = (uint16_t)(0x07FFU << 5);

	/*
	 * Bit 2 mask: IDE must be 0, standard frame.
	 * Bit 1 mask: RTR must be 0, data frame.
	 */
	filter.FilterMaskIdLow = 0x0006U;
	/* Chuyển các khung phù hợp với filter vào RX FIFO0. */
	filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
	/* Kích hoạt filter sau khi cấu hình. */
	filter.FilterActivation = ENABLE;
	/* Gửi cấu hình filter xuống ngoại vi CAN. */
	return HAL_CAN_ConfigFilter(&hcan, &filter);
}

/**
* @brief Cấu hình các CAN ID mà gateway cần tiếp nhận.
* Danh sách CAN ID vào các filter bank từ 0 đến 9.
* Nếu có bất kỳ filter nào cấu hình thất bại, hàm dừng ngay và trả về
* HAL_ERROR.
* @return HAL_OK nếu toàn bộ filter được cấu hình thành công.
*/
static HAL_StatusTypeDef CAN_ConfigGatewayFilters(void)
{
	/* Filter bank 0: nhận dữ liệu tốc độ. */
	if (CAN_ConfigExactStdIdFilter(0U, CAN_ID_SPEED) != HAL_OK)
		{
			return HAL_ERROR;
		}

	/* Filter bank 1: nhận dữ liệu mức nhiên liệu. */
	if (CAN_ConfigExactStdIdFilter(1U, CAN_ID_FUEL) != HAL_OK)
		{
			return HAL_ERROR;
		}

	/* Filter bank 2: nhận trạng thái đèn chiếu sáng. */
	if (CAN_ConfigExactStdIdFilter(2U, CAN_ID_HEADLIGHT_STATE) != HAL_OK)
		{
			return HAL_ERROR;
		}

	/* Filter bank 3: nhận tín hiệu nhịp điều khiển đèn báo rẽ. */
	if (CAN_ConfigExactStdIdFilter(3U, CAN_ID_BLINK_TICK) != HAL_OK)
		{
			return HAL_ERROR;
		}

	/* Filter bank 4: nhận trạng thái của bộ điều khiển phía trước. */
	if (CAN_ConfigExactStdIdFilter(4U, CAN_ID_FRONT_STATUS) != HAL_OK)
		{
			return HAL_ERROR;
		}

	/* Filter bank 5: nhận thông báo lỗi từ bộ điều khiển phía trước. */
	if (CAN_ConfigExactStdIdFilter(5U, CAN_ID_FRONT_ERROR) != HAL_OK)
		{
			return HAL_ERROR;
		}

	/* Filter bank 6: nhận trạng thái của bộ điều khiển phía sau. */
	if (CAN_ConfigExactStdIdFilter(6U, CAN_ID_REAR_STATUS) != HAL_OK)
		{
			return HAL_ERROR;
		}

	/* Filter bank 7: nhận dữ liệu khoảng cách phía sau. */
	if (CAN_ConfigExactStdIdFilter(7U, CAN_ID_REAR_DISTANCE) != HAL_OK)
		{
			return HAL_ERROR;
		}

	/* Filter bank 8: nhận thông báo lỗi từ bộ điều khiển phía sau. */
	if (CAN_ConfigExactStdIdFilter(8U, CAN_ID_REAR_ERROR) != HAL_OK)
		{
			return HAL_ERROR;
		}

	/* Filter bank 9: nhận thông báo lỗi chung của toàn hệ thống. */
	if (CAN_ConfigExactStdIdFilter(9U, CAN_ID_SYSTEM_ERROR) != HAL_OK)
		{
			return HAL_ERROR;
		}

	return HAL_OK;
}

/*
* @brief Tạo và gửi một CAN Standard Data Frame.
* Hàm kiểm tra DLC và con trỏ dữ liệu trước khi gọi HAL.
* @param std_id CAN Standard ID 11 bit.
* @param data Dữ liệu cần truyền.
* @param dlc Số byte dữ liệu từ 0 đến 8.
* @return Trạng thái trả về từ HAL_CAN_AddTxMessage().
*/
static HAL_StatusTypeDef CAN_SendStandardFrame(uint16_t std_id, const uint8_t *data, uint8_t dlc)
{
	/* Header mô tả khung CAN cần truyền. */
	CAN_TxHeaderTypeDef tx_header = {0};
	uint32_t tx_mailbox;

	/* Khung CAN chỉ cho phép tối đa 8 byte dữ liệu. */
	if (dlc > 8U)
		{
			return HAL_ERROR;
		}

	/*
	* Khi dlc lớn hơn 0, data phải trỏ đến vùng dữ liệu hợp lệ.
	* Khi dlc bằng 0, data có thể bằng NULL.
	*/
	if ((data == NULL) && (dlc > 0U))
		{
			return HAL_ERROR;
		}

	/* Chỉ giữ lại 11 bit hợp lệ của Standard ID. */
	tx_header.StdId = std_id & 0x07FFU;
	tx_header.ExtId = 0U;
	/* Chọn định dạng CAN Standard ID 11 bit. */
	tx_header.IDE = CAN_ID_STD;
	/* Chọn Data Frame, không phải Remote Frame. */
	tx_header.RTR = CAN_RTR_DATA;
	/* Thiết lập số byte dữ liệu trong khung CAN. */
	tx_header.DLC = dlc;
	tx_header.TransmitGlobalTime = DISABLE;

	return HAL_CAN_AddTxMessage(&hcan, &tx_header, (uint8_t *)data, &tx_mailbox);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_CAN_Init();
  MX_USART1_UART_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */
UART_SendString("\r\nCan-to-UART gateway starting...\r\n");

if (CAN_ConfigGatewayFilters() != HAL_OK)
{
	UART_SendString("CAN filter configuration error\r\n");
	Error_Handler();
}
if (HAL_CAN_Start(&hcan) != HAL_OK)
{
	UART_SendString("CAN start error\r\n");
	Error_Handler();
}
if (HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_ERROR | CAN_IT_BUSOFF) != HAL_OK)
{
	UART_SendString("CAN notification error\r\n");
	Error_Handler();
}

UART_SendString("CAN ready\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  HAL_Delay(100);
	  /*
	   * Loopback test
	  uint8_t speed_data[2] =
	  {
			  0x00,
			  0x64
	  };

	  uint8_t fuel_data[1] =
	  {
			  75U
	  };

	  uint8_t front_status_data[2] =
	  {
			  0x05,
			  0x01
	  };

	  uint8_t rear_distance_data[2] =
	  {
			  0x00,
			  0x96
	  };

	  uint8_t system_error_data[3] =
	  {
			  NODE_ID_REAR_BCM,
			  ERR_TRUNK_SENSOR,
			  0x01
	  };

	  CAN_SendStandardFrame(CAN_ID_SPEED, speed_data, 2U);
	  HAL_Delay(300);

	  CAN_SendStandardFrame(CAN_ID_FUEL, fuel_data, 1U);
	  HAL_Delay(300);

	  CAN_SendStandardFrame(CAN_ID_FRONT_STATUS, front_status_data, 2U);
	  HAL_Delay(300);

	  CAN_SendStandardFrame(CAN_ID_REAR_DISTANCE, rear_distance_data, 2U);
	  HAL_Delay(300);

	  CAN_SendStandardFrame(CAN_ID_SYSTEM_ERROR, system_error_data, 3U);
	  HAL_Delay(300);
	  */
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief CAN Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN_Init(void)
{

  /* USER CODE BEGIN CAN_Init 0 */

  /* USER CODE END CAN_Init 0 */

  /* USER CODE BEGIN CAN_Init 1 */

  /* USER CODE END CAN_Init 1 */
  hcan.Instance = CAN1;
  hcan.Init.Prescaler = 9;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_6TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_1TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = ENABLE;
  hcan.Init.AutoWakeUp = DISABLE;
  hcan.Init.AutoRetransmission = ENABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN_Init 2 */

  /* USER CODE END CAN_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

}

/* USER CODE BEGIN 4 */

/**
* @brief Callback khi RX FIFO0 có khung CAN đang chờ xử lý.
* Hàm thực hiện:
* 1. Kiểm tra callback có xuất phát từ CAN1 hay không.
* 2. Lấy một khung từ RX FIFO0.
* 3. Kiểm tra khung có phải Standard Data Frame hay không.
* 4. Chuyển thông tin khung thành chuỗi ASCII.
* 5. Gửi chuỗi qua UART1.
* Chuỗi đầu ra có dạng: CAN ID=123 DLC=3 DATA=01 02 FF
* @param phcan Con trỏ đến CAN handle đã phát sinh callback.
*/
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *phcan)
{
	/* Header chứa CAN ID, DLC, IDE, RTR và các thuộc tính khác. */
	CAN_RxHeaderTypeDef rx_header;
	/* CAN hỗ trợ tối đa 8 byte dữ liệu. */
	uint8_t rx_data[8];

	/* Bộ đệm chứa chuỗi sẽ được gửi qua UART. */
	char text[100];
	/* Số ký tự hiện đang có trong bộ đệm text. */
	int length;

	/*
	* Bỏ qua callback nếu nguồn phát sinh không phải CAN1.
	* Việc kiểm tra này giúp hàm an toàn khi hệ thống có nhiều CAN controller.
	*/
	if (phcan->Instance != CAN1)
		{
			return;
		}

	/*
	1071
	* Đọc một khung từ CAN RX FIFO0.
	* rx_header nhận thông tin header.
	* rx_data nhận tối đa 8 byte payload.
	*/
	if (HAL_CAN_GetRxMessage(phcan, CAN_RX_FIFO0, &rx_header, rx_data) != HAL_OK)
		{
			return;
		}

	/*
	* Chỉ xử lý Standard Frame có ID 11 bit.
	* Extended Frame bị bỏ qua.
	*/
	if (rx_header.IDE != CAN_ID_STD)
		{
			return;
		}

	/*
	* Chỉ xử lý Data Frame.
	* Remote Transmission Request Frame bị bỏ qua.
	*/
	if (rx_header.RTR != CAN_RTR_DATA)
		{
			return;
		}

	/*
	1104
	* Tạo phần đầu của thông báo.
	* %03lx: In Standard ID ở dạng hexadecimal với ít nhất 3 chữ số.
	* %lu: In DLC ở dạng số nguyên không dấu.
	*/
	length = snprintf(text, sizeof(text), "CAN ID=%03lx DLC=%lu DATA=", rx_header.StdId, rx_header.DLC);

	/*
	* snprintf() trả về:
	* - Giá trị âm nếu có lỗi định dạng.
	* - Số ký tự cần ghi, không bao gồm '\0'.
	* Nếu giá trị trả về lớn hơn hoặc bằng kích thước bộ đệm, chuỗi đã bị cắt nên không tiếp tục xử lý.
	*/
	if ((length < 0) || ((size_t)length >= sizeof(text)))
		{
			return;
		}

	/*
	* Thêm từng byte CAN vào chuỗi dưới dạng hexadecimal.
	* Điều kiện i < 8U bảo vệ bộ đệm rx_data nếu DLC bất thường.
	*/
	for (uint32_t i = 0; (i < rx_header.DLC) && (i < 8U); i++)
	{
		int written = snprintf(&text[length], sizeof(text) - (size_t)length, "%02X ", rx_data[i]);
		if (written < 0)
			{
				return;
			}
		if ((size_t)written >= (sizeof(text) - (size_t)length))
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
	text[length] = '\0';

	HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)length, 100U);
}

/*
* @brief Callback được gọi khi CAN phát sinh lỗi.
* Hàm đọc mã lỗi CAN từ HAL, chuyển mã lỗi thành chuỗi hexadecimal và gửi chuỗi qua UART1.
* Chuỗi đầu ra có dạng: CAN ERROR=0x00000001
* @param phcan Con trỏ đến CAN handle đã phát sinh lỗi.
*/
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *phcan)
{
	char text[64];
	int length;
	uint32_t error_code;

	if (phcan->Instance != CAN1)
		{
			return;
		}

	error_code = HAL_CAN_GetError(phcan);

	length = snprintf(text, sizeof(text), "CAN ERROR=0x%08lx\r\n", error_code);

	if ((length > 0) && ((size_t)length < sizeof(text)))
		{
			HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)length, 100U);
		}
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
