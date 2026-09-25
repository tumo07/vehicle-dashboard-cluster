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

    // 3. XỬ LÝ ĐIỀU PHỐI ĐÈN: CENTRAL COORDINATOR COMMAND (0x200) & FAIL-SAFE
    extern volatile uint8_t  f411_cmd_light_mask;
    extern volatile uint8_t  f411_cmd_light_brightness;
    extern volatile uint32_t f411_last_can_cmd_tick;
    extern volatile uint8_t  f411_can_connected;
    extern volatile uint8_t  f411_failsafe_active;

    uint8_t is_cos_active    = 0;
    uint8_t is_pha_active    = 0;
    uint8_t is_drl_on        = 1; // DRL (Daytime Running Light) luôn duy trì khi bật khóa điện
    uint8_t final_brightness = current_brightness_pct;

    // Kiểm tra mất kết nối Bus CAN > 1200ms -> Kích hoạt ASIL-B Fail-Safe
    if (f411_can_connected && (currentTick - f411_last_can_cmd_tick > 1200)) {
        f411_failsafe_active = 1;
    }

    if (f411_failsafe_active) {
        // [ISO 26262 ASIL-B FAIL-SAFE MODE]
        // Khi mất mạng CAN hoàn toàn: Tự động bật đèn chiếu gần (Low Beam) 100% công suất
        // để lái xe quan sát an toàn và tấp vào lề đường, tắt đèn pha và sương mù.
        is_cos_active    = 1;
        is_pha_active    = 0;
        is_fog_on        = 0;
        is_drl_on        = 1;
        final_brightness = 100;
    } else if (f411_can_connected) {
        // [CAN v3.0 CENTRAL COORDINATOR MODE - PURE EDGE EXECUTION]
        // Chấp hành 100% lệnh từ bản tin 0x200 CAN_ID_EXEC_FRONT_LIGHTS
        // Bit 0: DRL, Bit 1: Low Beam, Bit 2: High Beam, Bit 3: Fog Lamp
        is_drl_on     = (f411_cmd_light_mask & FRONT_LIGHT_DRL) ? 1 : 1;
        is_cos_active = (f411_cmd_light_mask & FRONT_LIGHT_LOW_BEAM) ? 1 : 0;
        is_pha_active = (f411_cmd_light_mask & FRONT_LIGHT_HIGH_BEAM) ? 1 : 0;
        if (is_pha_active) {
            is_cos_active = 1; // Tiêu chuẩn: Bật pha vẫn duy trì cos chiếu gần
        }
        is_fog_on = (f411_cmd_light_mask & FRONT_LIGHT_FOG) ? 1 : 0;

        if (f411_cmd_light_brightness > 0) {
            final_brightness = f411_cmd_light_brightness;
        }
    } else {
        // [STANDALONE BENCH TEST MODE - Không có CAN]
        if (is_headlight_power_on) {
            if (is_high_beam_selected) {
                is_pha_active = 1;
                is_cos_active = 1;
            } else {
                is_cos_active = 1;
                is_pha_active = 0;
            }
        }
    }

    // 4. Xuất phân công chấp hành phần cứng
    Headlight_Set_PWM_Pulse(HL_TIM_CHANNEL_COS, is_cos_active ? final_brightness : 0); // PB7 - Cos
    Headlight_Set_PWM_Pulse(HL_TIM_CHANNEL_PHA, is_pha_active ? final_brightness : 0); // PB8 - Pha

    // Xuất trực tiếp GPIO Port E cho Fog (PE12) và DRL (PE11)
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, is_fog_on ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, is_drl_on ? GPIO_PIN_SET : GPIO_PIN_RESET);

    // 5. Cập nhật cờ báo cáo trạng thái CAN 0x400 (Report Front Status)
    current_actuator_flags = 0;
    if (is_drl_on)      current_actuator_flags |= FRONT_ACT_DRL;       // Bit 0 (0x01)
    if (is_cos_active)  current_actuator_flags |= FRONT_ACT_HEADLIGHT; // Bit 1 (0x02) - Low Beam
    if (is_fog_on)      current_actuator_flags |= FRONT_ACT_FOG;       // Bit 2 (0x04)
    if (is_pha_active)  current_actuator_flags |= FRONT_ACT_HIGHBEAM;  // Bit 7 (0x80) - High Beam
}


uint8_t Headlight_Get_Actuator_Flags(void)
{
    return current_actuator_flags;
}

uint8_t Headlight_Get_Current_Brightness(void)
{
    return current_brightness_pct;
}





