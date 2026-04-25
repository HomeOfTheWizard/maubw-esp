#pragma once

#include "uwb_parser.h"
#include <stdbool.h>

/**
 * Process a parsed UWB frame result:
 *   - Initialises BLE beacon on the first frame that carries a non-zero dsto_id.
 *   - Calls display_update() and (once BLE is up) ble_beacon_update_uwb_data()
 *     for every successful RANGE frame.
 */
void uwb_handler_process_result(const uwb_parse_result_t *result);

/**
 * Reset internal state (BLE initialisation flag).
 * Intended for use in unit tests only.
 */
void uwb_handler_reset(void);

/**
 * Return whether the BLE beacon has been initialised.
 * Intended for use in unit tests only.
 */
bool uwb_handler_is_ble_initialized(void);
