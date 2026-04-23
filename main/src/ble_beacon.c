#include "ble_beacon.h"

#include "common.h"
#include "nvs_flash.h"

/* NimBLE host and port */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"

/* GAP and GATT services */
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

/* BLE store config (for host key storage) */
#include "store/config/ble_store_config.h"

/* Library function declarations */
void ble_store_config_init(void);

static const char *TAG = "ble_beacon";

#define BEACON_DEVICE_NAME  "atilla"

/* ------------------------------------------------------------------ */
/*  UWB data GATT characteristic                                        */
/*                                                                      */
/*  Service UUID:        12345678-1234-5678-1234-56789abcdef0           */
/*  Characteristic UUID: 12345678-1234-5678-1234-56789abcdef1           */
/*    Properties: READ | NOTIFY                                         */
/*    Value: UTF-8 string — same text shown on the OLED display         */
/* ------------------------------------------------------------------ */

#define UWB_DATA_BUF_LEN 256

static char     uwb_data_buf[UWB_DATA_BUF_LEN] = "Waiting for UWB...";
static uint16_t uwb_data_handle;

static const ble_uuid128_t svc_uuid =
    BLE_UUID128_INIT(0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
                     0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

static const ble_uuid128_t chr_uuid =
    BLE_UUID128_INIT(0xf1, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
                     0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

static int uwb_chr_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        int rc = os_mbuf_append(ctxt->om, uwb_data_buf, strlen(uwb_data_buf));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type            = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid            = &svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid       = &chr_uuid.u,
                .access_cb  = uwb_chr_access_cb,
                .val_handle = &uwb_data_handle,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            { 0 } /* sentinel */
        },
    },
    { 0 } /* sentinel */
};

/* Determined at sync time; used when starting advertising */
static uint8_t own_addr_type;

/* ------------------------------------------------------------------ */
/*  GAP event handler (forward declaration)                             */
/* ------------------------------------------------------------------ */

static int gap_event_cb(struct ble_gap_event *event, void *arg);

/* ------------------------------------------------------------------ */
/*  Advertising                                                         */
/* ------------------------------------------------------------------ */

static void start_advertising(void)
{
    int rc;
    const char *name;
    struct ble_hs_adv_fields adv_fields  = {0};
    struct ble_hs_adv_fields rsp_fields  = {0};
    struct ble_gap_adv_params adv_params = {0};

    /* General discoverable mode, BLE-only (no BR/EDR) */
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    /* Put device name in scan response to keep adv PDU compact */
    name = ble_svc_gap_device_name();
    rsp_fields.name             = (uint8_t *)name;
    rsp_fields.name_len         = strlen(name);
    rsp_fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set adv fields, rc=%d", rc);
        return;
    }

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set scan response fields, rc=%d", rc);
        return;
    }

    /* Connectable undirected so GATT clients can connect and subscribe */
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, gap_event_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start advertising, rc=%d", rc);
        return;
    }

    ESP_LOGI(TAG, "BLE advertising as \"%s\" (connectable)", BEACON_DEVICE_NAME);
}

/* ------------------------------------------------------------------ */
/*  GAP event handler                                                   */
/* ------------------------------------------------------------------ */

static int gap_event_cb(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "BLE client connected, handle=%d",
                     event->connect.conn_handle);
        } else {
            ESP_LOGW(TAG, "BLE connection failed, status=%d",
                     event->connect.status);
            start_advertising();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "BLE client disconnected, reason=%d",
                 event->disconnect.reason);
        start_advertising();
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        start_advertising();
        break;

    default:
        break;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  NimBLE host callbacks                                               */
/* ------------------------------------------------------------------ */

static void on_stack_sync(void)
{
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to infer addr type, rc=%d", rc);
        return;
    }
    start_advertising();
}

static void on_stack_reset(int reason)
{
    /* Host resets the stack on unrecoverable errors */
    ESP_LOGE(TAG, "BLE host stack reset, reason=%d", reason);
}

/* ------------------------------------------------------------------ */
/*  NimBLE host FreeRTOS task                                           */
/* ------------------------------------------------------------------ */

static void nimble_host_task(void *param)
{
    ESP_LOGI(TAG, "NimBLE host task started");
    /* Runs until nimble_port_stop() is called */
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

void ble_beacon_update_uwb_data(const char *data)
{
    strncpy(uwb_data_buf, data, UWB_DATA_BUF_LEN - 1);
    uwb_data_buf[UWB_DATA_BUF_LEN - 1] = '\0';

    /* Notify all subscribed BLE clients */
    if (uwb_data_handle != 0) {
        ble_gatts_chr_updated(uwb_data_handle);
    }
}

void ble_beacon_init(void)
{
    /* NVS flash is required by the BLE host for storing addresses/keys */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Initialize the NimBLE host port */
    ESP_ERROR_CHECK(nimble_port_init());

    /* Register GAP and GATT base services */
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ESP_ERROR_CHECK(ble_svc_gap_device_name_set(BEACON_DEVICE_NAME));

    /* Register custom GATT service table */
    ESP_ERROR_CHECK(ble_gatts_count_cfg(gatt_svcs));
    ESP_ERROR_CHECK(ble_gatts_add_svcs(gatt_svcs));

    /* Configure host callbacks */
    ble_hs_cfg.reset_cb        = on_stack_reset;
    ble_hs_cfg.sync_cb         = on_stack_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Initialize BLE host storage (for bonding / key material) */
    ble_store_config_init();

    /* Start the NimBLE host task — advertising begins inside on_stack_sync */
    nimble_port_freertos_init(nimble_host_task);
}
