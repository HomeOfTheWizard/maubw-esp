# Atilla ESP32-S3 Controller Firmware

## Introduction
This project is the firmware for the main controller (ESP32-S3) in the Atilla hardware platform.
Atilla is a wearable motion capture device designed to help analyze and guide archery training sessions.

The firmware reads UWB distance frames, bridges UART traffic for debugging/integration, updates an OLED UI in real time, and advertises a BLE beacon identity.

## Technical Stack
- MCU and SDK:
  - ESP32-S3
  - ESP-IDF (CMake-based project)
- RTOS and concurrency:
  - FreeRTOS tasks (`pc_rx_task`, `uwb_rx_task`, NimBLE host task)
- Communication and buses:
  - UART (UWB module + PC bridge)
  - I2C master bus (OLED display path)
- Display/UI:
  - LVGL (`lvgl/lvgl`)
  - ESP LVGL port (`esp_lvgl_port`)
  - ESP LCD panel driver (`esp_lcd_sh1107` + SSD1306 panel init path)
- Wireless:
  - BLE with NimBLE host stack
  - Non-connectable beacon advertising with device name `atilla`
- Storage/service dependencies:
  - NVS Flash (required by NimBLE host storage)

## Code Architecture
The project is organized as a modular ESP-IDF `main` component:

- `main/main.c`
  - Runtime orchestrator and UWB domain logic
  - Initializes shared I2C master bus and passes bus handle to display module
  - Initializes BLE beacon module
  - Configures UART0/UART2
  - Runs and coordinates:
    - `pc_rx_task`: forwards PC UART traffic to UWB UART
    - `uwb_rx_task`: reads UWB stream, validates/parses frames, updates display
  - Contains stream-safe frame parser:
    - Frame markers: `DSTO_HEAD`, `DSTO_FOOT`
    - Payload length guard: `MAX_PAYLOAD_LEN`
    - XOR CRC validation over payload

- `main/src/display.c`
  - Display-only module
  - Builds SSD1306 panel I/O on an already initialized I2C bus
  - Initializes LVGL and display port
  - Owns UI label state and updates text via `display_update(...)`
  - Performs thread-safe UI updates with LVGL lock/unlock

- `main/src/ble_beacon.c`
  - BLE beacon module using NimBLE
  - Initializes NVS + NimBLE host
  - Configures GAP name as `atilla`
  - Starts non-connectable, general-discoverable BLE advertising from host sync callback

- `main/include/`
  - `display.h`: display module interface
  - `ble_beacon.h`: BLE beacon module interface
  - `common.h`: shared ESP includes and shared constants used across modules

- `main/CMakeLists.txt`
  - Registers all main component sources and include directories
  - Declares required component dependencies (`esp_driver_uart`, `esp_driver_i2c`, `bt`, `nvs_flash`)

## Feature Summary
- UWB frame reception and robust stream parsing
  - Handles partial frames and fake headers safely
  - Validates frame boundaries and XOR CRC before processing
- Distance extraction and on-device visualization
  - Parses nearby device entries from UWB range packets
  - Displays up to multiple device ID/distance rows on OLED
- UART bridge support for integration/debugging
  - Mirrors UWB data to PC UART
  - Forwards PC UART commands/data to UWB UART
- BLE beacon identity broadcast
  - Advertises as `atilla` using NimBLE
  - Useful for provisioning, discovery, and ecosystem integration
- Embedded-focused reliability patterns
  - Bounded buffers and overflow handling
  - Non-blocking task loops with short delays
  - Actionable logging for malformed frames and parser faults

## Build and Run (ESP-IDF)
Typical development workflow:

```bash
idf.py build
idf.py flash
idf.py monitor
```

If BLE/NimBLE options are disabled in your local config, enable them through menuconfig before building beacon features.
