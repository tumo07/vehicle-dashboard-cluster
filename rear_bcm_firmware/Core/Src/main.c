/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body - Rear BCM Node 0x03
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "can_messages.h"

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan;
TIM_HandleTypeDef htim3; // Dùng để delay micro-giây cho HC-SR04
TIM_HandleTypeDef htim4; // Dùng cho Servo Cốp xe (PB6)

/* USER CODE BEGIN PV */
CAN_TxHeaderTypeDef TxHeader;
uint32_t TxMailbox;
uint8_t TxData[8];
// ----- BIẾN LƯU LỆNH TỪ CENTRAL ECU -----
volatile uint8_t exec_rear_turn_mask = 0;
// ----- BIẾN CHO CỐP XE -----
uint32_t last_kick_time = 0;
uint8_t trunk_state = 0;     // 0 = Cốp đóng, 1 = Cốp mở
uint8_t foot_in_zone = 0;    // Cờ chống nhận diện liên tục khi để tay lâu

// Biến cho thuật toán chạy Servo mượt (Sweep)
uint16_t current_servo_pwm = 500; // Bắt đầu ở 0.5ms (0 độ)
uint16_t target_servo_pwm = 500;
uint32_t last_servo_move = 0;
// Biến trạng thái vật lý
volatile uint8_t brake_pedal = 0;
volatile uint8_t trunk_switch = 0;
uint8_t last_trunk_switch = 0;
volatile uint8_t is_reversing = 0;
volatile uint8_t central_is_reversing = 0;
volatile uint8_t left_turn = 0;
volatile uint8_t right_turn = 0;

// Biến cho logic Non-blocking
uint32_t last_can_tx = 0;
uint32_t last_beep_time = 0;
uint16_t distance_cm = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CAN_Init(void);
static void MX_TIM4_Init(void);
static void MX_TIM3_Init(void);

/* USER CODE BEGIN 0 */
// Hàm delay theo micro-giây bằng TIM3
void Delay_us(uint16_t us) {
    __HAL_TIM_SET_COUNTER(&htim3, 0);
    while (__HAL_TIM_GET_COUNTER(&htim3) < us);
}

// Hàm đọc cảm biến siêu âm HC-SR04 an toàn (có chống treo chip)
uint16_t HCSR04_Read(void) {
    uint32_t local_time = 0;

    // Kích xung Trigger 10us
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
    Delay_us(10);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);

    // Chờ chân Echo lên mức Cao (Có Timeout để chống treo mạch)
    uint32_t timeout = HAL_GetTick();
    while (!(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13))) {
        if (HAL_GetTick() - timeout > 10) return 0; // Quá 10ms không thấy sóng dội
    }

    // Bắt đầu đo độ rộng xung Echo
    __HAL_TIM_SET_COUNTER(&htim3, 0);
    while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13)) {
        local_time = __HAL_TIM_GET_COUNTER(&htim3);
        if (local_time > 25000) return 400; // Tối đa ~4 mét, thoát sớm
    }

    // Công thức: Khoảng cách (cm) = Thời gian (us) / 58
    return local_time / 58;
}

// Hàm gửi yêu cầu điều khiển CỐP lên Central ECU (CAN ID 0x103)
void Rear_RequestTrunkAction(CmdTrunk_t req_cmd) {
    CAN_TxHeaderTypeDef reqHeader;
    uint32_t reqMailbox;
    uint8_t reqData[1];

    reqHeader.StdId = CAN_ID_CMD_TRUNK_CONTROL; // 0x103
    reqHeader.ExtId = 0x00;
    reqHeader.RTR   = CAN_RTR_DATA;
    reqHeader.IDE   = CAN_ID_STD;
    reqHeader.DLC   = 1;
    reqHeader.TransmitGlobalTime = DISABLE;

    reqData[0] = (uint8_t)req_cmd;

    HAL_CAN_AddTxMessage(&hcan, &reqHeader, reqData, &reqMailbox);
}
/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_CAN_Init();
  MX_TIM4_Init();
  MX_TIM3_Init();

  /* USER CODE BEGIN 2 */
  // Khởi động Timer
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, 500); // 1.0ms - Mặc định Cốp đóng
  HAL_TIM_Base_Start(&htim3); // Bật TIM3 chạy ngầm cho HC-SR04

  // Cấu hình Bộ lọc CAN
  CAN_FilterTypeDef canfilterconfig;
  canfilterconfig.FilterActivation = CAN_FILTER_ENABLE;
  canfilterconfig.FilterBank = 0;
  canfilterconfig.FilterFIFOAssignment = CAN_FILTER_FIFO0;
  canfilterconfig.FilterIdHigh = 0x0000;
  canfilterconfig.FilterIdLow = 0x0000;
  canfilterconfig.FilterMaskIdHigh = 0x0000;
  canfilterconfig.FilterMaskIdLow = 0x0000;
  canfilterconfig.FilterMode = CAN_FILTERMODE_IDMASK;
  canfilterconfig.FilterScale = CAN_FILTERSCALE_32BIT;
  HAL_CAN_ConfigFilter(&hcan, &canfilterconfig);
  HAL_CAN_Start(&hcan);
  // DÒNG NÀY ĐỂ CHO PHÉP CHIP NGHE MẠNG CAN:
    HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
  /* USER CODE END 2 */

  while (1)
  {
      /* 1. ĐỌC TÍN HIỆU NGÕ VÀO VẬT LÝ */
      brake_pedal = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);  // Bàn đạp phanh
      trunk_switch = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1); // Công tắc cốp
      is_reversing = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4); // Nút giả lập Số Lùi
      left_turn = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5);    // Nút giả lập Xi-nhan Trái
      right_turn = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6);   // Nút giả lập Xi-nhan Phải

      /* 2. ĐIỀU KHIỂN ĐÈN PHANH / XI-NHAN (KIỂU MỸ) */
      if (brake_pedal == GPIO_PIN_SET) {
          // Bật đèn phanh độc lập (nếu có gắn)
          HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);

          // Ép cả 2 đèn xi-nhan sáng rực để làm đèn phanh
          HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET); // Xi-nhan Trái (PB1)
          HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_SET); // Xi-nhan Phải (PB3)
      } else {
          // Tắt đèn phanh độc lập
          HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);

          // Nhường lại quyền điều khiển cho công tắc xi-nhan
          //HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, left_turn ? GPIO_PIN_SET : GPIO_PIN_RESET);
          //HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, right_turn ? GPIO_PIN_SET : GPIO_PIN_RESET);
      }

      /* 3. LOGIC CẢM BIẾN LÙI VÀ ĐÁ CỐP THÔNG MINH */
      uint32_t current_time = HAL_GetTick();
      distance_cm = HCSR04_Read();

      uint8_t effective_reversing = is_reversing || central_is_reversing;

      if (effective_reversing == 1) {
          // --- NGỮ CẢNH 1: ĐANG LÙI XE (PARKING SENSOR) ---
          if (distance_cm > 0 && distance_cm <= 20) {
              HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
          }
          else if (distance_cm > 20 && distance_cm <= 100) {
              uint32_t beep_interval = distance_cm * 5;
              if (current_time - last_beep_time >= beep_interval) {
                  HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_2);
                  last_beep_time = current_time;
              }
          }
          else {
              HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET);
          }
      }
      else {
          // --- NGỮ CẢNH 2: SMART KICK TRUNK (CHỈ GỬI REQUEST, KHÔNG TỰ QUAY SERVO) ---
          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET);

          if (distance_cm > 2 && distance_cm <= 8) {
              // CHỐNG NHIỄU: Chỉ nhận diện khi rút chân/tay ra rồi mới đưa vào lại (foot_in_zone == 0)
              if (foot_in_zone == 0 && (current_time - last_kick_time > 2000)) {
                  // Gửi yêu cầu đóng/mở cốp lên Central ECU (0x103) để Central ECU xét duyệt an toàn
                  Rear_RequestTrunkAction(trunk_state == 1 ? CMD_TRUNK_CLOSE : CMD_TRUNK_OPEN);
                  foot_in_zone = 1;
                  last_kick_time = current_time;
              }
          } else {
              foot_in_zone = 0;
          }
      }

      // --- NÚT BẤM CỐP VẬT LÝ TRÊN XE (PA1) ---
      if (trunk_switch == GPIO_PIN_SET && last_trunk_switch == GPIO_PIN_RESET) {
          // Khi bấm nút mở cốp vật lý -> Gửi yêu cầu lên Central ECU phê duyệt (0x103)
          Rear_RequestTrunkAction(trunk_state == 1 ? CMD_TRUNK_CLOSE : CMD_TRUNK_OPEN);
      }
      last_trunk_switch = trunk_switch;

      // --- THUẬT TOÁN QUÉT MƯỢT SERVO (NON-BLOCKING SWEEP) ---
      // Nếu vị trí hiện tại chưa tới mục tiêu do Central ECU chỉ định, nhích từng bước một
      if (current_servo_pwm != target_servo_pwm) {
          if (current_time - last_servo_move >= 5) { // Cứ 5ms nhích 1 lần

              if (current_servo_pwm < target_servo_pwm) {
                  current_servo_pwm += 5; // Tăng dần PWM để mở chậm
                  if (current_servo_pwm > target_servo_pwm) current_servo_pwm = target_servo_pwm;
              } else {
                  current_servo_pwm -= 5; // Giảm dần PWM để đóng chậm
                  if (current_servo_pwm < target_servo_pwm) current_servo_pwm = target_servo_pwm;
              }

              // Bơm từ từ độ rộng xung ra chân PB6
              __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, current_servo_pwm);
              last_servo_move = current_time;
          }
      }

      /* 4. TRUYỀN GÓI TIN CAN CHUẨN ĐỒ ÁN V3.0 (CHU KỲ 200ms) */
      if (current_time - last_can_tx >= 200) {

          // GÓI 1: 0x410 - TRẠNG THÁI NÚT BẤM & ĐỘNG CƠ CỐP
          TxHeader.StdId = CAN_ID_REPORT_REAR_STATUS; // 0x410
          TxHeader.ExtId = 0x01;
          TxHeader.RTR = CAN_RTR_DATA;
          TxHeader.IDE = CAN_ID_STD;
          TxHeader.DLC = 3; // DLC 3 Byte
          TxHeader.TransmitGlobalTime = DISABLE;

          // Byte 0: Mặt nạ nút bấm & hoạt động cốp
          TxData[0] = 0x00;
          if (brake_pedal == GPIO_PIN_SET) TxData[0] |= REAR_ACT_BRAKE;
          if (left_turn == GPIO_PIN_SET)   TxData[0] |= REAR_ACT_LTURN;
          if (right_turn == GPIO_PIN_SET)  TxData[0] |= REAR_ACT_RTURN;
          if (current_servo_pwm != target_servo_pwm) TxData[0] |= REAR_ACT_TRUNK_ACTIVE;

          // Byte 1: Trạng thái động cơ (Đang đứng yên hay đang di chuyển)
          if (current_servo_pwm < target_servo_pwm) TxData[1] = TRUNK_OPENING;
          else if (current_servo_pwm > target_servo_pwm) TxData[1] = TRUNK_CLOSING;
          else TxData[1] = TRUNK_IDLE;

          // Byte 2: % Độ mở cốp xe (0% - 100%) tính theo PWM thực tế
          uint8_t open_pct = 0;
          if (current_servo_pwm >= 500) {
              open_pct = (uint8_t)(((uint32_t)(current_servo_pwm - 500) * 100U) / 2000U);
              if (open_pct > 100) open_pct = 100;
          }
          TxData[2] = open_pct;

          HAL_CAN_AddTxMessage(&hcan, &TxHeader, TxData, &TxMailbox);

          // GÓI 2: 0x411 - KHOẢNG CÁCH SIÊU ÂM (Gửi 4 Byte)
          TxHeader.StdId = CAN_ID_REPORT_REAR_SENSORS; // 0x411
          TxHeader.DLC = 4; // DLC 4 Byte

          // Byte 0 & 1: Khoảng cách cm (Dùng Macro PACK_U16)
          TxData[0] = UNPACK_HIGH_BYTE(distance_cm);
          TxData[1] = UNPACK_LOW_BYTE(distance_cm);

          // Byte 2: Trạng thái chốt vật lý (Hall effect: 1=Mở, 0=Đóng)
          TxData[2] = (current_servo_pwm > 500) ? 1 : 0;

          // Byte 3: Tính toán mức độ đỗ xe an toàn (Xanh/Vàng/Đỏ)
          TxData[3] = effective_reversing ? CALC_PARKING_LEVEL(distance_cm) : PARKING_CLEAR;

          HAL_CAN_AddTxMessage(&hcan, &TxHeader, TxData, &TxMailbox);


                            last_can_tx = current_time;
                            // GÓI 3: 0x720 - REAR BCM HEARTBEAT
                            TxHeader.StdId = CAN_ID_HEARTBEAT_REAR_BCM;
                            TxHeader.DLC = 2;
                            static uint8_t hb_counter = 0;
                            TxData[0] = hb_counter++; // Tăng dần mỗi chu kỳ để báo trạng thái hoạt động liên tục
                            TxData[1] = HB_INIT_OK | HB_CAN_OK | HB_SENSORS_OK;
                            HAL_CAN_AddTxMessage(&hcan, &TxHeader, TxData, &TxMailbox);
                        }
  }
}

/**
  * @brief System Clock Configuration
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM3 Initialization Function (Delay Microgiây)
  */
static void MX_TIM3_Init(void)
{
__HAL_RCC_TIM3_CLK_ENABLE();
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 35; // 36MHz / (35+1) = 1MHz -> 1 tick = 1 us
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 0xFFFF;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief CAN Initialization Function
  */
static void MX_CAN_Init(void)
{
  hcan.Instance = CAN1;
  hcan.Init.Prescaler = 4;
  //hcan.Init.Mode = CAN_MODE_LOOPBACK;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_15TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = DISABLE;
  hcan.Init.AutoWakeUp = DISABLE;
  hcan.Init.AutoRetransmission = DISABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM4 Initialization Function (PWM Servo Cốp)
  */
static void MX_TIM4_Init(void)
{
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 35;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 19999;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_TIM_MspPostInit(&htim4);
}

/**
  * @brief GPIO Initialization Function
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_3|GPIO_PIN_12, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);

  /* Cấu hình 5 Ngõ vào: Phanh (PA0), Cốp (PA1), Lùi (PA4), Xi-nhan Trái (PA5), Xi-nhan Phải (PA6) */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* Cấu hình Ngõ ra: Còi/LED Báo Lùi (PA2) */
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* Cấu hình Ngõ ra: Đèn Phanh (PB0), Xi-nhan Trái (PB1), Xi-nhan Phải (PB3), Trigger Siêu âm (PB12) */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_3|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* Cấu hình Ngõ vào: Echo Siêu âm (PB13) */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}
/* USER CODE BEGIN 4 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef RxHeader;
    uint8_t RxData[8];

    // Đọc gói tin từ trong hòm thư (FIFO0) ra
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK)
    {
        // 1. Nhận nhịp chớp đồng bộ từ Central ECU (0x130)
        if (RxHeader.StdId == CAN_ID_BLINK_TICK)
        {
            // Nháy xi-nhan TRÁI dựa vào lệnh của Main ECU (Trái hoặc Hazard)
            if ((exec_rear_turn_mask & EXEC_TURN_LEFT_ARM) || (exec_rear_turn_mask & EXEC_TURN_HAZARD_ARM))
                HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_1);
            else
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);

            // Nháy xi-nhan PHẢI dựa vào lệnh của Main ECU (Phải hoặc Hazard)
            if ((exec_rear_turn_mask & EXEC_TURN_RIGHT_ARM) || (exec_rear_turn_mask & EXEC_TURN_HAZARD_ARM))
                HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_3);
            else
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);
        }

        // 2. Nhận lệnh điều khiển CỐP từ Central ECU (0x211)
        else if (RxHeader.StdId == CAN_ID_EXEC_REAR_TRUNK)
        {
            if (RxData[0] == CMD_TRUNK_OPEN)
            {
                trunk_state = 1;
                target_servo_pwm = 2500; // Mở 180 độ
            }
            else if (RxData[0] == CMD_TRUNK_CLOSE)
            {
                trunk_state = 0;
                target_servo_pwm = 500;  // Đóng 0 độ
            }
            else if (RxData[0] == CMD_TRUNK_STOP)
            {
                target_servo_pwm = current_servo_pwm; // Dừng ngay vị trí hiện tại
            }
        }

        // 3. Nhận lệnh điều khiển XI-NHAN/HAZARD từ Central ECU (0x210)
        else if (RxHeader.StdId == CAN_ID_EXEC_REAR_TURN)
        {
            exec_rear_turn_mask = RxData[0]; // Lưu lại bitmask Main ECU gửi để nháy ở (1)
        }

        // 4. Nhận trạng thái số và cờ vận hành từ Central ECU (0x300)
        else if (RxHeader.StdId == CAN_ID_STATUS_VEHICLE_STATE && RxHeader.DLC >= 2)
        {
            uint8_t current_gear = RxData[0];
            uint8_t state_flags  = RxData[1];
            central_is_reversing = (current_gear == GEAR_REVERSE) || (state_flags & STATE_REVERSE_ACTIVE);
        }

    }
}
/* USER CODE END 4 */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */
