---
description: "Use when working on this ESP-IDF firmware project (ESP32-S3, UART/UWB frame parsing, LVGL OLED UI, FreeRTOS tasks, and NimBLE beacon). Apply these project conventions for edits, refactors, and new features."
applyTo: "**"
---

# ESP32-S3 UWB OLED Project Conventions

- Use ESP-IDF APIs and patterns already present in `main/main.c` unless the task explicitly asks for a redesign.
- Keep UART/UWB framing logic deterministic and stream-safe: preserve partial-frame handling, header/footer validation, length checks, and CRC verification.
- When editing frame parsing, avoid magic-number drift: keep frame offsets centralized with clear comments or constants.
- Keep LVGL updates thread-safe: call UI updates inside `lvgl_port_lock(...)` and `lvgl_port_unlock()`.
- Keep FreeRTOS tasks non-blocking enough for responsiveness: prefer short delays and bounded read timeouts.
- Preserve low-memory embedded practices: avoid large dynamic allocations in loops, check bounds before memcpy/memmove, and guard against buffer overflow.
- Keep hardware pin and protocol constants at the top-level define section for discoverability.
- Remove unused includes and stale code paths when touching related areas, unless they are intentionally reserved for near-term features.
- Keep logs actionable: warnings for malformed frames and errors for overflow/reset conditions.

## Current Architecture

- `main/main.c` is the runtime orchestrator and keeps UWB-specific logic:
	- Initializes shared I2C master bus (`i2c_new_master_bus`) and passes bus handle to display module.
	- Initializes BLE beacon module.
	- Configures UART0/UART2 and runs `pc_rx_task` + `uwb_rx_task`.
	- Contains frame parser (`DSTO_HEAD`/`DSTO_FOOT`, payload length check, XOR CRC check) and UWB payload interpretation.
- `main/src/display.c` contains display-only logic:
	- Accepts initialized `i2c_master_bus_handle_t` from caller.
	- Creates SSD1306 panel I/O and panel, initializes LVGL display, and owns display label state.
	- Exposes `display_update(...)` and handles LVGL locking internally.
- `main/src/ble_beacon.c` contains NimBLE beacon logic:
	- Initializes NVS and NimBLE host.
	- Sets GAP device name to `"atilla"`.
	- Starts non-connectable BLE advertising from host sync callback.
- `main/include/display.h` and `main/include/ble_beacon.h` are module interfaces.
- `main/include/common.h` is a shared include header for common ESP headers and shared constants.

## Project Entry Points

- Firmware target and project bootstrap: `CMakeLists.txt`
- Main component registration and source list: `main/CMakeLists.txt`
- Main runtime logic (UWB parser, UART bridge, task startup): `main/main.c`
- Display module implementation: `main/src/display.c`
- BLE beacon module implementation: `main/src/ble_beacon.c`
- Public module headers: `main/include/`
- Managed component versions (LVGL and LCD port): `main/idf_component.yml`

## Build/Run Notes

- Use ESP-IDF extension commands for build/flash/monitor workflows in this workspace.
- If build settings are touched, keep compatibility with ESP32-S3 defaults already in `sdkconfig` and `sdkconfig.defaults.esp32s3`.
- Keep `main/CMakeLists.txt` dependencies aligned with used headers/APIs. If source includes `driver/uart.h` or `driver/i2c_master.h`, declare needed `PRIV_REQUIRES` driver components.
