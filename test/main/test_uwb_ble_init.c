#include <string.h>
#include "unity.h"
#include "uwb_handler.h"
#include "uwb_parser.h"

/* ------------------------------------------------------------------ */
/*  Mock state                                                          */
/* ------------------------------------------------------------------ */

static int      mock_ble_init_calls;
static uint32_t mock_ble_init_dev_id;
static int      mock_ble_update_calls;
static char     mock_ble_update_data[UWB_DISPLAY_STR_LEN];
static int      mock_display_calls;

/* ------------------------------------------------------------------ */
/*  Mock implementations (replace real symbols at link time)           */
/* ------------------------------------------------------------------ */

void ble_beacon_init(uint32_t dev_id)
{
    mock_ble_init_calls++;
    mock_ble_init_dev_id = dev_id;
}

void ble_beacon_update_uwb_data(const char *data)
{
    mock_ble_update_calls++;
    strncpy(mock_ble_update_data, data, UWB_DISPLAY_STR_LEN - 1);
    mock_ble_update_data[UWB_DISPLAY_STR_LEN - 1] = '\0';
}

void display_update(const char *text)
{
    (void)text;
    mock_display_calls++;
}

/* ------------------------------------------------------------------ */
/*  Frame builder                                                       */
/*                                                                      */
/*  Frame layout (absolute byte offsets from buf[0]):                  */
/*    [0]        HEAD (0x2A)                                            */
/*    [1..2]     payload_len (little-endian)                            */
/*    [3..3+N-1] payload (N = payload_len bytes)                        */
/*    [3+N]      XOR CRC of payload bytes                               */
/*    [3+N+1]    FOOT (0x23)                                            */
/*                                                                      */
/*  Key absolute offsets used by uwb_parse_frame:                      */
/*    [19] cmd_type   [25] dsto_id   [36] online_dev_num                */
/*    [37 + i*10 .. +7] dev_para (8 bytes)   [+8..+9] distance (2 B)   */
/* ------------------------------------------------------------------ */

/* Minimum payload length to reach data[36] (online_dev_num) */
#define MIN_PAYLOAD_LEN 34u

static uint16_t build_range_frame(uint8_t *buf, uint8_t dsto_id,
                                   uint8_t dev_count,
                                   const uwb_device_entry_t *devices)
{
    uint16_t payload_len = (uint16_t)(MIN_PAYLOAD_LEN + (uint16_t)dev_count * 10u);
    memset(buf, 0, 5u + payload_len);

    buf[0] = UWB_FRAME_HEAD;
    buf[1] = (uint8_t)(payload_len & 0xFFu);
    buf[2] = (uint8_t)((payload_len >> 8) & 0xFFu);

    buf[19] = UWB_CMD_TYPE_RANGE;
    buf[25] = dsto_id;
    buf[36] = dev_count;

    for (int i = 0; i < dev_count; i++) {
        int base = 37 + i * 10;
        /* dev_id occupies bits [2..33] of the 8-byte dev_para word */
        uint64_t dev_para_val = ((uint64_t)devices[i].dev_id) << 2;
        memcpy(&buf[base], &dev_para_val, 8);
        memcpy(&buf[base + 8], &devices[i].distance_cm, 2);
    }

    buf[3 + payload_len]     = uwb_calc_xor_crc(buf, 3, payload_len);
    buf[3 + payload_len + 1] = UWB_FRAME_FOOT;

    return payload_len;
}

/* ------------------------------------------------------------------ */
/*  Per-test setup / teardown                                           */
/* ------------------------------------------------------------------ */

void setUp(void)
{
    mock_ble_init_calls   = 0;
    mock_ble_init_dev_id  = 0;
    mock_ble_update_calls = 0;
    mock_display_calls    = 0;
    memset(mock_ble_update_data, 0, sizeof(mock_ble_update_data));
    uwb_handler_reset();
}

void tearDown(void) {}

/* ------------------------------------------------------------------ */
/*  Tests                                                               */
/* ------------------------------------------------------------------ */

void test_ble_not_initialized_when_dsto_id_is_zero(void)
{
    uint8_t buf[256];
    uint16_t plen = build_range_frame(buf, 0, 0, NULL);
    uwb_parse_result_t r = uwb_parse_frame(buf, plen);

    uwb_handler_process_result(&r);

    TEST_ASSERT_EQUAL_INT(0, mock_ble_init_calls);
    TEST_ASSERT_FALSE(uwb_handler_is_ble_initialized());
}

void test_ble_initialized_on_first_nonzero_dsto_id(void)
{
    uint8_t buf[256];
    uint16_t plen = build_range_frame(buf, 42, 0, NULL);
    uwb_parse_result_t r = uwb_parse_frame(buf, plen);

    uwb_handler_process_result(&r);

    TEST_ASSERT_EQUAL_INT(1, mock_ble_init_calls);
    TEST_ASSERT_EQUAL_UINT32(42, mock_ble_init_dev_id);
    TEST_ASSERT_TRUE(uwb_handler_is_ble_initialized());
}

void test_ble_not_reinitialized_on_subsequent_packets(void)
{
    uint8_t buf[256];
    uint16_t plen = build_range_frame(buf, 42, 0, NULL);

    for (int i = 0; i < 5; i++) {
        uwb_parse_result_t r = uwb_parse_frame(buf, plen);
        uwb_handler_process_result(&r);
    }

    TEST_ASSERT_EQUAL_INT(1, mock_ble_init_calls);
}

void test_ble_update_not_called_before_initialization(void)
{
    uint8_t buf[256];
    uint16_t plen = build_range_frame(buf, 0, 0, NULL);
    uwb_parse_result_t r = uwb_parse_frame(buf, plen);

    uwb_handler_process_result(&r);

    TEST_ASSERT_EQUAL_INT(0, mock_ble_update_calls);
}

void test_ble_update_called_with_correct_data_after_init(void)
{
    uwb_device_entry_t dev = { .dev_id = 7, .distance_cm = 150 };
    uint8_t buf[256];
    uint16_t plen = build_range_frame(buf, 42, 1, &dev);
    uwb_parse_result_t r = uwb_parse_frame(buf, plen);

    uwb_handler_process_result(&r);

    TEST_ASSERT_EQUAL_INT(1, mock_ble_update_calls);
    TEST_ASSERT_NOT_NULL(strstr(mock_ble_update_data, "ID7"));
    TEST_ASSERT_NOT_NULL(strstr(mock_ble_update_data, "150"));
}

void test_display_update_called_even_when_no_devices(void)
{
    uint8_t buf[256];
    uint16_t plen = build_range_frame(buf, 42, 0, NULL);
    uwb_parse_result_t r = uwb_parse_frame(buf, plen);

    uwb_handler_process_result(&r);

    TEST_ASSERT_EQUAL_INT(1, mock_display_calls);
    TEST_ASSERT_EQUAL_INT(1, mock_ble_update_calls);
    TEST_ASSERT_EQUAL_STRING("No Devices", mock_ble_update_data);
}

void test_crc_error_frame_is_detected(void)
{
    uint8_t buf[256];
    uint16_t plen = build_range_frame(buf, 42, 0, NULL);
    buf[3 + plen] ^= 0xFF; /* corrupt the CRC byte */

    uwb_parse_result_t r = uwb_parse_frame(buf, plen);

    TEST_ASSERT_EQUAL_INT(UWB_PARSE_ERR_CRC, r.status);
    /* main.c discards CRC-error frames before calling uwb_handler — verify
       that no mock was touched (parser level check only). */
    TEST_ASSERT_EQUAL_INT(0, mock_ble_init_calls);
    TEST_ASSERT_EQUAL_INT(0, mock_ble_update_calls);
}

/* ------------------------------------------------------------------ */
/*  Entry point                                                         */
/* ------------------------------------------------------------------ */

void app_main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ble_not_initialized_when_dsto_id_is_zero);
    RUN_TEST(test_ble_initialized_on_first_nonzero_dsto_id);
    RUN_TEST(test_ble_not_reinitialized_on_subsequent_packets);
    RUN_TEST(test_ble_update_not_called_before_initialization);
    RUN_TEST(test_ble_update_called_with_correct_data_after_init);
    RUN_TEST(test_display_update_called_even_when_no_devices);
    RUN_TEST(test_crc_error_frame_is_detected);
    UNITY_END();
}
