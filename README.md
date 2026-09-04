# Smart Vehicle Dashboard Cluster

This repository contains the complete firmware and software for the Smart Vehicle Dashboard Cluster project.

## Architecture

The project is structured as a monolithic repository (monorepo) segmented into 5 distinct modules, plus a shared CAN definitions folder. This allows the team of 5 to collaborate easily while defining clear ownership boundaries via the `.github/CODEOWNERS` file.

## Repository Structure

1. **`qt_dashboard/`** - PC UI Dashboard (C++ / Qt6 / QML / SQLite / MQTT)
2. **`translator_firmware/`** - CAN to UART Bridge (STM32F103C8T6)
3. **`front_bcm_firmware/`** - Front Actuators & Lights (STM32F103C8T6)
4. **`rear_bcm_firmware/`** - Rear Actuators, Trunk Servo & HC-SR04 Radar (STM32F103C8T6)
5. **`main_mcu_firmware/`** - Central Sensor MCU & Error Aggregator (STM32F407VGT6 Discovery)
6. **`can_messages_shared/`** - Shared CAN ID definitions, DLCs, and bitmasks used by all C/C++ projects.

## Collaboration

Each team member is responsible for their specific module. When you push code or open a Pull Request, GitHub will automatically request a code review from the owner of that specific directory.
