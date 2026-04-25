#include "uwb_handler.h"
#include "ble_beacon.h"
#include "common.h"
#include <stdbool.h>

/* Forward declaration — avoids pulling in all LCD/LVGL headers in tests */
void display_update(const char *text);

static const char *TAG = "uwb_handler";
static bool ble_initialized = false;

void uwb_handler_reset(void)
{
    ble_initialized = false;
}

bool uwb_handler_is_ble_initialized(void)
{
    return ble_initialized;
}

void uwb_handler_process_result(const uwb_parse_result_t *result)
{
    if (result->dsto_id != 0 && !ble_initialized) {
        ble_beacon_init(result->dsto_id);
        ble_initialized = true;
    }

    if (result->status == UWB_PARSE_OK_RANGE) {
        display_update(result->display_str);
        if (ble_initialized) {
            ble_beacon_update_uwb_data(result->display_str);
        }
    } else if (result->status == UWB_PARSE_OK_CFG) {
        ESP_LOGI(TAG, "Received configuration packet");
    } else {
        ESP_LOGD(TAG, "Other command type: 0x%02X", result->cmd_type);
    }
}
