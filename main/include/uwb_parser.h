#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/*  DSTP frame protocol constants                                       */
/* ------------------------------------------------------------------ */

#define UWB_FRAME_HEAD      0x2A
#define UWB_FRAME_FOOT      0x23
#define UWB_MAX_PAYLOAD_LEN 240

#define UWB_CMD_TYPE_RANGE  0x71
#define UWB_CMD_TYPE_CFG    0x02

/* ------------------------------------------------------------------ */
/*  Internal bit-field layout of the 8-byte device parameter word      */
/* ------------------------------------------------------------------ */

typedef union {
    struct __attribute__((packed)) {
        uint64_t  dev_mode  :2;
        uint64_t  dev_id    :32;
        uint64_t  dev_type  :3;
        uint64_t  done      :1;
        uint64_t  inNet     :1;
        uint64_t  reserved  :25;
    } bit;
    uint64_t value;
} uwb_dev_para_t;

/* ------------------------------------------------------------------ */
/*  Parsed output types                                                 */
/* ------------------------------------------------------------------ */

#define UWB_MAX_DEVICES     16
#define UWB_DISPLAY_STR_LEN 256

typedef struct {
    uint32_t dev_id;
    uint16_t distance_cm;
} uwb_device_entry_t;

typedef enum {
    UWB_PARSE_OK_RANGE,   /* range frame, devices[] / display_str populated */
    UWB_PARSE_OK_CFG,     /* configuration frame                             */
    UWB_PARSE_OK_OTHER,   /* unknown command type                            */
    UWB_PARSE_ERR_CRC,    /* CRC mismatch — result should be discarded       */
} uwb_parse_status_t;

typedef struct {
    uwb_parse_status_t  status;
    uint8_t             cmd_type;
    uint32_t            dsto_id;                       /* raw ID from data[25] */
    uint8_t             device_count;                  /* entries in devices[] */
    uwb_device_entry_t  devices[UWB_MAX_DEVICES];
    char                display_str[UWB_DISPLAY_STR_LEN]; /* ready-to-display string */
} uwb_parse_result_t;

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

/**
 * Compute XOR checksum over data[offset .. offset+len-1].
 */
uint8_t uwb_calc_xor_crc(const uint8_t *data, int offset, int len);

/**
 * Parse a single DSTP payload that starts at data[0].
 * Verifies the CRC and fills a uwb_parse_result_t.
 *
 * @param data        Pointer to the first byte of the frame (the header byte 0x2A).
 * @param payload_len Length field extracted from bytes [1..2].
 * @return            Populated result struct; check result.status first.
 */
uwb_parse_result_t uwb_parse_frame(const uint8_t *data, uint16_t payload_len);
