# Central ECU BluePill — STM32F103C8T6 Firmware

## Overview

This is the **Central ECU** firmware for the Smart Vehicle Dashboard Cluster project (CAN v3.0).
The Central ECU is the **sole coordinator and decision-maker** on the CAN bus.

**Hardware:** Blue Pill board (STM32F103C8T6)
**IDE:** STM32CubeIDE  
**HAL Library:** STM32F1xx HAL  
**CAN Speed:** 500 kbps  

---

## Architecture

```
Qt Dashboard ──(GROUP A 0x10x)──► Central ECU (this node)
                                       │
                     ┌─────────────────┼──────────────────┐
                     │                 │                  │
             (GROUP B 0x2xx)   (GROUP C 0x3xx)   (GROUP F 0x6xx)
                     │           (to Qt)           (banners)
                     ▼
              Front BCM / Rear BCM
              (GROUP D 0x4xx reports back)
              (GROUP E 0x5xx fault DTCs)
              (GROUP G 0x7xx heartbeats)
```

---

## STM32CubeMX Settings

### Project
| Setting | Value |
|---------|-------|
| MCU | STM32F103C8Tx |
| Language | C |
| Toolchain | STM32CubeIDE |

### RCC
| Setting | Value |
|---------|-------|
| HSE | Crystal/Ceramic Resonator |
| PLL Source | HSE |
| PLL Multiplier | x9 |
| SYSCLK | 72 MHz |
| AHB Prescaler | /1 → HCLK = 72 MHz |
| APB1 Prescaler | /2 → PCLK1 = 36 MHz |
| APB2 Prescaler | /1 → PCLK2 = 72 MHz |
| ADC Prescaler | /6 → ADC CLK = 12 MHz |

### CAN1
| Setting | Value |
|---------|-------|
| Mode | Activated |
| Pin Remap | PB8 (RX), PB9 (TX) — AFIO Remap 2 |
| Prescaler | 9 |
| Time Quanta Bit Seg 1 | 6 TQ |
| Time Quanta Bit Seg 2 | 1 TQ |
| ReSynchronization Jump Width | 1 TQ |
| **Resulting Bit Rate** | **500 kbps** |
| Auto Bus-Off Management | Enable |
| Automatic Retransmission | Enable |
| Operating Mode | Normal |

> **Important:** `__HAL_AFIO_REMAP_CAN1_2()` is called in `MX_CAN_Init()` BEFORE `HAL_CAN_Init()` to apply the PB8/PB9 remap.

### ADC1
| Setting | Value |
|---------|-------|
| IN0 (PA0) | Rank 1 — speed potentiometer |
| IN1 (PA1) | Rank 2 — fuel potentiometer |
| Scan Conversion Mode | Enabled |
| Continuous Conversion Mode | Enabled |
| DMA Requests | Enabled → DMA1 Channel 1 |
| Data Alignment | Right |
| Number of Conversions | 2 |
| Sampling Time (both) | 55.5 Cycles |

### DMA
| Setting | Value |
|---------|-------|
| Channel | DMA1 Channel 1 |
| Direction | Peripheral to Memory |
| Mode | Circular |
| Peripheral Data Width | Half Word (16-bit) |
| Memory Data Width | Half Word (16-bit) |
| Memory Increment | Enabled |

### USART1
| Setting | Value |
|---------|-------|
| Mode | Asynchronous |
| TX Pin | PA9 |
| RX Pin | PA10 |
| Baud Rate | 115200 |
| Word Length | 8 Bits |
| Stop Bits | 1 |
| Parity | None |
| Hardware Flow Control | None |

### TIM2 (100 ms — sensors + heartbeat)
| Setting | Value |
|---------|-------|
| Clock Source | Internal Clock |
| Prescaler (PSC) | 35999 |
| Counter Period (ARR) | 199 |
| **Resulting Period** | **100 ms** |
| Trigger interrupt | Enabled |

> Calculation: TIM2 clock = 2×APB1 = 72 MHz; tick = 72MHz/36000 = 2000 Hz; period = 200/2000 = 100 ms

### TIM3 (500 ms — blink tick)
| Setting | Value |
|---------|-------|
| Clock Source | Internal Clock |
| Prescaler (PSC) | 35999 |
| Counter Period (ARR) | 999 |
| **Resulting Period** | **500 ms** |
| Trigger interrupt | Enabled |

### TIM4 (1000 ms — BCM watchdog)
| Setting | Value |
|---------|-------|
| Clock Source | Internal Clock |
| Prescaler (PSC) | 35999 |
| Counter Period (ARR) | 1999 |
| **Resulting Period** | **1000 ms** |
| Trigger interrupt | Enabled |

### NVIC Settings
| Interrupt | Priority | Sub-Priority | Notes |
|-----------|----------|--------------|-------|
| USB_LP_CAN1_RX0 | 0 | 0 | CAN RX - highest priority |
| TIM2 | 1 | 0 | Sensor broadcast |
| TIM3 | 1 | 1 | Blink tick |
| TIM4 | 1 | 2 | BCM watchdog |
| DMA1 Channel1 | 2 | 0 | ADC DMA |

### GPIO
| Pin | Mode | Pull | Label |
|-----|------|------|-------|
| PA0 | Analog | None | ADC1_CH0 Speed pot |
| PA1 | Analog | None | ADC1_CH1 Fuel pot |
| PA2 | Input | Pull-Down | BTN_HEADLIGHT (active HIGH) |
| PA9 | AF_PP | None | USART1_TX |
| PA10 | Input | None | USART1_RX |
| PB0 | Output_PP | None | LED_GREEN (speed < 60) |
| PB1 | Output_PP | None | LED_ORANGE (60-99 km/h) |
| PB8 | AF_Input | None | CAN1_RX (AFIO remap) |
| PB9 | AF_PP | None | CAN1_TX (AFIO remap) |
| PB12 | Output_PP | None | LED_RED (speed >= 100) |
| PC13 | Output_PP | None | LED_CAN_TX (active LOW, onboard) |

---

## Include Path Configuration

In STM32CubeIDE, add to **C/C++ include paths**:

```
../../../../can_messages_shared
```

Or relative from `Core/Src/`:
```
../../can_messages_shared
```

This allows `#include "can_messages.h"` to resolve to the shared header.

---

## CAN Message Groups

| Group | Direction | IDs | Description |
|-------|-----------|-----|-------------|
| A | Qt → Central ECU | 0x100–0x104 | User commands |
| A | Central ECU → Qt | 0x105 | ACK/NACK |
| B | Central ECU → BCMs | 0x200–0x211 | Approved commands |
| C | Central ECU → Qt | 0x300–0x305 | Real-time status |
| D | BCMs → Central ECU | 0x400–0x411 | BCM status reports |
| E | BCMs → Central ECU | 0x500, 0x510 | Fault/DTC reports |
| F | Central ECU → Qt | 0x600–0x610 | Banners + watchdog alerts |
| G | All nodes | 0x700–0x720 | Heartbeats |
| — | Central ECU → BCMs | 0x130 | Blink tick sync |

---

## Files

| File | Description |
|------|-------------|
| `Core/Inc/main.h` | Pin defines, Error_Handler prototype |
| `Core/Inc/stm32f1xx_hal_conf.h` | HAL module enables (TIM, CAN, ADC, UART, DMA) |
| `Core/Inc/stm32f1xx_it.h` | ISR function prototypes |
| `Core/Src/main.c` | Full Central ECU firmware |
| `Core/Src/stm32f1xx_it.c` | ISR implementations (CAN, TIM2/3/4, DMA) |

---

## Build Notes

1. Ensure the startup file `startup_stm32f103c8tx.s` is in `Core/Startup/`
2. Ensure `system_stm32f1xx.c` is in `Core/Src/`
3. The linker script must target STM32F103C8 (64KB flash, 20KB RAM)
4. Link against `STM32F1xx_HAL_Driver` with the modules listed in `stm32f1xx_hal_conf.h`
