#include "wiper.h"
/*
 * headlight.c
 * Front BCM - Headlight & Fog Logic Controller
 * Map chuẩn chân:
 *   - PE15: Nút bấm Fog -> Xuất đèn PE12
 *   - PE14: Nút bấm Nguồn đèn chính (Pha/Cos)
 *   - PE13: Nút chuyển chế độ Cos (PB7) <-> Pha (PB8)
 */

#include "headlight.h"
#include "RX_data_UART.h"

static TIM_HandleTypeDef *pHLTim = NULL;
static ADC_HandleTypeDef *pHLAdc = NULL;

static uint8_t current_actuator_flags = 0;
static uint8_t current_brightness_pct = 100;

extern uint16_t adc_dma_buffer[2];

// Trạng thái các đèn
static uint8_t is_fog_on             = 0; // Đèn Fog (PE12)
static uint8_t is_headlight_power_on = 0; // Nguồn cụm Pha/Cos
static uint8_t is_high_beam_selected = 0; // 0: Cos (PB7), 1: Pha (PB8)

// Biến lưu trạng thái nút bấm
static uint8_t  lastPin13State       = 1;
static uint8_t  lastPin14State       = 1;
static uint8_t  lastPin15State       = 1;
static uint32_t lastDebounceTimeBtn   = 0;
static uint32_t lastAdcReadTime       = 0;

static uint8_t Headlight_Read_Potentiometer(void)
{
    uint16_t raw = adc_dma_buffer[1];
    extern ADC_HandleTypeDef hadc1;
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_dma_buffer, 2);
    uint8_t pct = (uint8_t)((raw * 100) / 4095);
    if (pct < 20) pct = 20; // Min brightness to prevent headlights from turning fully off and looking broken
    return pct;
}

static void Headlight_Set_PWM_Pulse(uint32_t channel, uint8_t brightness_pct)
{
    if (pHLTim == NULL) return;
    if (brightness_pct > 100) brightness_pct = 100;

    uint32_t pulse = (uint32_t)((brightness_pct * (uint32_t)HL_PWM_MAX_PERIOD) / 100UL);
    __HAL_TIM_SET_COMPARE(pHLTim, channel, pulse);
}

void Headlight_Init(TIM_HandleTypeDef *htim, ADC_HandleTypeDef *hadc)
{
    pHLTim = htim;
    pHLAdc = hadc;

    if (pHLTim != NULL) {
        __HAL_TIM_SET_COMPARE(pHLTim, HL_TIM_CHANNEL_COS, 0); // PB7 - CH2
        __HAL_TIM_SET_COMPARE(pHLTim, HL_TIM_CHANNEL_PHA, 0); // PB8 - CH3
        HAL_TIM_PWM_Start(pHLTim, HL_TIM_CHANNEL_COS);
        HAL_TIM_PWM_Start(pHLTim, HL_TIM_CHANNEL_PHA);
    }

    is_fog_on             = 0;
    is_headlight_power_on = 0;
    is_high_beam_selected = 0;
    current_actuator_flags = 0;
    current_brightness_pct = 100;

    // Xuất dứt khoát: Tắt Fog (PE12), Bật DRL (PE11)
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_SET);

    lastPin13State = HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_13);
    lastPin14State = HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_14);
    lastPin15State = HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_15);

    lastDebounceTimeBtn = HAL_GetTick();
    lastAdcReadTime     = HAL_GetTick();
}

void Headlight_Task(void)
{
    uint32_t currentTick = HAL_GetTick();

    // 1. Đọc ADC Dimmer mỗi 100ms
    if (currentTick - lastAdcReadTime >= 100) {
        current_brightness_pct = Headlight_Read_Potentiometer();
        lastAdcReadTime = currentTick;
    }

    // 2. Quét 3 nút bấm vật lý (Chu kỳ 30ms)
    if (currentTick - lastDebounceTimeBtn >= 30) {
        uint8_t pin13 = HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_13); // Nút chuyển Pha/Cos
        uint8_t pin14 = HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_14); // Nút nguồn Pha-Cos
        uint8_t pin15 = HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_15); // Nút bật/tắt Fog

        // NÚT 1 (PE15): BẬT / TẮT ĐÈN FOG
        if (pin15 == GPIO_PIN_RESET && lastPin15State == GPIO_PIN_SET) {
            is_fog_on = !is_fog_on;
        }

        // NÚT 2 (PE14): BẬT / TẮT NGUỒN CỤM PHA - COS
        if (pin14 == GPIO_PIN_RESET && lastPin14State == GPIO_PIN_SET) {
            is_headlight_power_on = !is_headlight_power_on;
        }

        // NÚT 3 (PE13): CHUYỂN CHẾ ĐỘ COS (PB7) <-> PHA (PB8)
        if (pin13 == GPIO_PIN_RESET && lastPin13State == GPIO_PIN_SET) {
            if (is_headlight_power_on) {
                is_high_beam_selected = !is_high_beam_selected;
            }
        }

        lastPin13State      = pin13;
        lastPin14State      = pin14;
        lastPin15State      = pin15;
        lastDebounceTimeBtn = currentTick;
    }

    // 3. Phân luồng công suất Cos / Pha
    uint8_t is_cos_active = 0;
    uint8_t is_pha_active = 0;

    if (is_headlight_power_on) {
        if (is_high_beam_selected) {
            is_pha_active = 1; // Sáng Pha PB8
            is_cos_active = 1; // FIXED: Keep Low Beam ON when High Beam is ON!
        } else {
            is_cos_active = 1; // Sáng Cos PB7
            is_pha_active = 0;
        }
    }

    // 4. DRL: Tu tat khi bat Pha hoac Cos, Tu bat khi tat den chinh
    uint8_t is_drl_on = 1; // FIXED: DRL stays ON permanently

    // --- INTEGRATE CAN COMMANDS FROM CENTRAL ECU ---
    extern volatile uint8_t f411_cmd_light_mask;
    extern volatile uint8_t f411_cmd_light_brightness;
    
    if (f411_cmd_light_mask & (1<<0)) is_drl_on = 1;     // EXEC_FRONT_DRL
    if (f411_cmd_light_mask & (1<<1)) is_cos_active = 1; // EXEC_FRONT_HEADLIGHT
    if (f411_cmd_light_mask & (1<<2)) { is_pha_active = 1; is_cos_active = 1; } // EXEC_FRONT_HIGH_BEAM (keeps Low on)
    if (f411_cmd_light_mask & (1<<3)) is_fog_on = 1;     // EXEC_FRONT_FOG
    
    uint8_t final_brightness = current_brightness_pct;
    if (f411_cmd_light_brightness > 0) final_brightness = f411_cmd_light_brightness;

    // AUTO FOG LOGIC: Sensor overrides button activation
    if (Get_RainSensor_Percent() > 0) {
        is_fog_on = 1;
    }

    // 5. Xuat phan cong chap hanh
    Headlight_Set_PWM_Pulse(HL_TIM_CHANNEL_COS, is_cos_active ? final_brightness : 0); // PB7
    Headlight_Set_PWM_Pulse(HL_TIM_CHANNEL_PHA, is_pha_active ? final_brightness : 0); // PB8

    // Xuat truc tiep Port E cho Fog (PE12) va DRL (PE11)
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, is_fog_on ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, is_drl_on ? GPIO_PIN_SET : GPIO_PIN_RESET);

    // 6. Cap nhat co CAN 0x400
    current_actuator_flags = 0;
    if (is_drl_on)      current_actuator_flags |= 0x01; // bit0 = FRONT_ACT_DRL
    if (is_cos_active)  current_actuator_flags |= 0x02; // bit1 = FRONT_ACT_HEADLIGHT (Low Beam)
    if (is_fog_on)      current_actuator_flags |= 0x04; // bit2 = FRONT_ACT_FOG
    if (is_pha_active)  current_actuator_flags |= 0x80; // bit7 = HIGH_BEAM (separate from Low Beam)
}


uint8_t Headlight_Get_Actuator_Flags(void)
{
    return current_actuator_flags;
}

uint8_t Headlight_Get_Current_Brightness(void)
{
    return current_brightness_pct;
}





