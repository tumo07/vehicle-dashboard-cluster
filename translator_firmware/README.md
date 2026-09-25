# Translator Firmware

Firmware for the **CAN-UART Gateway** in the **Smart Vehicle Dashboard Cluster** project. The translator runs on an STM32F103C8T6 Blue Pill and provides bidirectional communication between the Qt desktop application and the vehicle CAN network.

## System Overview

The translator operates as a communication bridge. It does not implement the vehicle control state machine.

```text
Qt Dashboard / PC
       |
       | UART 115200, 8N1
       v
STM32F103C8T6 Blue Pill
       |
       | bxCAN 500 kbit/s
       v
CAN Transceiver
       |
       +---------------- CANH / CANL ----------------+
       |                                              |
 Central ECU                                     BCM Nodes
```

Communication flow:

```text
Qt -> UART -> Translator -> CAN -> Central ECU
Central ECU -> CAN -> Translator -> UART -> Qt
```

The Central ECU validates user commands, manages system state, and sends approved execution commands to the Front BCM or Rear BCM.

## Main Features

- Receives 11-bit Standard CAN Data Frames from the CAN bus.
- Filters CAN messages using the bxCAN hardware filter banks.
- Converts received CAN frames into UART text messages for the Qt application.
- Receives UART commands from the computer using interrupts.
- Parses and validates UART commands before transmitting CAN frames.
- Allows the computer to transmit only command IDs from `0x100` to `0x104`.
- Validates CAN ID, DLC, payload length, and hexadecimal characters.
- Returns ACK or NACK responses for UART commands.
- Monitors and reports CAN controller errors.
- Supports CAN Normal mode and optional Loopback mode for testing.

## Project Structure

```text
translator_firmware/
├── README.md
└── CAN_UART_Gateway/
    ├── Core/
    │   ├── Inc/
    │   │   ├── main.h
    │   │   ├── can_messages.h
    │   │   ├── can_gateway.h
    │   │   └── uart_protocol.h
    │   └── Src/
    │       ├── main.c
    │       ├── can_gateway.c
    │       └── uart_protocol.c
    ├── Drivers/
    ├── Startup/
    ├── CAN_UART_Gateway.ioc
    ├── .project
    ├── .cproject
    └── STM32F103C8TX_FLASH.ld
```

### Module Responsibilities

- `main.c`: Initializes HAL, system clock, GPIO, CAN, and UART, then runs the high-level processing loop.
- `can_gateway.c/.h`: Manages CAN filters, CAN reception, command transmission, validation, and CAN error handling.
- `uart_protocol.c/.h`: Manages UART reception, command parsing, timeout handling, ACK/NACK responses, and CAN-to-UART formatting.
- `can_messages.h`: Single source of truth for CAN IDs, DLC definitions, enums, bit masks, status values, and diagnostic codes.

## Hardware Requirements

- STM32F103C8T6 Blue Pill.
- ST-Link V2 or compatible programmer/debugger.
- 3.3 V CAN transceiver, such as SN65HVD230.
- USB-to-UART adapter, such as CP2102 or CH340.
- Two 120-ohm termination resistors installed at the two physical ends of the CAN bus.
- CANH, CANL, and reference GND wiring.

> Do not connect CANH or CANL directly to STM32 GPIO pins. PA11 and PA12 must connect to the logic-side RXD and TXD pins of a CAN transceiver.

## Wiring

### Blue Pill to CAN Transceiver

```text
Blue Pill              CAN Transceiver
---------------------------------------
3.3V        ---------> VCC
GND         <--------> GND
PA12 CAN_TX ---------> TXD / CTX
PA11 CAN_RX <--------- RXD / CRX
                       CANH <----> CANH bus
                       CANL <----> CANL bus
```

### Blue Pill to USB-UART Adapter

```text
Blue Pill              USB-UART
--------------------------------
PA9  USART1_TX ------> RX
PA10 USART1_RX <------ TX
GND              <--> GND
```

## Clock Configuration

```text
HSE       = 8 MHz
PLL       = x9
SYSCLK    = 72 MHz
HCLK      = 72 MHz
PCLK1     = 36 MHz
PCLK2     = 72 MHz
```

## CAN Configuration

```text
Mode                     = Normal
Frame format             = Standard 11-bit ID
Bit rate                 = 500 kbit/s
Prescaler                = 9
Synchronization Jump Width = 1 TQ
Bit Segment 1            = 6 TQ
Bit Segment 2            = 1 TQ
Sample point             = 87.5%
Automatic Bus-Off        = Enabled
Automatic Retransmission = Enabled
Receive FIFO             = FIFO0
```

Bit-rate calculation:

```text
CAN bit rate = 36 MHz / [9 x (1 + 6 + 1)]
             = 500 kbit/s
```

## UART Configuration

```text
Peripheral   = USART1
Baud rate    = 115200
Data bits    = 8
Parity       = None
Stop bits    = 1
Flow control = None
Mode         = TX/RX
```

## UART Protocol

### Computer-to-CAN Command

Format:

```text
TX:<CAN_ID_HEX>:<DLC_DEC>:<DATA_HEX>\r\n
```

Rules:

- The CAN ID must be a Standard ID between `0x000` and `0x7FF`.
- DLC is a decimal value from 1 to 8.
- The payload must contain exactly `DLC x 2` hexadecimal characters.
- A command ends with CR, LF, or CRLF.
- The translator accepts only Qt-to-Central-ECU command IDs.

Example:

```text
TX:103:1:01
```

Meaning:

```text
CAN ID = 0x103
DLC    = 1
DATA   = 01
Action = Request trunk opening
```

### CAN-to-Computer Message

Format:

```text
CAN:<CAN_ID_HEX>:<DLC_DEC>:<DATA_HEX>\r\n
```

Example:

```text
CAN:303:2:2D01
```

Meaning:

```text
CAN ID = 0x303
DLC    = 2
Byte 0 = 0x2D = 45 percent open
Byte 1 = 0x01 = TRUNK_OPENING
```

### UART Responses

```text
ACK:TX
NACK:FORMAT
NACK:ID
NACK:DLC
NACK:DATA
NACK:LENGTH
NACK:HEX
NACK:COMMAND
NACK:EXTRA_FIELD
NACK:UART_TIMEOUT
NACK:BUSY
NACK:CAN
```

## CAN IDs Allowed from Qt

The translator allows Qt to send only the following commands to the Central ECU:

| CAN ID | Message | DLC | Description |
|---|---|---:|---|
| `0x100` | Light Control | 2 | Light mode and brightness from 0 to 100 percent |
| `0x101` | Wiper Control | 2 | Wiper mode and washer spray request |
| `0x102` | Turn Signal | 1 | Off, left, right, or hazard |
| `0x103` | Trunk Control | 1 | No action, open, close, or stop |
| `0x104` | Diagnostic Command | 2-8 | Diagnostic command and parameters |

Qt must not transmit BCM execution IDs such as `0x200`, `0x210`, or `0x211` directly. The Central ECU validates the system state before sending execution commands to the BCM nodes.

## CAN IDs Forwarded to Qt

| CAN ID | Message | Purpose |
|---|---|---|
| `0x105` | Command ACK | Central ECU command validation result |
| `0x300` | Vehicle State | Gear, state flags, speed, fuel, temperature, and battery voltage |
| `0x301` | Lights State | Current light and wiper state |
| `0x302` | Turn Blink | Left and right turn-signal phase |
| `0x303` | Trunk State | Trunk opening percentage and motor state |
| `0x304` | Reverse Radar | Distance and parking warning level |
| `0x305` | Diagnostic Response | Multi-frame diagnostic response |
| `0x600` | Fault Banner | Fault notification for the Qt dashboard |
| `0x601` | Banner Clear | Clears one or all fault banners |
| `0x610` | Watchdog Alert | Reports a node heartbeat timeout |
| `0x700` | Central ECU Heartbeat | Central ECU health status |
| `0x710` | Front BCM Heartbeat | Front BCM health status |
| `0x720` | Rear BCM Heartbeat | Rear BCM health status |

Detailed payload definitions are available in `CAN_UART_Gateway/Core/Inc/can_messages.h`.

## Command Examples

### Open the Trunk

```text
TX:103:1:01
```

### Close the Trunk

```text
TX:103:1:02
```

### Stop the Trunk Motor

```text
TX:103:1:03
```

### Enable the Left Turn Signal

```text
TX:102:1:01
```

### Enable Hazard Lights

```text
TX:102:1:03
```

### Enable Low-Beam Headlights at 80 Percent Brightness

```text
TX:100:2:0250
```

### Set Slow Wiper Mode without Washer Spray

```text
TX:101:2:0200
```

## Loopback Test Mode

Loopback mode verifies the CAN controller, hardware filters, FIFO0 interrupt, callback routing, and UART output without requiring a CAN transceiver or a second CAN node.

In STM32CubeMX:

```text
Connectivity -> CAN -> Mode -> Loopback
```

Verify the generated initialization code:

```c
hcan.Init.Mode = CAN_MODE_LOOPBACK;
```

After testing, restore production mode:

```c
hcan.Init.Mode = CAN_MODE_NORMAL;
```

Optional loopback test code should be protected by a compile-time macro and disabled in production builds.

## Building the Firmware

1. Open `CAN_UART_Gateway/CAN_UART_Gateway.ioc` in STM32CubeIDE.
2. Verify the system clock, CAN, USART1, GPIO, and NVIC settings.
3. Generate code if the IOC configuration was changed.
4. Select `Project -> Clean`.
5. Select `Project -> Build Project`.
6. Confirm that the build completes with `0 errors` and `0 warnings`.
7. Program the Blue Pill using ST-Link.

## Required NVIC Interrupts

Enable both interrupts:

```text
USB low priority or CAN RX0 interrupt
USART1 global interrupt
```

`stm32f1xx_it.c` must call the corresponding HAL handlers:

```c
void USB_LP_CAN1_RX0_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&hcan);
}

void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}
```

## Quick Tests

### CAN to UART

Send the following frame from a CAN analyzer or the Central ECU:

```text
ID   = 0x303
DLC  = 2
DATA = 2D 01
```

Expected terminal output:

```text
CAN:303:2:2D01
```

### UART to CAN

Send the following line from a serial terminal or Qt application:

```text
TX:103:1:01
```

Expected UART response:

```text
ACK:TX
```

Expected CAN frame:

```text
ID   = 0x103
DLC  = 1
DATA = 01
```

### Invalid Command

```text
TX:103:1:FF
```

Expected response:

```text
NACK:COMMAND
```

## Error Handling and Current Limitations

- UART RX uses a fixed-size command buffer and an inter-byte timeout.
- CAN payload length is limited to 8 bytes.
- CAN TX checks that a mailbox is available before transmission.
- CAN RX drains all pending frames from FIFO0 when the receive callback runs.
- FIFO0 overrun, error warning, error passive, and bus-off notifications are enabled.
- The current implementation may still use blocking UART transmission when forwarding CAN frames.
- A production version should use a UART TX ring buffer and DMA to reduce the risk of frame loss under high CAN bus load.
- The current command receiver stores one complete UART command at a time. The Qt application should wait for ACK or NACK before sending the next command.

## Files Tracked by Git

Keep the following project files under version control:

```text
*.ioc
.project
.cproject
Core/
Drivers/
Startup/
*.ld
```

Do not commit generated build output:

```text
Debug/
Release/
*.o
*.d
*.elf
*.bin
*.hex
*.map
```

## Contribution Guidelines

1. Create a branch from `main` or the designated integration branch.
2. Build the firmware successfully before committing.
3. Do not commit generated build output.
4. Use clear commit messages, for example:

```text
feat: add CAN UART command parser
fix: handle CAN FIFO overrun
refactor: split CAN gateway and UART protocol modules
docs: update translator firmware README
```

5. Open a pull request describing the changes, test procedure, and results.

## Development Status

- [x] CAN Normal mode at 500 kbit/s.
- [x] Standard 11-bit CAN identifiers.
- [x] Hardware CAN filters.
- [x] CAN RX FIFO0 interrupt.
- [x] UART RX interrupt.
- [x] UART command parser.
- [x] CAN command validation.
- [x] UART ACK/NACK responses.
- [x] Separate `can_gateway` and `uart_protocol` modules.
- [ ] UART TX DMA and ring buffer.
- [ ] Independent watchdog.
- [ ] High-load testing.
- [ ] Long-duration testing on the physical CAN bus.

## Related Files

- `CAN_UART_Gateway/Core/Inc/can_messages.h`: CAN message and payload specification.
- `CAN_UART_Gateway/Core/Inc/can_gateway.h`: CAN gateway public API.
- `CAN_UART_Gateway/Core/Inc/uart_protocol.h`: UART protocol API and settings.
- `CAN_UART_Gateway/CAN_UART_Gateway.ioc`: STM32CubeMX configuration.

## License

This firmware is part of the Smart Vehicle Dashboard Cluster repository. Refer to the repository root `LICENSE` file when available.
