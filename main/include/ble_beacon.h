#pragma once

/**
 * Initialize NVS flash, the NimBLE host stack, GATT server, and start
 * connectable BLE advertising as "atilla".
 * Must be called once from app_main.
 */
void ble_beacon_init(void);

/**
 * Update the UWB distance string exposed via the GATT characteristic.
 * Subscribed BLE clients will receive a NOTIFY indication automatically.
 * Thread-safe: can be called from any FreeRTOS task.
 * @param data  Null-terminated UTF-8 string (same text shown on OLED).
 */
void ble_beacon_update_uwb_data(const char *data);
