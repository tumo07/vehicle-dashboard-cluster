/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Central ECU firmware - Smart Vehicle Dashboard Cluster
  * @project        : Smart Vehicle Dashboard Cluster
  * @node           : Central ECU (Blue Pill STM32F103C8T6)
  * @version        : CAN v3.0 - Full Coordinator Architecture
  *
  * ROLE: Sole coordinator and decision-maker on the CAN bus.
  *   - Receives commands from Qt dashboard (GROUP A 0x10x)
  *   - Validates against vehicle state machine
  *   - Sends approved commands to Front/Rear BCMs (GROUP B 0x2xx)
  *   - Broadcasts real-time vehicle status to Qt (GROUP C 0x3xx)
  *   - Receives BCM status reports and fault DTCs (GROUP D/E 0x4xx/0x5xx)
  *   - Forwards banners and diagnostics to Qt (GROUP F 0x6xx)
  *   - Sends heartbeat and blink tick (GROUP G 0x7xx / 0x130)
  *
  * PIN MAP (Blue Pill STM32F103C8T6):
  *   PA0   ADC1 CH0   Speed potentiometer (0-150 km/h)
  *   PA1   ADC1 CH1   Fuel potentiometer  (0-100%)
  *   PA2   GPIO_IN    Headlight button (pull-down, active HIGH)
  *   PA9   UART1_TX   Debug UART 115200
  *   PA10  UART1_RX
  *   PB0   GPIO_OUT   LED green  (speed < 60)
  *   PB1   GPIO_OUT   LED orange (60-99 km/h)
  *   PB8   CAN1_RX    (AFIO remap 2)
  *   PB9   CAN1_TX
  *   PB12  GPIO_OUT   LED red    (speed >= 100)
  *   PC13  GPIO_OUT   Onboard LED - CAN TX activity (active LOW)
  *
  * CLOCK: HSE 8MHz x PLL9 = 72MHz SYSCLK, APB1=36MHz, APB2=72MHz, ADC=12MHz
  * CAN:   500kbps (Prescaler=9, BS1=6TQ, BS2=1TQ)
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
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
#include "can_messages.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef struct {
    VehicleGear_t       gear;
    uint8_t             state_flags;
    uint16_t            speed_kmh;
    uint8_t             fuel_pct;
    uint8_t             coolant_temp;
    uint8_t             battery_v;
    uint8_t             light_flags;
    CmdWiper_t          wiper_mode;
    TrunkMotorState_t   trunk_state;
    uint8_t             trunk_pct;
    CmdTurn_t           turn_armed;
    uint8_t             blink_phase;
    uint16_t            rear_distance_cm;
    ParkingLevel_t      parking_level;
} VehicleState_t;

typedef struct {
    uint16_t            dtc_code;
    DTC_Severity_t      severity;
    uint8_t             count;
    uint8_t             node_id;
    SimpleErrorCode_t   simple_code;
    uint8_t             active;
} DtcEntry_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DTC_STORE_SIZE  8U
#define BCM_TIMEOUT_MS  500U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef  hadc1;
CAN_HandleTypeDef  hcan;
DMA_HandleTypeDef  hdma_adc1;
TIM_HandleTypeDef  htim2;
TIM_HandleTypeDef  htim3;
TIM_HandleTypeDef  htim4;
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
uint16_t adc_buf[2];

static VehicleState_t g_vehicle = {
    .gear              = GEAR_PARK,
    .state_flags       = STATE_ENGINE_RUNNING,
    .speed_kmh         = 0U,
    .fuel_pct          = 100U,
    .coolant_temp      = ENCODE_TEMP(25),
    .battery_v         = ENCODE_VOLTAGE(12.6f),
    .light_flags       = 0U,
    .wiper_mode        = CMD_WIPER_OFF,
    .trunk_state       = TRUNK_IDLE,
    .trunk_pct         = 0U,
    .turn_armed        = CMD_TURN_OFF,
    .blink_phase       = 0U,
    .rear_distance_cm  = DIST_NO_OBJECT,
    .parking_level     = PARKING_CLEAR
};

static DtcEntry_t g_dtc_store[DTC_STORE_SIZE];
static uint8_t    g_dtc_count  = 0U;
static uint32_t   g_last_hb_front = 0U;
static uint32_t   g_last_hb_rear  = 0U;
static uint8_t    g_btn_prev   = 0U;
static uint8_t    g_uptime_cnt = 0U;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_DMA_Init(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_CAN_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
/* USER CODE BEGIN PFP */
static HAL_StatusTypeDef CAN_Send(uint16_t id, const uint8_t *data, uint8_t dlc);
static HAL_StatusTypeDef CAN_ConfigExactStdIdFilter(uint32_t filter_bank, uint16_t std_id);
static HAL_StatusTypeDef CAN_ConfigFilters(void);
static void Update_Sensors(void);
static void Broadcast_VehicleState(void);
static void Broadcast_LightState(void);
static void Broadcast_TrunkState(void);
static void Broadcast_RadarState(void);
static void Send_Heartbeat(void);
static void Send_BlinkTick(void);
static ValidationResult_t Validate_LightCmd(CmdLight_t cmd);
static ValidationResult_t Validate_WiperCmd(CmdWiper_t cmd);
static ValidationResult_t Validate_TurnCmd(CmdTurn_t cmd);
static ValidationResult_t Validate_TrunkCmd(CmdTrunk_t cmd);
static void Send_ACK(uint8_t cmd_nibble, AckStatus_t status, ValidationResult_t reason);
static void Execute_LightCmd(CmdLight_t cmd, uint8_t brightness);
static void Execute_WiperCmd(CmdWiper_t mode, uint8_t spray);
static void Execute_TurnCmd(CmdTurn_t cmd);
static void Execute_TrunkCmd(CmdTrunk_t cmd);
static void Handle_DiagRequest(const uint8_t *rx_data, uint8_t dlc);
static void Log_DTC(uint8_t node_id, DTC_Severity_t sev, uint16_t dtc, uint8_t count, SimpleErrorCode_t simple);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static HAL_StatusTypeDef CAN_Send(uint16_t id, const uint8_t *data, uint8_t dlc)
{
    CAN_TxHeaderTypeDef hdr = {0};
    uint32_t mailbox;
    if (dlc > 8U)                     { return HAL_ERROR; }
    if ((data == NULL) && (dlc > 0U)) { return HAL_ERROR; }
    hdr.StdId              = id & 0x07FFU;
    hdr.ExtId              = 0U;
    hdr.IDE                = CAN_ID_STD;
    hdr.RTR                = CAN_RTR_DATA;
    hdr.DLC                = dlc;
    hdr.TransmitGlobalTime = DISABLE;
    HAL_GPIO_TogglePin(LED_CAN_TX_PORT, LED_CAN_TX_PIN);
    return HAL_CAN_AddTxMessage(&hcan, &hdr, (uint8_t *)data, &mailbox);
}

static HAL_StatusTypeDef CAN_ConfigExactStdIdFilter(uint32_t filter_bank, uint16_t std_id)
{
    CAN_FilterTypeDef filter = {0};
    std_id &= 0x07FFU;
    filter.FilterBank           = filter_bank;
    filter.FilterMode           = CAN_FILTERMODE_IDMASK;
    filter.FilterScale          = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh         = (uint16_t)(std_id << 5);
    filter.FilterIdLow          = 0x0000U;
    filter.FilterMaskIdHigh     = (uint16_t)(0x07FFU << 5);
    filter.FilterMaskIdLow      = 0x0006U;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation     = ENABLE;
    return HAL_CAN_ConfigFilter(&hcan, &filter);
}

static HAL_StatusTypeDef CAN_ConfigFilters(void)
{
    if (CAN_ConfigExactStdIdFilter(0U,  CAN_ID_CMD_LIGHT_CONTROL)    != HAL_OK) { return HAL_ERROR; }
    if (CAN_ConfigExactStdIdFilter(1U,  CAN_ID_CMD_WIPER_CONTROL)    != HAL_OK) { return HAL_ERROR; }
    if (CAN_ConfigExactStdIdFilter(2U,  CAN_ID_CMD_TURN_SIGNAL)      != HAL_OK) { return HAL_ERROR; }
    if (CAN_ConfigExactStdIdFilter(3U,  CAN_ID_CMD_TRUNK_CONTROL)    != HAL_OK) { return HAL_ERROR; }
    if (CAN_ConfigExactStdIdFilter(4U,  CAN_ID_CMD_DIAGNOSTIC)       != HAL_OK) { return HAL_ERROR; }
    if (CAN_ConfigExactStdIdFilter(5U,  CAN_ID_REPORT_FRONT_STATUS)  != HAL_OK) { return HAL_ERROR; }
    if (CAN_ConfigExactStdIdFilter(6U,  CAN_ID_REPORT_FRONT_SENSORS) != HAL_OK) { return HAL_ERROR; }
    if (CAN_ConfigExactStdIdFilter(7U,  CAN_ID_REPORT_REAR_STATUS)   != HAL_OK) { return HAL_ERROR; }
    if (CAN_ConfigExactStdIdFilter(8U,  CAN_ID_REPORT_REAR_SENSORS)  != HAL_OK) { return HAL_ERROR; }
    if (CAN_ConfigExactStdIdFilter(9U,  CAN_ID_FAULT_FRONT_BCM)      != HAL_OK) { return HAL_ERROR; }
    if (CAN_ConfigExactStdIdFilter(10U, CAN_ID_FAULT_REAR_BCM)       != HAL_OK) { return HAL_ERROR; }
    if (CAN_ConfigExactStdIdFilter(11U, CAN_ID_HEARTBEAT_FRONT_BCM)  != HAL_OK) { return HAL_ERROR; }
    if (CAN_ConfigExactStdIdFilter(12U, CAN_ID_HEARTBEAT_REAR_BCM)   != HAL_OK) { return HAL_ERROR; }
    return HAL_OK;
}

static void Update_Sensors(void)
{
    g_vehicle.speed_kmh = (uint16_t)((adc_buf[0] * 150UL) / 4095UL);
    g_vehicle.fuel_pct  = (uint8_t)((adc_buf[1] * 100UL) / 4095UL);

    if (g_vehicle.speed_kmh > 0U) {
        g_vehicle.state_flags |= STATE_VEHICLE_MOVING;
    } else {
        g_vehicle.state_flags &= (uint8_t)(~STATE_VEHICLE_MOVING);
    }
    if (g_vehicle.gear == GEAR_REVERSE) {
        g_vehicle.state_flags |= STATE_REVERSE_ACTIVE;
    } else {
        g_vehicle.state_flags &= (uint8_t)(~STATE_REVERSE_ACTIVE);
    }

    HAL_GPIO_WritePin(LED_GREEN_PORT,  LED_GREEN_PIN,
        (g_vehicle.speed_kmh < 60U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_ORANGE_PORT, LED_ORANGE_PIN,
        ((g_vehicle.speed_kmh >= 60U) && (g_vehicle.speed_kmh < 100U)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_RED_PORT,    LED_RED_PIN,
        (g_vehicle.speed_kmh >= 100U) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    uint8_t btn = (uint8_t)HAL_GPIO_ReadPin(BTN_HEADLIGHT_PORT, BTN_HEADLIGHT_PIN);
    if ((btn != 0U) && (g_btn_prev == 0U)) {
        if ((g_vehicle.light_flags & STATUS_HEADLIGHT_ON) != 0U) {
            g_vehicle.light_flags &= (uint8_t)(~STATUS_HEADLIGHT_ON);
        } else {
            g_vehicle.light_flags |= STATUS_HEADLIGHT_ON;
        }
        Broadcast_LightState();
    }
    g_btn_prev = btn;
}

static void Broadcast_VehicleState(void)
{
    uint8_t d[7];
    d[0] = (uint8_t)g_vehicle.gear;
    d[1] = g_vehicle.state_flags;
    d[2] = UNPACK_HIGH_BYTE(g_vehicle.speed_kmh);
    d[3] = UNPACK_LOW_BYTE(g_vehicle.speed_kmh);
    d[4] = g_vehicle.fuel_pct;
    d[5] = g_vehicle.coolant_temp;
    d[6] = g_vehicle.battery_v;
    CAN_Send(CAN_ID_STATUS_VEHICLE_STATE, d, 7U);
}

static void Broadcast_LightState(void)
{
    uint8_t d[2];
    d[0] = g_vehicle.light_flags;
    d[1] = (uint8_t)g_vehicle.wiper_mode;
    CAN_Send(CAN_ID_STATUS_LIGHTS_STATE, d, 2U);
}

static void Broadcast_TrunkState(void)
{
    uint8_t d[2];
    d[0] = g_vehicle.trunk_pct;
    d[1] = (uint8_t)g_vehicle.trunk_state;
    CAN_Send(CAN_ID_STATUS_TRUNK_STATE, d, 2U);
}

static void Broadcast_RadarState(void)
{
    if ((g_vehicle.state_flags & STATE_REVERSE_ACTIVE) == 0U) { return; }
    uint8_t d[4];
    d[0] = UNPACK_HIGH_BYTE(g_vehicle.rear_distance_cm);
    d[1] = UNPACK_LOW_BYTE(g_vehicle.rear_distance_cm);
    d[2] = (uint8_t)g_vehicle.parking_level;
    d[3] = 0x00U;
    CAN_Send(CAN_ID_STATUS_REVERSE_RADAR, d, 4U);
}

static void Send_Heartbeat(void)
{
    uint8_t flags = HB_INIT_OK | HB_CAN_OK | HB_SENSORS_OK;
    if (g_dtc_count > 0U) { flags |= HB_DTC_ACTIVE; }
    uint8_t d[2] = { g_uptime_cnt++, flags };
    CAN_Send(CAN_ID_HEARTBEAT_CENTRAL, d, 2U);
}

static void Send_BlinkTick(void)
{
    g_vehicle.blink_phase ^= 1U;
    uint8_t tick[1] = { 0x01U };
    CAN_Send(CAN_ID_BLINK_TICK, tick, 1U);
    uint8_t phase = 0U;
    if ((g_vehicle.turn_armed == CMD_TURN_LEFT) || (g_vehicle.turn_armed == CMD_TURN_HAZARD)) {
        phase |= (g_vehicle.blink_phase != 0U) ? 0x01U : 0x00U;
    }
    if ((g_vehicle.turn_armed == CMD_TURN_RIGHT) || (g_vehicle.turn_armed == CMD_TURN_HAZARD)) {
        phase |= (g_vehicle.blink_phase != 0U) ? 0x02U : 0x00U;
    }
    uint8_t bp[1] = { phase };
    CAN_Send(CAN_ID_STATUS_TURN_BLINK, bp, 1U);
}

static ValidationResult_t Validate_LightCmd(CmdLight_t cmd)
{
    (void)cmd;
    return VALIDATION_OK;
}

static ValidationResult_t Validate_WiperCmd(CmdWiper_t cmd)
{
    (void)cmd;
    return VALIDATION_OK;
}

static ValidationResult_t Validate_TurnCmd(CmdTurn_t cmd)
{
    (void)cmd;
    return VALIDATION_OK;
}

static ValidationResult_t Validate_TrunkCmd(CmdTrunk_t cmd)
{
    if (cmd == CMD_TRUNK_STOP) { return VALIDATION_OK; }
    if ((g_vehicle.state_flags & STATE_VEHICLE_MOVING) != 0U) { return VALIDATION_ERR_SPEED; }
    if ((g_vehicle.gear != GEAR_PARK) && (g_vehicle.gear != GEAR_NEUTRAL)) { return VALIDATION_ERR_BAD_GEAR; }
    if ((g_vehicle.trunk_state == TRUNK_OPENING) || (g_vehicle.trunk_state == TRUNK_CLOSING)) {
        return VALIDATION_ERR_RESOURCE_BUSY;
    }
    for (uint8_t i = 0U; i < DTC_STORE_SIZE; i++) {
        if ((g_dtc_store[i].active != 0U) &&
            ((g_dtc_store[i].dtc_code == (uint16_t)DTC_B1020) ||
             (g_dtc_store[i].dtc_code == (uint16_t)DTC_B1021))) {
            return VALIDATION_ERR_DTC_ACTIVE;
        }
    }
    return VALIDATION_OK;
}

static void Send_ACK(uint8_t cmd_nibble, AckStatus_t status, ValidationResult_t reason)
{
    uint8_t d[3] = { cmd_nibble, (uint8_t)status, (uint8_t)reason };
    CAN_Send(CAN_ID_CMD_ACK, d, 3U);
}

static void Execute_LightCmd(CmdLight_t cmd, uint8_t brightness)
{
    uint8_t exec_mask = 0U;
    switch (cmd) {
        case CMD_LIGHT_OFF:
            g_vehicle.light_flags &= (uint8_t)(~(STATUS_HEADLIGHT_ON | STATUS_DRL_ON | STATUS_HIGH_BEAM_ON | STATUS_FOG_ON));
            exec_mask = 0U;
            break;
        case CMD_LIGHT_DRL_ON:
            g_vehicle.light_flags |= STATUS_DRL_ON;
            exec_mask = EXEC_FRONT_DRL;
            break;
        case CMD_LIGHT_HEADLIGHT_LOW:
            g_vehicle.light_flags |= STATUS_HEADLIGHT_ON;
            g_vehicle.light_flags &= (uint8_t)(~STATUS_HIGH_BEAM_ON);
            exec_mask = EXEC_FRONT_HEADLIGHT;
            break;
        case CMD_LIGHT_HEADLIGHT_HIGH:
            g_vehicle.light_flags |= STATUS_HEADLIGHT_ON | STATUS_HIGH_BEAM_ON;
            exec_mask = EXEC_FRONT_HEADLIGHT | EXEC_FRONT_HIGH_BEAM;
            break;
        case CMD_LIGHT_FOG_ON:
            g_vehicle.light_flags |= STATUS_FOG_ON;
            exec_mask = EXEC_FRONT_FOG;
            break;
        case CMD_LIGHT_FOG_OFF:
            g_vehicle.light_flags &= (uint8_t)(~STATUS_FOG_ON);
            exec_mask = 0U;
            break;
        case CMD_LIGHT_AUTO_MODE:
            g_vehicle.light_flags |= STATUS_DRL_ON;
            exec_mask = EXEC_FRONT_DRL;
            break;
        default:
            break;
    }
    uint8_t d[2] = { exec_mask, brightness };
    CAN_Send(CAN_ID_EXEC_FRONT_LIGHTS, d, 2U);
    Broadcast_LightState();
}

static void Execute_WiperCmd(CmdWiper_t mode, uint8_t spray)
{
    g_vehicle.wiper_mode = mode;
    uint8_t d[2] = { (uint8_t)mode, spray };
    CAN_Send(CAN_ID_EXEC_FRONT_WIPERS, d, 2U);
    Broadcast_LightState();
}

static void Execute_TurnCmd(CmdTurn_t cmd)
{
    g_vehicle.turn_armed = cmd;
    uint8_t exec = 0U;
    switch (cmd) {
        case CMD_TURN_LEFT:
            exec = EXEC_TURN_LEFT_ARM;
            g_vehicle.light_flags &= (uint8_t)(~(STATUS_TURN_RIGHT_ON | STATUS_HAZARD_ON));
            g_vehicle.state_flags &= (uint8_t)(~STATE_HAZARD_ACTIVE);
            break;
        case CMD_TURN_RIGHT:
            exec = EXEC_TURN_RIGHT_ARM;
            g_vehicle.light_flags &= (uint8_t)(~(STATUS_TURN_LEFT_ON | STATUS_HAZARD_ON));
            g_vehicle.state_flags &= (uint8_t)(~STATE_HAZARD_ACTIVE);
            break;
        case CMD_TURN_HAZARD:
            exec = EXEC_TURN_HAZARD_ARM;
            g_vehicle.state_flags |= STATE_HAZARD_ACTIVE;
            g_vehicle.light_flags |= STATUS_HAZARD_ON;
            break;
        case CMD_TURN_OFF:
        default:
            g_vehicle.state_flags &= (uint8_t)(~STATE_HAZARD_ACTIVE);
            g_vehicle.light_flags &= (uint8_t)(~(STATUS_HAZARD_ON | STATUS_TURN_LEFT_ON | STATUS_TURN_RIGHT_ON));
            exec = 0U;
            break;
    }
    uint8_t d[1] = { exec };
    CAN_Send(CAN_ID_EXEC_FRONT_TURN, d, 1U);
    CAN_Send(CAN_ID_EXEC_REAR_TURN, d, 1U);
    Broadcast_LightState();
}

static void Execute_TrunkCmd(CmdTrunk_t cmd)
{
    switch (cmd) {
        case CMD_TRUNK_OPEN:
            g_vehicle.trunk_state = TRUNK_OPENING;
            g_vehicle.state_flags |= STATE_TRUNK_AJAR;
            break;
        case CMD_TRUNK_CLOSE:
            g_vehicle.trunk_state = TRUNK_CLOSING;
            break;
        case CMD_TRUNK_STOP:
        case CMD_TRUNK_NO_ACTION:
        default:
            g_vehicle.trunk_state = TRUNK_IDLE;
            break;
    }
    uint8_t d[1] = { (uint8_t)cmd };
    CAN_Send(CAN_ID_EXEC_REAR_TRUNK, d, 1U);
    Broadcast_TrunkState();
}

static void Handle_DiagRequest(const uint8_t *rx_data, uint8_t dlc)
{
    if (dlc < 1U) { return; }
    DiagCmd_t cmd = (DiagCmd_t)rx_data[0];
    uint8_t resp[8] = {0U};
    resp[0] = (uint8_t)cmd;
    resp[1] = 0U;
    resp[2] = 1U;

    switch (cmd) {
        case DIAG_CMD_READ_DTC_COUNT:
            resp[3] = g_dtc_count;
            CAN_Send(CAN_ID_STATUS_DIAG_RESPONSE, resp, 4U);
            break;

        case DIAG_CMD_READ_ALL_DTCS:
            if (g_dtc_count == 0U) {
                CAN_Send(CAN_ID_STATUS_DIAG_RESPONSE, resp, 4U);
            } else {
                for (uint8_t i = 0U; (i < DTC_STORE_SIZE) && (g_dtc_store[i].active != 0U); i += 2U) {
                    resp[3] = UNPACK_HIGH_BYTE(g_dtc_store[i].dtc_code);
                    resp[4] = UNPACK_LOW_BYTE(g_dtc_store[i].dtc_code);
                    resp[5] = g_dtc_store[i].count;
                    if (((i + 1U) < DTC_STORE_SIZE) && (g_dtc_store[i + 1U].active != 0U)) {
                        resp[6] = UNPACK_HIGH_BYTE(g_dtc_store[i + 1U].dtc_code);
                        resp[7] = UNPACK_LOW_BYTE(g_dtc_store[i + 1U].dtc_code);
                    } else {
                        resp[6] = 0U; resp[7] = 0U;
                    }
                    CAN_Send(CAN_ID_STATUS_DIAG_RESPONSE, resp, 8U);
                    resp[1]++;
                }
            }
            break;

        case DIAG_CMD_CLEAR_ALL_DTCS:
            memset(g_dtc_store, 0, sizeof(g_dtc_store));
            g_dtc_count = 0U;
            g_vehicle.state_flags &= (uint8_t)(~STATE_DTC_ACTIVE);
            resp[3] = 0xAAU;
            CAN_Send(CAN_ID_STATUS_DIAG_RESPONSE, resp, 4U);
            {
                uint8_t clr[2] = { 0x00U, 0x00U };
                CAN_Send(CAN_ID_BANNER_CLEAR, clr, 2U);
            }
            break;

        case DIAG_CMD_NODE_STATUS: {
            uint32_t now = HAL_GetTick();
            resp[3] = NODE_ID_CENTRAL_ECU;
            resp[4] = ((now - g_last_hb_front) < BCM_TIMEOUT_MS) ? 0x01U : 0x00U;
            resp[5] = ((now - g_last_hb_rear)  < BCM_TIMEOUT_MS) ? 0x01U : 0x00U;
            CAN_Send(CAN_ID_STATUS_DIAG_RESPONSE, resp, 6U);
            break;
        }

        case DIAG_CMD_READ_LIVE_DATA:
            resp[3] = UNPACK_HIGH_BYTE(g_vehicle.speed_kmh);
            resp[4] = UNPACK_LOW_BYTE(g_vehicle.speed_kmh);
            resp[5] = g_vehicle.fuel_pct;
            resp[6] = g_vehicle.coolant_temp;
            resp[7] = g_vehicle.battery_v;
            CAN_Send(CAN_ID_STATUS_DIAG_RESPONSE, resp, 8U);
            break;

        case DIAG_CMD_ECU_RESET:
            resp[3] = 0xBBU;
            CAN_Send(CAN_ID_STATUS_DIAG_RESPONSE, resp, 4U);
            HAL_Delay(50U);
            NVIC_SystemReset();
            break;

        case DIAG_CMD_READ_FREEZE_FRAME:
        default:
            CAN_Send(CAN_ID_STATUS_DIAG_RESPONSE, resp, 4U);
            break;
    }
}

static void Log_DTC(uint8_t node_id, DTC_Severity_t sev, uint16_t dtc,
                    uint8_t count, SimpleErrorCode_t simple)
{
    for (uint8_t i = 0U; i < DTC_STORE_SIZE; i++) {
        if ((g_dtc_store[i].active != 0U) && (g_dtc_store[i].dtc_code == dtc)) {
            g_dtc_store[i].count = count;
            return;
        }
    }
    for (uint8_t i = 0U; i < DTC_STORE_SIZE; i++) {
        if (g_dtc_store[i].active == 0U) {
            g_dtc_store[i].dtc_code    = dtc;
            g_dtc_store[i].severity    = sev;
            g_dtc_store[i].count       = count;
            g_dtc_store[i].node_id     = node_id;
            g_dtc_store[i].simple_code = simple;
            g_dtc_store[i].active      = 1U;
            if (g_dtc_count < DTC_STORE_SIZE) { g_dtc_count++; }
            g_vehicle.state_flags |= STATE_DTC_ACTIVE;
            uint8_t b[6];
            b[0] = node_id; b[1] = (uint8_t)sev;
            b[2] = UNPACK_HIGH_BYTE(dtc); b[3] = UNPACK_LOW_BYTE(dtc);
            b[4] = count; b[5] = (uint8_t)simple;
            CAN_Send(CAN_ID_BANNER_FAULT, b, 6U);
            return;
        }
    }
    memmove(&g_dtc_store[0], &g_dtc_store[1], sizeof(DtcEntry_t) * (DTC_STORE_SIZE - 1U));
    g_dtc_store[DTC_STORE_SIZE - 1U].dtc_code    = dtc;
    g_dtc_store[DTC_STORE_SIZE - 1U].severity    = sev;
    g_dtc_store[DTC_STORE_SIZE - 1U].count       = count;
    g_dtc_store[DTC_STORE_SIZE - 1U].node_id     = node_id;
    g_dtc_store[DTC_STORE_SIZE - 1U].simple_code = simple;
    g_dtc_store[DTC_STORE_SIZE - 1U].active      = 1U;
    uint8_t b[6];
    b[0] = node_id; b[1] = (uint8_t)sev;
    b[2] = UNPACK_HIGH_BYTE(dtc); b[3] = UNPACK_LOW_BYTE(dtc);
    b[4] = count; b[5] = (uint8_t)simple;
    CAN_Send(CAN_ID_BANNER_FAULT, b, 6U);
}

/* USER CODE END 0 */

int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  MX_DMA_Init();
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_CAN_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();

  /* USER CODE BEGIN 2 */
  if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buf, 2U) != HAL_OK) { Error_Handler(); }
  if (CAN_ConfigFilters() != HAL_OK) { Error_Handler(); }
  if (HAL_CAN_Start(&hcan) != HAL_OK) { Error_Handler(); }
  if (HAL_CAN_ActivateNotification(&hcan,
      CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_ERROR | CAN_IT_BUSOFF) != HAL_OK) { Error_Handler(); }
  if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK) { Error_Handler(); }
  if (HAL_TIM_Base_Start_IT(&htim3) != HAL_OK) { Error_Handler(); }
  if (HAL_TIM_Base_Start_IT(&htim4) != HAL_OK) { Error_Handler(); }
  g_last_hb_front = HAL_GetTick();
  g_last_hb_rear  = HAL_GetTick();
  /* USER CODE END 2 */

  /* USER CODE BEGIN WHILE */
  while (1)
  {
      __WFI();
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  RCC_OscInitStruct.OscillatorType    = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState          = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue    = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState          = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState      = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource     = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL        = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

  RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                   | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) { Error_Handler(); }

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection    = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) { Error_Handler(); }
}

static void MX_ADC1_Init(void)
{
  /* USER CODE BEGIN ADC1_Init 0 */
  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */
  /* USER CODE END ADC1_Init 1 */

  hadc1.Instance                   = ADC1;
  hadc1.Init.ScanConvMode          = ADC_SCAN_ENABLE;
  hadc1.Init.ContinuousConvMode    = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion       = 2U;
  if (HAL_ADC_Init(&hadc1) != HAL_OK) { Error_Handler(); }

  sConfig.Channel      = ADC_CHANNEL_0;
  sConfig.Rank         = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) { Error_Handler(); }

  sConfig.Channel      = ADC_CHANNEL_1;
  sConfig.Rank         = ADC_REGULAR_RANK_2;
  sConfig.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) { Error_Handler(); }

  /* USER CODE BEGIN ADC1_Init 2 */
  /* USER CODE END ADC1_Init 2 */
}

static void MX_CAN_Init(void)
{
  /* USER CODE BEGIN CAN_Init 0 */
  /* USER CODE END CAN_Init 0 */

  /* USER CODE BEGIN CAN_Init 1 */
  /* Remap CAN1 to PB8(RX)/PB9(TX). Must be before HAL_CAN_Init. */
  __HAL_AFIO_REMAP_CAN1_2();
  /* USER CODE END CAN_Init 1 */

  hcan.Instance                  = CAN1;
  hcan.Init.Prescaler            = 9U;
  hcan.Init.Mode                 = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth        = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1             = CAN_BS1_6TQ;
  hcan.Init.TimeSeg2             = CAN_BS2_1TQ;
  hcan.Init.TimeTriggeredMode    = DISABLE;
  hcan.Init.AutoBusOff           = ENABLE;
  hcan.Init.AutoWakeUp           = DISABLE;
  hcan.Init.AutoRetransmission   = ENABLE;
  hcan.Init.ReceiveFifoLocked    = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK) { Error_Handler(); }

  /* Configure CAN RX0 NVIC - highest priority among peripherals */
  HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 0U, 0U);
  HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);

  /* USER CODE BEGIN CAN_Init 2 */
  /* USER CODE END CAN_Init 2 */
}

static void MX_USART1_UART_Init(void)
{
  /* USER CODE BEGIN USART1_Init 0 */
  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */
  /* USER CODE END USART1_Init 1 */

  huart1.Instance          = USART1;
  huart1.Init.BaudRate     = 115200U;
  huart1.Init.WordLength   = UART_WORDLENGTH_8B;
  huart1.Init.StopBits     = UART_STOPBITS_1;
  huart1.Init.Parity       = UART_PARITY_NONE;
  huart1.Init.Mode         = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK) { Error_Handler(); }

  /* USER CODE BEGIN USART1_Init 2 */
  /* USER CODE END USART1_Init 2 */
}

static void MX_DMA_Init(void)
{
  __HAL_RCC_DMA1_CLK_ENABLE();

  hdma_adc1.Instance                 = DMA1_Channel1;
  hdma_adc1.Init.Direction           = DMA_PERIPH_TO_MEMORY;
  hdma_adc1.Init.PeriphInc           = DMA_PINC_DISABLE;
  hdma_adc1.Init.MemInc              = DMA_MINC_ENABLE;
  hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  hdma_adc1.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
  hdma_adc1.Init.Mode                = DMA_CIRCULAR;
  hdma_adc1.Init.Priority            = DMA_PRIORITY_LOW;
  if (HAL_DMA_Init(&hdma_adc1) != HAL_OK) { Error_Handler(); }

  __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);

  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 2U, 0U);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_AFIO_CLK_ENABLE();

  /* PA0, PA1 - Analog for ADC */
  GPIO_InitStruct.Pin  = ADC_SPEED_PIN | ADC_FUEL_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PA2 - Headlight button, pull-down */
  GPIO_InitStruct.Pin  = BTN_HEADLIGHT_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PC13 - Onboard LED (active LOW), start off (HIGH) */
  GPIO_InitStruct.Pin   = LED_CAN_TX_PIN;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_CAN_TX_PORT, &GPIO_InitStruct);
  HAL_GPIO_WritePin(LED_CAN_TX_PORT, LED_CAN_TX_PIN, GPIO_PIN_SET);

  /* PB0, PB1, PB12 - Speed LEDs */
  GPIO_InitStruct.Pin   = LED_GREEN_PIN | LED_ORANGE_PIN | LED_RED_PIN;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOB, LED_GREEN_PIN | LED_ORANGE_PIN | LED_RED_PIN, GPIO_PIN_RESET);
}

static void MX_TIM2_Init(void)
{
  /* USER CODE BEGIN TIM2_Init 0 */
  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef  sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig      = {0};

  /* USER CODE BEGIN TIM2_Init 1 */
  /* USER CODE END TIM2_Init 1 */

  /*
   * TIM clock = APB1_timer_clk = 2 x APB1 = 2 x 36MHz = 72MHz
   * PSC=35999 -> tick = 72MHz/36000 = 2000 Hz
   * ARR=199   -> period = 200/2000 = 100ms
   */
  htim2.Instance               = TIM2;
  htim2.Init.Prescaler         = 35999U;
  htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim2.Init.Period            = 199U;
  htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK) { Error_Handler(); }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) { Error_Handler(); }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK) { Error_Handler(); }

  HAL_NVIC_SetPriority(TIM2_IRQn, 1U, 0U);
  HAL_NVIC_EnableIRQ(TIM2_IRQn);

  /* USER CODE BEGIN TIM2_Init 2 */
  /* USER CODE END TIM2_Init 2 */
}

static void MX_TIM3_Init(void)
{
  /* USER CODE BEGIN TIM3_Init 0 */
  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef  sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig      = {0};

  /* USER CODE BEGIN TIM3_Init 1 */
  /* USER CODE END TIM3_Init 1 */

  /*
   * PSC=35999 -> tick = 2000 Hz
   * ARR=999   -> period = 1000/2000 = 500ms
   */
  htim3.Instance               = TIM3;
  htim3.Init.Prescaler         = 35999U;
  htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim3.Init.Period            = 999U;
  htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK) { Error_Handler(); }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK) { Error_Handler(); }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK) { Error_Handler(); }

  HAL_NVIC_SetPriority(TIM3_IRQn, 1U, 1U);
  HAL_NVIC_EnableIRQ(TIM3_IRQn);

  /* USER CODE BEGIN TIM3_Init 2 */
  /* USER CODE END TIM3_Init 2 */
}

static void MX_TIM4_Init(void)
{
  /* USER CODE BEGIN TIM4_Init 0 */
  /* USER CODE END TIM4_Init 0 */

  TIM_ClockConfigTypeDef  sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig      = {0};

  /* USER CODE BEGIN TIM4_Init 1 */
  /* USER CODE END TIM4_Init 1 */

  /*
   * PSC=35999 -> tick = 2000 Hz
   * ARR=1999  -> period = 2000/2000 = 1000ms
   */
  htim4.Instance               = TIM4;
  htim4.Init.Prescaler         = 35999U;
  htim4.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim4.Init.Period            = 1999U;
  htim4.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim4) != HAL_OK) { Error_Handler(); }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK) { Error_Handler(); }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK) { Error_Handler(); }

  HAL_NVIC_SetPriority(TIM4_IRQn, 1U, 2U);
  HAL_NVIC_EnableIRQ(TIM4_IRQn);

  /* USER CODE BEGIN TIM4_Init 2 */
  /* USER CODE END TIM4_Init 2 */
}

/* USER CODE BEGIN 4 */

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *phcan)
{
    if (phcan->Instance != CAN1) { return; }

    CAN_RxHeaderTypeDef rx_hdr;
    uint8_t rx_data[8] = {0U};

    if (HAL_CAN_GetRxMessage(phcan, CAN_RX_FIFO0, &rx_hdr, rx_data) != HAL_OK) { return; }
    if (rx_hdr.IDE != CAN_ID_STD)   { return; }
    if (rx_hdr.RTR != CAN_RTR_DATA) { return; }

    uint16_t id  = (uint16_t)rx_hdr.StdId;
    uint8_t  dlc = (uint8_t)rx_hdr.DLC;
    ValidationResult_t result;

    switch (id) {
        /* GROUP A: Commands from Qt */
        case CAN_ID_CMD_LIGHT_CONTROL:
            result = Validate_LightCmd((CmdLight_t)rx_data[0]);
            Send_ACK(0x00U, (result == VALIDATION_OK) ? ACK_STATUS_APPROVED : ACK_STATUS_REJECTED, result);
            if (result == VALIDATION_OK) { Execute_LightCmd((CmdLight_t)rx_data[0], (dlc > 1U) ? rx_data[1] : 100U); }
            break;
        case CAN_ID_CMD_WIPER_CONTROL:
            result = Validate_WiperCmd((CmdWiper_t)rx_data[0]);
            Send_ACK(0x01U, (result == VALIDATION_OK) ? ACK_STATUS_APPROVED : ACK_STATUS_REJECTED, result);
            if (result == VALIDATION_OK) { Execute_WiperCmd((CmdWiper_t)rx_data[0], (dlc > 1U) ? rx_data[1] : 0U); }
            break;
        case CAN_ID_CMD_TURN_SIGNAL:
            result = Validate_TurnCmd((CmdTurn_t)rx_data[0]);
            Send_ACK(0x02U, (result == VALIDATION_OK) ? ACK_STATUS_APPROVED : ACK_STATUS_REJECTED, result);
            if (result == VALIDATION_OK) { Execute_TurnCmd((CmdTurn_t)rx_data[0]); }
            break;
        case CAN_ID_CMD_TRUNK_CONTROL:
            result = Validate_TrunkCmd((CmdTrunk_t)rx_data[0]);
            Send_ACK(0x03U, (result == VALIDATION_OK) ? ACK_STATUS_APPROVED : ACK_STATUS_REJECTED, result);
            if (result == VALIDATION_OK) { Execute_TrunkCmd((CmdTrunk_t)rx_data[0]); }
            break;
        case CAN_ID_CMD_DIAGNOSTIC:
            Handle_DiagRequest(rx_data, dlc);
            break;
        /* GROUP D: BCM Status Reports */
        case CAN_ID_REPORT_FRONT_STATUS:
            if (dlc >= 1U) {
                uint8_t f = rx_data[0];
                if ((f & FRONT_ACT_HEADLIGHT) != 0U) { g_vehicle.light_flags |= STATUS_HEADLIGHT_ON; }
                else                                  { g_vehicle.light_flags &= (uint8_t)(~STATUS_HEADLIGHT_ON); }
                if ((f & FRONT_ACT_DRL) != 0U)        { g_vehicle.light_flags |= STATUS_DRL_ON; }
                else                                  { g_vehicle.light_flags &= (uint8_t)(~STATUS_DRL_ON); }
                if ((f & FRONT_ACT_FOG) != 0U)        { g_vehicle.light_flags |= STATUS_FOG_ON; }
                else                                  { g_vehicle.light_flags &= (uint8_t)(~STATUS_FOG_ON); }
            }
            if (dlc >= 2U) { g_vehicle.wiper_mode = (CmdWiper_t)rx_data[1]; }
            break;
        case CAN_ID_REPORT_REAR_STATUS:
            if (dlc >= 3U) {
                g_vehicle.trunk_state = (TrunkMotorState_t)rx_data[1];
                g_vehicle.trunk_pct   = rx_data[2];
                if ((g_vehicle.trunk_state == TRUNK_IDLE) && (g_vehicle.trunk_pct == 0U)) {
                    g_vehicle.state_flags &= (uint8_t)(~STATE_TRUNK_AJAR);
                } else {
                    g_vehicle.state_flags |= STATE_TRUNK_AJAR;
                }
                Broadcast_TrunkState();
                
                /* Turn Signal inputs from Rear BCM (hardware pivot) */
                if (g_vehicle.turn_armed != CMD_TURN_HAZARD) {
                    if (rx_data[0] & REAR_ACT_LTURN) {
                        if (g_vehicle.turn_armed != CMD_TURN_LEFT) Execute_TurnCmd(CMD_TURN_LEFT);
                    } else if (rx_data[0] & REAR_ACT_RTURN) {
                        if (g_vehicle.turn_armed != CMD_TURN_RIGHT) Execute_TurnCmd(CMD_TURN_RIGHT);
                    } else {
                        if (g_vehicle.turn_armed != CMD_TURN_OFF) Execute_TurnCmd(CMD_TURN_OFF);
                    }
                }
            }
            break;
        case CAN_ID_REPORT_REAR_SENSORS:
            if (dlc >= 4U) {
                g_vehicle.rear_distance_cm = PACK_U16(rx_data[0], rx_data[1]);
                g_vehicle.parking_level    = (ParkingLevel_t)rx_data[3];
                Broadcast_RadarState();
            }
            break;
        case CAN_ID_REPORT_FRONT_SENSORS:
            /* Ambient light + washer fluid - reserved for future auto-light logic */
            break;
        /* GROUP E: BCM Fault Reports */
        case CAN_ID_FAULT_FRONT_BCM:
        case CAN_ID_FAULT_REAR_BCM:
            if (dlc >= 5U) {
                uint8_t src          = (id == CAN_ID_FAULT_FRONT_BCM) ? NODE_ID_FRONT_BCM : NODE_ID_REAR_BCM;
                DTC_Severity_t sev   = (DTC_Severity_t)rx_data[0];
                uint16_t dtc         = PACK_U16(rx_data[1], rx_data[2]);
                uint8_t cnt          = rx_data[3];
                SimpleErrorCode_t sc = (SimpleErrorCode_t)rx_data[4];
                Log_DTC(src, sev, dtc, cnt, sc);
            }
            break;
        /* GROUP G: BCM Heartbeats */
        case CAN_ID_HEARTBEAT_FRONT_BCM:
            g_last_hb_front = HAL_GetTick();
            break;
        case CAN_ID_HEARTBEAT_REAR_BCM:
            g_last_hb_rear = HAL_GetTick();
            break;
        default:
            break;
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        Update_Sensors();
        Broadcast_VehicleState();
        Send_Heartbeat();
    }
    else if (htim->Instance == TIM3) {
        Send_BlinkTick();
    }
    else if (htim->Instance == TIM4) {
        uint32_t now = HAL_GetTick();
        if ((now - g_last_hb_front) > BCM_TIMEOUT_MS) {
            uint8_t d[2] = { NODE_ID_FRONT_BCM, 1U };
            CAN_Send(CAN_ID_WATCHDOG_ALERT, d, 2U);
            Log_DTC(NODE_ID_FRONT_BCM, DTC_SEVERITY_ERROR, (uint16_t)DTC_C1002, 1U, ERR_NODE_TIMEOUT);
        }
        if ((now - g_last_hb_rear) > BCM_TIMEOUT_MS) {
            uint8_t d[2] = { NODE_ID_REAR_BCM, 1U };
            CAN_Send(CAN_ID_WATCHDOG_ALERT, d, 2U);
            Log_DTC(NODE_ID_REAR_BCM, DTC_SEVERITY_ERROR, (uint16_t)DTC_C1003, 1U, ERR_NODE_TIMEOUT);
        }
    }
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *phcan)
{
    if (phcan->Instance != CAN1) { return; }
    uint32_t err = HAL_CAN_GetError(phcan);
    if ((err & HAL_CAN_ERROR_BOF) != 0U) {
        Log_DTC(NODE_ID_CENTRAL_ECU, DTC_SEVERITY_CRITICAL, (uint16_t)DTC_C1001, 1U, ERR_CAN_BUS_OFF);
    }
}

/* USER CODE END 4 */

void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
      HAL_GPIO_TogglePin(LED_CAN_TX_PORT, LED_CAN_TX_PIN);
      for (volatile uint32_t i = 0U; i < 1440000UL; i++) { __NOP(); }
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  (void)file;
  (void)line;
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
