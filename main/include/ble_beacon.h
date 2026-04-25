#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * Initialize NVS flash, the NimBLE host stack, GATT server, and start
 * connectable BLE advertising as "atilla_<dev_id>".
 * Must be called once, after the first valid UWB dev_id is known.
 * @param dev_id  Device ID suffix appended to the base name.
 */
void ble_beacon_init(uint32_t dev_id);

/**
 * Update the UWB distance string exposed via the GATT characteristic.
 * Subscribed BLE clients will receive a NOTIFY indication automatically.
 * Thread-safe: can be called from any FreeRTOS task.
 * @param data  Null-terminated UTF-8 string (same text shown on OLED).
 */
void ble_beacon_update_uwb_data(const char *data);

/**
 * Update the FSR press state exposed via the GATT characteristic.
 * Subscribed BLE clients will receive a NOTIFY indication automatically.
 * No-op if BLE has not finished initialising (handle not yet assigned).
 * Thread-safe: can be called from any FreeRTOS task.
 * @param pressed  true when the FSR sensor is pressed, false when released.
 */
void ble_beacon_update_fsr_data(bool pressed);
