---
description: "Use when working on this ESP-IDF firmware project (ESP32-S3, UART/UWB frame parsing, LVGL OLED UI, FreeRTOS tasks). Apply these project conventions for edits, refactors, and new features."
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

## Project Entry Points

- Firmware target and project bootstrap: `CMakeLists.txt`
- Main component registration: `main/CMakeLists.txt`
- Main runtime logic (UART bridge, UWB parser, LVGL UI): `main/main.c`
- Managed component versions (LVGL and LCD port): `main/idf_component.yml`

## Build/Run Notes

- Use ESP-IDF extension commands for build/flash/monitor workflows in this workspace.
- If build settings are touched, keep compatibility with ESP32-S3 defaults already in `sdkconfig` and `sdkconfig.defaults.esp32s3`.
