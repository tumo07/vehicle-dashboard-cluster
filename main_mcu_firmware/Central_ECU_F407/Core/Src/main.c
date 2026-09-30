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
#include "can_messages.h"
#include <string.h>
#include <stdint.h>
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
DMA_HandleTypeDef hdma_adc1;

CAN_HandleTypeDef hcan1;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim6;
TIM_HandleTypeDef htim7;

/* USER CODE BEGIN PV */

/* ADC DMA buffer: [0]=speed pot (PA1), [1]=fuel pot (PA2) */
static uint16_t adc_buf[2];

/* ── Vehicle state ──────────────────────────────────────────────────────── */
typedef struct {
    VehicleGear_t      gear;
    uint8_t            state_flags;
    uint16_t           speed_kmh;
    uint8_t            fuel_pct;
    uint8_t            coolant_temp;
    uint8_t            battery_v;
    uint8_t            light_flags;
    CmdWiper_t         wiper_mode;
    TrunkMotorState_t  trunk_state;
    uint8_t            trunk_pct;
    CmdTurn_t          turn_armed;
    uint8_t            blink_phase;
    uint16_t           rear_dist_cm;
    ParkingLevel_t     parking_level;
} VehicleState_t;

static VehicleState_t g_veh = {
    .gear         = GEAR_PARK,
    .state_flags  = STATE_ENGINE_RUNNING,
    .speed_kmh    = 0,
    .fuel_pct     = 100,
    .coolant_temp = ENCODE_TEMP(25),
    .battery_v    = ENCODE_VOLTAGE(12.6f),
    .light_flags  = 0,
    .wiper_mode   = CMD_WIPER_OFF,
    .trunk_state  = TRUNK_IDLE,
    .trunk_pct    = 0,
    .turn_armed   = CMD_TURN_OFF,
    .blink_phase  = 0,
    .rear_dist_cm = 0xFFFF,
    .parking_level= PARKING_CLEAR
};

/* ── DTC storage (8-slot ring) ──────────────────────────────────────────── */
typedef struct {
    uint16_t          dtc_code;
    DTC_Severity_t    severity;
    uint8_t           count;
    uint8_t           node_id;
    SimpleErrorCode_t simple_code;
    uint8_t           active;
} DtcEntry_t;

#define DTC_STORE_SIZE  8U
static DtcEntry_t g_dtc[DTC_STORE_SIZE];
static uint8_t    g_dtc_count = 0;

/* ── BCM heartbeat watchdog timestamps ──────────────────────────────────── */
static uint32_t g_hb_front = 0;
static uint32_t g_hb_rear  = 0;
#define BCM_TIMEOUT_MS  500U

/* Misc */
static uint8_t  g_btn_prev   = 0;
static uint8_t  g_uptime_cnt = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_CAN1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM6_Init(void);
static void MX_TIM7_Init(void);
/* USER CODE BEGIN PFP */
static HAL_StatusTypeDef CAN_ConfigExactFilter(uint32_t bank, uint16_t id);
static HAL_StatusTypeDef CAN_ConfigFilters(void);
static HAL_StatusTypeDef CAN_Send(uint16_t id, const uint8_t *data, uint8_t dlc);
static void Update_Sensors(void);
static void Broadcast_VehicleState(void);
static void Broadcast_LightState(void);
static void Broadcast_TrunkState(void);
static void Broadcast_RadarState(void);
static void Send_Heartbeat(void);
static void Send_BlinkTick(void);
static void Send_ACK(uint8_t cmd_nibble, AckStatus_t st, ValidationResult_t reason);
static void Log_DTC(uint8_t node, DTC_Severity_t sev, uint16_t dtc, uint8_t cnt, SimpleErrorCode_t sc);
static ValidationResult_t Validate_TrunkCmd(CmdTrunk_t cmd);
static void Execute_LightCmd(CmdLight_t cmd, uint8_t brightness);
static void Execute_WiperCmd(CmdWiper_t mode, uint8_t spray);
static void Execute_TurnCmd(CmdTurn_t cmd);
static void Execute_TrunkCmd(CmdTrunk_t cmd);
static void Handle_DiagRequest(const uint8_t *d, uint8_t dlc);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ── CAN helpers ─────────────────────────────────────────────────────────── */
static HAL_StatusTypeDef CAN_ConfigExactFilter(uint32_t bank, uint16_t id)
{
    CAN_FilterTypeDef f = {0};
    f.FilterBank           = bank;
    f.FilterMode           = CAN_FILTERMODE_IDMASK;
    f.FilterScale          = CAN_FILTERSCALE_32BIT;
    f.FilterIdHigh         = (uint16_t)(id << 5U);
    f.FilterIdLow          = 0x0000U;
    f.FilterMaskIdHigh     = (uint16_t)(0x7FFU << 5U);
    f.FilterMaskIdLow      = 0x0000U;
    f.FilterFIFOAssignment = CAN_RX_FIFO0;
    f.FilterActivation     = CAN_FILTER_ENABLE;
    f.SlaveStartFilterBank = 14U;
    return HAL_CAN_ConfigFilter(&hcan1, &f);
}

static HAL_StatusTypeDef CAN_ConfigFilters(void)
{
    /* GROUP A: Qt -> Central ECU commands */
    if (CAN_ConfigExactFilter(0U,  CAN_ID_CMD_LIGHT_CONTROL)   != HAL_OK) return HAL_ERROR;
    if (CAN_ConfigExactFilter(1U,  CAN_ID_CMD_WIPER_CONTROL)   != HAL_OK) return HAL_ERROR;
    if (CAN_ConfigExactFilter(2U,  CAN_ID_CMD_TURN_SIGNAL)     != HAL_OK) return HAL_ERROR;
    if (CAN_ConfigExactFilter(3U,  CAN_ID_CMD_TRUNK_CONTROL)   != HAL_OK) return HAL_ERROR;
    if (CAN_ConfigExactFilter(4U,  CAN_ID_CMD_DIAGNOSTIC)      != HAL_OK) return HAL_ERROR;
    /* GROUP D: BCM status + sensor reports */
    if (CAN_ConfigExactFilter(5U,  CAN_ID_REPORT_FRONT_STATUS) != HAL_OK) return HAL_ERROR;
    if (CAN_ConfigExactFilter(6U,  CAN_ID_REPORT_FRONT_SENSORS)!= HAL_OK) return HAL_ERROR;
    if (CAN_ConfigExactFilter(7U,  CAN_ID_REPORT_REAR_STATUS)  != HAL_OK) return HAL_ERROR;
    if (CAN_ConfigExactFilter(8U,  CAN_ID_REPORT_REAR_SENSORS) != HAL_OK) return HAL_ERROR;
    /* GROUP E: BCM fault reports */
    if (CAN_ConfigExactFilter(9U,  CAN_ID_FAULT_FRONT_BCM)     != HAL_OK) return HAL_ERROR;
    if (CAN_ConfigExactFilter(10U, CAN_ID_FAULT_REAR_BCM)      != HAL_OK) return HAL_ERROR;
    /* GROUP G: BCM heartbeats */
    if (CAN_ConfigExactFilter(11U, CAN_ID_HEARTBEAT_FRONT_BCM) != HAL_OK) return HAL_ERROR;
    if (CAN_ConfigExactFilter(12U, CAN_ID_HEARTBEAT_REAR_BCM)  != HAL_OK) return HAL_ERROR;
    return HAL_OK;
}

static HAL_StatusTypeDef CAN_Send(uint16_t id, const uint8_t *data, uint8_t dlc)
{
    CAN_TxHeaderTypeDef hdr = {0};
    uint32_t mailbox;
    if (dlc > 8U || (data == NULL && dlc > 0U)) return HAL_ERROR;
    hdr.StdId              = id & 0x07FFU;
    hdr.IDE                = CAN_ID_STD;
    hdr.RTR                = CAN_RTR_DATA;
    hdr.DLC                = dlc;
    hdr.TransmitGlobalTime = DISABLE;
    HAL_GPIO_TogglePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin); /* CAN TX activity */
    return HAL_CAN_AddTxMessage(&hcan1, &hdr, (uint8_t *)data, &mailbox);
}

/* ── Sensor update (called in TIM6 100ms ISR) ────────────────────────────── */
static void Update_Sensors(void)
{
    /* 1. Rotary Gear Selector from Potentiometer 2 (PA2) */
    uint16_t raw_gear = adc_buf[1];
    if (raw_gear < 820U) {
        g_veh.gear = GEAR_PARK;     /* 0%  - 20%: P (Park) */
    } else if (raw_gear < 1640U) {
        g_veh.gear = GEAR_REVERSE;  /* 20% - 40%: R (Reverse) */
    } else if (raw_gear < 2460U) {
        g_veh.gear = GEAR_NEUTRAL;  /* 40% - 60%: N (Neutral) */
    } else if (raw_gear < 3280U) {
        g_veh.gear = GEAR_DRIVE;    /* 60% - 80%: D (Drive) */
    } else {
        g_veh.gear = GEAR_SPORT;    /* 80% - 100%: S (Sport) */
    }

    /* 2. Fuel level fixed at 85% */
    g_veh.fuel_pct = 85U;

    /* 3. Speed calculation from Potentiometer 1 (PA1) based on active Gear */
    uint16_t pot_speed = (uint16_t)((adc_buf[0] * 150UL) / 4095UL);
    if (g_veh.gear == GEAR_PARK || g_veh.gear == GEAR_NEUTRAL) {
        /* Park & Neutral: vehicle wheels are locked / disengaged */
        g_veh.speed_kmh = 0U;
    } else if (g_veh.gear == GEAR_REVERSE) {
        /* Reverse: speed capped at 40 km/h */
        g_veh.speed_kmh = (uint16_t)((adc_buf[0] * 40UL) / 4095UL);
    } else {
        /* Drive (D) & Sport (S): full speed range 0-150 km/h */
        g_veh.speed_kmh = pot_speed;
    }

    if (g_veh.speed_kmh > 0U)
        g_veh.state_flags |=  STATE_VEHICLE_MOVING;
    else
        g_veh.state_flags &= ~STATE_VEHICLE_MOVING;

    if (g_veh.gear == GEAR_REVERSE)
        g_veh.state_flags |=  STATE_REVERSE_ACTIVE;
    else
        g_veh.state_flags &= ~STATE_REVERSE_ACTIVE;


    /* Speed zone LEDs (Green is strictly reserved for CAN RX now) */
    // Orange LED for 0-60 km/h (replaced Green)
    HAL_GPIO_WritePin(LED_ORANGE_GPIO_Port, LED_ORANGE_Pin,
        (g_veh.speed_kmh < 60U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    // Red LED for >= 60 km/h
    HAL_GPIO_WritePin(LED_RED_GPIO_Port,    LED_RED_Pin,
        (g_veh.speed_kmh >= 60U) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /* Headlight button debounce (PA0 = onboard USER button) */
    uint8_t btn = HAL_GPIO_ReadPin(BTN_HEADLIGHT_GPIO_Port, BTN_HEADLIGHT_Pin);
    if (btn && !g_btn_prev) {
        if (g_veh.light_flags & STATUS_HEADLIGHT_ON)
            g_veh.light_flags &= ~STATUS_HEADLIGHT_ON;
        else
            g_veh.light_flags |=  STATUS_HEADLIGHT_ON;
    }
    g_btn_prev = btn;
}

/* ── Broadcast functions (GROUP C) ───────────────────────────────────────── */
static void Broadcast_VehicleState(void)
{
    uint8_t d[7];
    d[0] = (uint8_t)g_veh.gear;
    d[1] = g_veh.state_flags;
    d[2] = UNPACK_HIGH_BYTE(g_veh.speed_kmh);
    d[3] = UNPACK_LOW_BYTE (g_veh.speed_kmh);
    d[4] = g_veh.fuel_pct;
    d[5] = g_veh.coolant_temp;
    d[6] = g_veh.battery_v;
    CAN_Send(CAN_ID_STATUS_VEHICLE_STATE, d, 7U);
}

static void Broadcast_LightState(void)
{
    uint8_t d[2] = { g_veh.light_flags, (uint8_t)g_veh.wiper_mode };
    CAN_Send(CAN_ID_STATUS_LIGHTS_STATE, d, 2U);
}

static void Broadcast_TrunkState(void)
{
    uint8_t d[2] = { g_veh.trunk_pct, (uint8_t)g_veh.trunk_state };
    CAN_Send(CAN_ID_STATUS_TRUNK_STATE, d, 2U);
}

static void Broadcast_RadarState(void)
{
    if (!(g_veh.state_flags & STATE_REVERSE_ACTIVE)) return;
    uint8_t d[4];
    d[0] = UNPACK_HIGH_BYTE(g_veh.rear_dist_cm);
    d[1] = UNPACK_LOW_BYTE (g_veh.rear_dist_cm);
    d[2] = (uint8_t)g_veh.parking_level;
    d[3] = 0x00U;
    CAN_Send(CAN_ID_STATUS_REVERSE_RADAR, d, 4U);
}

static void Send_Heartbeat(void)
{
    uint8_t flags = HB_INIT_OK | HB_CAN_OK | HB_SENSORS_OK;
    if (g_dtc_count > 0U) flags |= HB_DTC_ACTIVE;
    uint8_t d[2] = { g_uptime_cnt++, flags };
    CAN_Send(CAN_ID_HEARTBEAT_CENTRAL, d, 2U);
}

/* ── Blink tick (TIM7 500ms ISR) ─────────────────────────────────────────── */
static void Send_BlinkTick(void)
{
    g_veh.blink_phase ^= 1U;
    uint8_t tick[1] = { 0x01U };
    CAN_Send(CAN_ID_BLINK_TICK, tick, 1U);

    uint8_t phase = 0;
    if (g_veh.turn_armed == CMD_TURN_LEFT  || g_veh.turn_armed == CMD_TURN_HAZARD)
        phase |= g_veh.blink_phase ? 0x01U : 0x00U;
    if (g_veh.turn_armed == CMD_TURN_RIGHT || g_veh.turn_armed == CMD_TURN_HAZARD)
        phase |= g_veh.blink_phase ? 0x02U : 0x00U;
    uint8_t bp[1] = { phase };
    CAN_Send(CAN_ID_STATUS_TURN_BLINK, bp, 1U);
}

/* ── ACK helper ──────────────────────────────────────────────────────────── */
static void Send_ACK(uint8_t cmd_nibble, AckStatus_t st, ValidationResult_t reason)
{
    uint8_t d[3] = { cmd_nibble, (uint8_t)st, (uint8_t)reason };
    CAN_Send(CAN_ID_CMD_ACK, d, 3U);
}

/* ── DTC logging + banner dispatch ──────────────────────────────────────── */
static void Log_DTC(uint8_t node, DTC_Severity_t sev, uint16_t dtc,
                    uint8_t cnt, SimpleErrorCode_t sc)
{
    for (uint8_t i = 0; i < DTC_STORE_SIZE; i++) {
        if (g_dtc[i].active && g_dtc[i].dtc_code == dtc) {
            g_dtc[i].count = cnt;
            return;
        }
    }
    for (uint8_t i = 0; i < DTC_STORE_SIZE; i++) {
        if (!g_dtc[i].active) {
            g_dtc[i] = (DtcEntry_t){ dtc, sev, cnt, node, sc, 1U };
            if (g_dtc_count < DTC_STORE_SIZE) g_dtc_count++;
            g_veh.state_flags |= STATE_DTC_ACTIVE;
            uint8_t b[6] = { node, (uint8_t)sev,
                UNPACK_HIGH_BYTE(dtc), UNPACK_LOW_BYTE(dtc), cnt, (uint8_t)sc };
            CAN_Send(CAN_ID_BANNER_FAULT, b, 6U);
            return;
        }
    }
    /* Store full — overwrite oldest */
    memmove(&g_dtc[0], &g_dtc[1], sizeof(DtcEntry_t) * (DTC_STORE_SIZE - 1U));
    g_dtc[DTC_STORE_SIZE-1U] = (DtcEntry_t){ dtc, sev, cnt, node, sc, 1U };
    uint8_t b[6] = { node, (uint8_t)sev,
        UNPACK_HIGH_BYTE(dtc), UNPACK_LOW_BYTE(dtc), cnt, (uint8_t)sc };
    CAN_Send(CAN_ID_BANNER_FAULT, b, 6U);
}

/* ── Command validation ──────────────────────────────────────────────────── */
static ValidationResult_t Validate_TrunkCmd(CmdTrunk_t cmd)
{
    if (cmd == CMD_TRUNK_STOP) return VALIDATION_OK;
    if (g_veh.state_flags & STATE_VEHICLE_MOVING)    return VALIDATION_ERR_SPEED;
    if (g_veh.gear != GEAR_PARK && g_veh.gear != GEAR_NEUTRAL) return VALIDATION_ERR_BAD_GEAR;
    if (g_veh.trunk_state == TRUNK_OPENING || g_veh.trunk_state == TRUNK_CLOSING)
        return VALIDATION_ERR_RESOURCE_BUSY;
    for (uint8_t i = 0; i < DTC_STORE_SIZE; i++) {
        if (g_dtc[i].active &&
            (g_dtc[i].dtc_code == (uint16_t)DTC_B1020 ||
             g_dtc[i].dtc_code == (uint16_t)DTC_B1021))
            return VALIDATION_ERR_DTC_ACTIVE;
    }
    return VALIDATION_OK;
}

/* ── Command execution ───────────────────────────────────────────────────── */
static void Execute_LightCmd(CmdLight_t cmd, uint8_t brightness)
{
    switch (cmd) {
        case CMD_LIGHT_OFF:
            g_veh.light_flags &= ~(STATUS_HEADLIGHT_ON|STATUS_DRL_ON|
                                   STATUS_HIGH_BEAM_ON|STATUS_FOG_ON);
            break;
        case CMD_LIGHT_DRL_ON:
            g_veh.light_flags |= STATUS_DRL_ON; break;
        case CMD_LIGHT_HEADLIGHT_LOW:
            g_veh.light_flags |= STATUS_HEADLIGHT_ON;
            g_veh.light_flags &= ~STATUS_HIGH_BEAM_ON;
            break;
        case CMD_LIGHT_HEADLIGHT_HIGH:
            g_veh.light_flags |= STATUS_HEADLIGHT_ON|STATUS_HIGH_BEAM_ON;
            break;
        case CMD_LIGHT_FOG_ON:
            g_veh.light_flags |= STATUS_FOG_ON; break;
        case CMD_LIGHT_FOG_OFF:
            g_veh.light_flags &= ~STATUS_FOG_ON; break;
        default: break;
    }
    
    uint8_t exec = 0;
    if (g_veh.light_flags & STATUS_DRL_ON)       exec |= EXEC_FRONT_DRL;
    if (g_veh.light_flags & STATUS_HEADLIGHT_ON) exec |= EXEC_FRONT_HEADLIGHT;
    if (g_veh.light_flags & STATUS_HIGH_BEAM_ON) exec |= EXEC_FRONT_HIGH_BEAM;
    if (g_veh.light_flags & STATUS_FOG_ON)       exec |= EXEC_FRONT_FOG;

    uint8_t d[2] = { exec, brightness };
    CAN_Send(CAN_ID_EXEC_FRONT_LIGHTS, d, 2U);
    Broadcast_LightState();
}

static void Execute_WiperCmd(CmdWiper_t mode, uint8_t spray)
{
    g_veh.wiper_mode = mode;
    uint8_t d[2] = { (uint8_t)mode, spray };
    CAN_Send(CAN_ID_EXEC_FRONT_WIPERS, d, 2U);
    Broadcast_LightState();
}

static void Execute_TurnCmd(CmdTurn_t cmd)
{
    g_veh.turn_armed = cmd;
    uint8_t exec = 0;
    switch (cmd) {
        case CMD_TURN_LEFT:   exec = EXEC_TURN_LEFT_ARM;  break;
        case CMD_TURN_RIGHT:  exec = EXEC_TURN_RIGHT_ARM; break;
        case CMD_TURN_HAZARD:
            exec = EXEC_TURN_HAZARD_ARM;
            g_veh.state_flags |= STATE_HAZARD_ACTIVE;
            g_veh.light_flags |= STATUS_HAZARD_ON;
            break;
        default:
            g_veh.state_flags &= ~STATE_HAZARD_ACTIVE;
            g_veh.light_flags &= ~(STATUS_HAZARD_ON|STATUS_TURN_LEFT_ON|STATUS_TURN_RIGHT_ON);
            break;
    }
    uint8_t d[1] = { exec };
    CAN_Send(CAN_ID_EXEC_FRONT_TURN, d, 1U);
    CAN_Send(CAN_ID_EXEC_REAR_TURN,  d, 1U);
    Broadcast_LightState();
}

static void Execute_TrunkCmd(CmdTrunk_t cmd)
{
    if      (cmd == CMD_TRUNK_OPEN)  { g_veh.trunk_state = TRUNK_OPENING;
                                       g_veh.state_flags |= STATE_TRUNK_AJAR; }
    else if (cmd == CMD_TRUNK_CLOSE) { g_veh.trunk_state = TRUNK_CLOSING; }
    else                             { g_veh.trunk_state = TRUNK_IDLE; }
    uint8_t d[1] = { (uint8_t)cmd };
    CAN_Send(CAN_ID_EXEC_REAR_TRUNK, d, 1U);
    Broadcast_TrunkState();
}

/* ── Diagnostic service handler ──────────────────────────────────────────── */
static void Handle_DiagRequest(const uint8_t *rx, uint8_t dlc)
{
    if (dlc < 1U) return;
    DiagCmd_t cmd = (DiagCmd_t)rx[0];
    uint8_t resp[8] = { (uint8_t)cmd, 0, 1, 0, 0, 0, 0, 0 };

    switch (cmd) {
        case DIAG_CMD_READ_DTC_COUNT:
            resp[3] = g_dtc_count;
            CAN_Send(CAN_ID_STATUS_DIAG_RESPONSE, resp, 4U);
            break;
        case DIAG_CMD_READ_ALL_DTCS:
            for (uint8_t i = 0; i < DTC_STORE_SIZE; i += 2U) {
                if (!g_dtc[i].active) break;
                resp[3] = UNPACK_HIGH_BYTE(g_dtc[i].dtc_code);
                resp[4] = UNPACK_LOW_BYTE (g_dtc[i].dtc_code);
                resp[5] = g_dtc[i].count;
                if (i+1U < DTC_STORE_SIZE && g_dtc[i+1U].active) {
                    resp[6] = UNPACK_HIGH_BYTE(g_dtc[i+1U].dtc_code);
                    resp[7] = UNPACK_LOW_BYTE (g_dtc[i+1U].dtc_code);
                }
                CAN_Send(CAN_ID_STATUS_DIAG_RESPONSE, resp, 8U);
                resp[1]++;
            }
            if (g_dtc_count == 0U) CAN_Send(CAN_ID_STATUS_DIAG_RESPONSE, resp, 4U);
            break;
        case DIAG_CMD_CLEAR_ALL_DTCS:
            memset(g_dtc, 0, sizeof(g_dtc));
            g_dtc_count = 0;
            g_veh.state_flags &= ~STATE_DTC_ACTIVE;
            resp[3] = 0xAAU;
            CAN_Send(CAN_ID_STATUS_DIAG_RESPONSE, resp, 4U);
            { uint8_t clr[2] = {0,0}; CAN_Send(CAN_ID_BANNER_CLEAR, clr, 2U); }
            break;
        case DIAG_CMD_NODE_STATUS: {
            uint32_t now = HAL_GetTick();
            resp[3] = NODE_ID_CENTRAL_ECU;
            resp[4] = ((now - g_hb_front) < BCM_TIMEOUT_MS) ? 0x01U : 0x00U;
            resp[5] = ((now - g_hb_rear)  < BCM_TIMEOUT_MS) ? 0x01U : 0x00U;
            CAN_Send(CAN_ID_STATUS_DIAG_RESPONSE, resp, 6U);
            break;
        }
        default: break;
    }
}

/* ── CAN RX callback (all GROUP A / D / E / G messages) ─────────────────── */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *phcan)
{
    if (phcan->Instance != CAN1) return;
    
    /* Toggle Green LED (PD12) on message receive */
    HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);

    CAN_RxHeaderTypeDef hdr;
    uint8_t d[8] = {0};
    if (HAL_CAN_GetRxMessage(phcan, CAN_RX_FIFO0, &hdr, d) != HAL_OK) return;
    if (hdr.IDE != CAN_ID_STD || hdr.RTR != CAN_RTR_DATA) return;

    uint16_t id  = (uint16_t)hdr.StdId;
    uint8_t  dlc = (uint8_t) hdr.DLC;
    ValidationResult_t res;

    switch (id) {
        /* GROUP A: commands from Qt */
        case CAN_ID_CMD_LIGHT_CONTROL:
            res = VALIDATION_OK;
            Send_ACK(0x00, ACK_STATUS_APPROVED, res);
            Execute_LightCmd((CmdLight_t)d[0], dlc > 1U ? d[1] : 100U);
            break;
        case CAN_ID_CMD_WIPER_CONTROL:
            Send_ACK(0x01, ACK_STATUS_APPROVED, VALIDATION_OK);
            Execute_WiperCmd((CmdWiper_t)d[0], dlc > 1U ? d[1] : 0U);
            break;
        case CAN_ID_CMD_TURN_SIGNAL:
            Send_ACK(0x02, ACK_STATUS_APPROVED, VALIDATION_OK);
            Execute_TurnCmd((CmdTurn_t)d[0]);
            break;
        case CAN_ID_CMD_TRUNK_CONTROL:
            res = Validate_TrunkCmd((CmdTrunk_t)d[0]);
            Send_ACK(0x03, res == VALIDATION_OK ? ACK_STATUS_APPROVED : ACK_STATUS_REJECTED, res);
            if (res == VALIDATION_OK) Execute_TrunkCmd((CmdTrunk_t)d[0]);
            break;
        case CAN_ID_CMD_DIAGNOSTIC:
            Handle_DiagRequest(d, dlc);
            break;
        /* GROUP D: BCM status */
        case CAN_ID_REPORT_FRONT_STATUS:
            if (dlc >= 1U) {
                if (d[0] & FRONT_ACT_HEADLIGHT) g_veh.light_flags |= STATUS_HEADLIGHT_ON;
                else                             g_veh.light_flags &= ~STATUS_HEADLIGHT_ON;
                if (d[0] & FRONT_ACT_DRL)       g_veh.light_flags |= STATUS_DRL_ON;
                else                             g_veh.light_flags &= ~STATUS_DRL_ON;
            }
            break;
        case CAN_ID_REPORT_REAR_STATUS:
            if (dlc >= 3U) {
                /* 1. Trunk status parsing */
                g_veh.trunk_state = (TrunkMotorState_t)d[1];
                g_veh.trunk_pct   = d[2];
                if (g_veh.trunk_state == TRUNK_IDLE && g_veh.trunk_pct == 0U)
                    g_veh.state_flags &= ~STATE_TRUNK_AJAR;
                else
                    g_veh.state_flags |= STATE_TRUNK_AJAR;
                Broadcast_TrunkState();

                /* 2. Turn Signal inputs from Rear BCM (hardware pivot) */
                if (g_veh.turn_armed != CMD_TURN_HAZARD) {
                    if (d[0] & REAR_ACT_LTURN) {
                        if (g_veh.turn_armed != CMD_TURN_LEFT) Execute_TurnCmd(CMD_TURN_LEFT);
                    } else if (d[0] & REAR_ACT_RTURN) {
                        if (g_veh.turn_armed != CMD_TURN_RIGHT) Execute_TurnCmd(CMD_TURN_RIGHT);
                    } else {
                        if (g_veh.turn_armed != CMD_TURN_OFF) Execute_TurnCmd(CMD_TURN_OFF);
                    }
                }
            }
            break;
        case CAN_ID_REPORT_REAR_SENSORS:
            if (dlc >= 4U) {
                g_veh.rear_dist_cm  = PACK_U16(d[0], d[1]);
                g_veh.parking_level = (ParkingLevel_t)d[3];
                Broadcast_RadarState();
            }
            break;
        case CAN_ID_REPORT_FRONT_SENSORS:
            break; /* reserved for future auto-light logic */
        /* GROUP E: BCM faults */
        case CAN_ID_FAULT_FRONT_BCM:
        case CAN_ID_FAULT_REAR_BCM:
            if (dlc >= 5U) {
                uint8_t src = (id == CAN_ID_FAULT_FRONT_BCM) ?
                               NODE_ID_FRONT_BCM : NODE_ID_REAR_BCM;
                Log_DTC(src, (DTC_Severity_t)d[0],
                        PACK_U16(d[1], d[2]), d[3], (SimpleErrorCode_t)d[4]);
            }
            break;
        /* GROUP G: BCM heartbeats */
        case CAN_ID_HEARTBEAT_FRONT_BCM:
            g_hb_front = HAL_GetTick(); break;
        case CAN_ID_HEARTBEAT_REAR_BCM:
            g_hb_rear  = HAL_GetTick(); break;
        default: break;
    }
}

/* ── CAN error callback ───────────────────────────────────────────────────── */
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *phcan)
{
    if (phcan->Instance != CAN1) return;
    if (HAL_CAN_GetError(phcan) & HAL_CAN_ERROR_BOF)
        Log_DTC(NODE_ID_CENTRAL_ECU, DTC_SEVERITY_CRITICAL,
                (uint16_t)DTC_C1001, 1U, ERR_CAN_BUS_OFF);
}

/* ── Timer period elapsed callback ──────────────────────────────────────── */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) {
        /* 100ms: read sensors, broadcast vehicle state, send heartbeat */
        Update_Sensors();
        Broadcast_VehicleState();
        Send_Heartbeat();
    }
    else if (htim->Instance == TIM7) {
        /* 500ms: blink tick + turn-blink phase to Qt */
        Send_BlinkTick();
    }
    else if (htim->Instance == TIM2) {
        /* 1000ms: BCM watchdog */
        uint32_t now = HAL_GetTick();
        if ((now - g_hb_front) > BCM_TIMEOUT_MS) {
            uint8_t wd[2] = { NODE_ID_FRONT_BCM, 1U };
            CAN_Send(CAN_ID_WATCHDOG_ALERT, wd, 2U);
            Log_DTC(NODE_ID_FRONT_BCM, DTC_SEVERITY_ERROR,
                    (uint16_t)DTC_C1002, 1U, ERR_NODE_TIMEOUT);
        }
        if ((now - g_hb_rear) > BCM_TIMEOUT_MS) {
            uint8_t wd[2] = { NODE_ID_REAR_BCM, 1U };
            CAN_Send(CAN_ID_WATCHDOG_ALERT, wd, 2U);
            Log_DTC(NODE_ID_REAR_BCM, DTC_SEVERITY_ERROR,
                    (uint16_t)DTC_C1003, 1U, ERR_NODE_TIMEOUT);
        }
    }
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
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_CAN1_Init();
  MX_TIM2_Init();
  MX_TIM6_Init();
  MX_TIM7_Init();
  /* USER CODE BEGIN 2 */

  /* Start ADC with DMA (circular, fills adc_buf[2] continuously) */
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buf, 2);

  /* Configure CAN RX filter banks */
  if (CAN_ConfigFilters() != HAL_OK) Error_Handler();

  /* Start CAN peripheral */
  if (HAL_CAN_Start(&hcan1) != HAL_OK) Error_Handler();

  /* Enable CAN interrupts: RX FIFO0 pending, error, bus-off */
  if (HAL_CAN_ActivateNotification(&hcan1,
      CAN_IT_RX_FIFO0_MSG_PENDING |
      CAN_IT_ERROR                |
      CAN_IT_BUSOFF) != HAL_OK) Error_Handler();

  /* Start all timers with interrupt */
  HAL_TIM_Base_Start_IT(&htim6);  /* 100ms — sensors + heartbeat */
  HAL_TIM_Base_Start_IT(&htim7);  /* 500ms — blink tick */
  HAL_TIM_Base_Start_IT(&htim2);  /* 1000ms — BCM watchdog */

  /* Seed watchdog timestamps so no false alerts on boot */
  g_hb_front = HAL_GetTick();
  g_hb_rear  = HAL_GetTick();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
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

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ENABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 2;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = 2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief CAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 6;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_11TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = ENABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = ENABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */

  /* USER CODE END CAN1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 8399;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 9999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 8399;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 99;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}

/**
  * @brief TIM7 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM7_Init(void)
{

  /* USER CODE BEGIN TIM7_Init 0 */

  /* USER CODE END TIM7_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM7_Init 1 */

  /* USER CODE END TIM7_Init 1 */
  htim7.Instance = TIM7;
  htim7.Init.Prescaler = 8399;
  htim7.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim7.Init.Period = 499;
  htim7.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim7) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim7, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM7_Init 2 */

  /* USER CODE END TIM7_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, LED_GREEN_Pin|LED_ORANGE_Pin|LED_RED_Pin|LED_BLUE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : BTN_HEADLIGHT_Pin */
  GPIO_InitStruct.Pin = BTN_HEADLIGHT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(BTN_HEADLIGHT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LED_GREEN_Pin LED_ORANGE_Pin LED_RED_Pin LED_BLUE_Pin */
  GPIO_InitStruct.Pin = LED_GREEN_Pin|LED_ORANGE_Pin|LED_RED_Pin|LED_BLUE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
#ifdef USE_FULL_ASSERT
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
