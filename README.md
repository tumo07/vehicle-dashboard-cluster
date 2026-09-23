📡 Protocol Overview
To prevent bus collisions and guarantee deterministic real-time behavior, CAN IDs are strictly categorized by priority and direction:

GROUP A (0x100 - 0x10F): UI Commands (Qt -> Central ECU). Triggered by user interaction.

GROUP B (0x200 - 0x21F): Execution Commands (Central ECU -> BCMs). Approved and state-validated commands driving actuator pins.

GROUP C (0x300 - 0x30F): Status Updates (Central ECU -> Qt). Real-time telemetry (Speed, Fuel, Gear).

GROUP D (0x400 - 0x41F): BCM Reports (BCMs -> Central ECU). Physical inputs and raw sensor data.

GROUP E/F (0x500 - 0x61F): Diagnostics & Faults. OBD-style DTC (Diagnostic Trouble Codes) propagation.

GROUP G (0x700 - 0x720): Heartbeats. Sent at 1Hz - 5Hz. The Central ECU flags a DTC_C100X network fault if a node drops.

⚡ Hardware Pivot Highlight: Turn Signal Sync
To guarantee perfect 1:1 synchronization between physical LEDs and the software UI without latency, the system utilizes a Hardware Pivot logic:

Input: Driver presses the turn stalk wired to the Rear BCM.

Report: Rear BCM blindly reports the physical state via 0x410.

Process: Central ECU validates the input against Hazard states and issues an authorization bitmask via CAN_ID_EXEC_REAR_TURN (0x210).

Sync & Execute: Central ECU broadcasts a global 0x130 Blink Tick every 500ms. Upon receiving this tick, the Rear BCM hardware toggles the LEDs, while the Qt Dashboard simultaneously toggles the UI arrow.

🤝 Collaboration & Code Review
Pull Requests are required for merging into main. The .github/CODEOWNERS configuration ensures that the respective module owner is automatically requested for a code review before any integration.


---

### 2. File `README.md` dành riêng cho thư mục của bạn (`rear_bcm_firmware/`)
*Bạn tạo một file `README.md` mới tinh nằm ngay bên trong thư mục `rear_bcm_firmware` và dán đoạn này vào:*

```markdown
# 🚘 Rear BCM Firmware (Node 0x03)

This module contains the firmware for the **Rear Body Control Module (Rear BCM)**, built on the STM32F103C8T6 (Blue Pill) micro-controller. 

Operating under the **CAN V3.0 Central Coordinator Architecture**, this node acts as an intelligent peripheral that processes high-frequency sensor acquisitions locally and relies on the Central ECU for execution authorization.

## 🎯 Key Features & Algorithms

*   **Smart Kick-to-Open Trunk:** Integrates an HC-SR04 ultrasonic sensor with a 2-second debounce filter (`foot_in_zone` logic) to prevent false triggers. The mechanical movement is executed via a Non-blocking PWM Sweep algorithm, simulating a smooth 2-second power-liftgate sequence.
*   **Reverse Parking Radar (Multi-stage Warning):** Dynamically categorizes ultrasonic readings into `PARKING_CLEAR`, `CAUTION`, `WARNING`, and `CRITICAL` levels, driving a local buzzer and transmitting encoded states to the Central ECU for dashboard visualization.
*   **Hardware Pivot Turn Signals:** Implements remote-armed blinking logic. It listens for physical inputs, reports them (`0x410`), awaits execution masks from the Central ECU (`0x210`), and toggles LEDs strictly synced to the global `0x130` Blink Tick.
*   **Safety Watchdog:** Broadcasts a rolling-counter heartbeat (`0x720`) at 5Hz to maintain link integrity with the Central ECU.

## 🔌 Hardware Pinout (STM32F103C8T6)

| Pin | Type | Function |
| :--- | :---: | :--- |
| **PA0** | `Input` | Brake Pedal Switch |
| **PA1** | `Input` | Trunk Manual Switch |
| **PA4** | `Input` | Reverse Gear Simulation |
| **PA5 / PA6** | `Input` | Turn Signal Stalk (Left / Right) |
| **PA2** | `Output` | Parking Radar Buzzer (Low-level trigger) |
| **PB0** | `Output` | Dedicated Brake LED |
| **PB1 / PB3** | `Output` | Turn Signal LEDs (Left / Right) |
| **PB6** | `PWM` | SG90 Trunk Servo (TIM4 CH1) |
| **PB12 / PB13**| `GPIO` | HC-SR04 Trigger / Echo (Timed via TIM3) |
| **PB8 / PB9** | `CAN1` | RX / TX (AFIO Remap 2) |

## 📡 Node 0x03 CAN Dictionary

### Transmitting (TX)
*   `0x410` (DLC: 3) - **REPORT_REAR_STATUS**: Button states (Brake, Turn), Trunk motor motion state, and Opening percentage.
*   `0x411` (DLC: 4) - **REPORT_REAR_SENSORS**: Packed 16-bit ultrasonic distance (cm), Trunk Hall-effect state, and calculated Parking Level.
*   `0x720` (DLC: 2) - **HEARTBEAT_REAR_BCM**: Rolling counter and hardware health bitmask.

### Receiving (RX) - Interrupt Driven
*   `0x210` (DLC: 1) - **EXEC_REAR_TURN**: Arming mask from Central ECU (Left, Right, Hazard).
*   `0x211` (DLC: 1) - **EXEC_REAR_TRUNK**: Remote trunk commands (Open/Close from Qt).
*   `0x130` (DLC: 1) - **BLINK_TICK**: Global 500ms synchronization pulse.
