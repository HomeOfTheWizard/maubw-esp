#pragma once

/**
 * Initialize NVS flash, the NimBLE host stack, and start BLE beacon advertising.
 * The beacon advertises the device name "atilla" in non-connectable general
 * discoverable mode.
 * Must be called once from app_main.
 */
void ble_beacon_init(void);
