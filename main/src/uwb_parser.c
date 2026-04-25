#include "uwb_parser.h"

#include <string.h>
#include <stdio.h>

uint8_t uwb_calc_xor_crc(const uint8_t *data, int offset, int len)
{
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        crc ^= data[offset + i];
    }
    return crc;
}

uwb_parse_result_t uwb_parse_frame(const uint8_t *data, uint16_t payload_len)
{
    uwb_parse_result_t result = {0};

    /* ---- CRC check ---- */
    uint8_t pkt_crc  = data[3 + payload_len];
    uint8_t calc_crc = uwb_calc_xor_crc(data, 3, payload_len);

    if (pkt_crc != calc_crc) {
        result.status = UWB_PARSE_ERR_CRC;
        return result;
    }

    result.dsto_id  = data[25];
    result.cmd_type = data[19];

    if (result.cmd_type == UWB_CMD_TYPE_RANGE) {
        uint8_t online_dev_num = data[36];
        int     str_len        = 0;
        int     display_count  = 0;

        for (int i = 0; i < online_dev_num; i++) {
            int base_idx = 37 + i * 10;

            if (base_idx + 10 > 3 + payload_len) break;

            uwb_dev_para_t dev_para;
            memcpy(&dev_para.value, &data[base_idx], 8);

            if (dev_para.bit.dev_id == 0) continue;

            uint16_t dis;
            memcpy(&dis, &data[base_idx + 8], 2);

            /* Store structured entry */
            if (result.device_count < UWB_MAX_DEVICES) {
                result.devices[result.device_count].dev_id      = (uint32_t)dev_para.bit.dev_id;
                result.devices[result.device_count].distance_cm = dis;
                result.device_count++;
            }

            /* Build display string (up to 6 entries) */
            if (display_count < 6) {
                int added = snprintf(result.display_str + str_len,
                                     UWB_DISPLAY_STR_LEN - str_len,
                                     "ID%d:%d cm\n",
                                     (int)dev_para.bit.dev_id, dis);
                if (added > 0) str_len += added;
                display_count++;
            }
        }

        if (result.device_count == 0) {
            snprintf(result.display_str, UWB_DISPLAY_STR_LEN, "No Devices");
        }

        result.status = UWB_PARSE_OK_RANGE;

    } else if (result.cmd_type == UWB_CMD_TYPE_CFG) {
        result.status = UWB_PARSE_OK_CFG;
    } else {
        result.status = UWB_PARSE_OK_OTHER;
    }

    return result;
}
