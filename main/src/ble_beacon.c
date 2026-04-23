#include "ble_beacon.h"

#include "common.h"
#include "nvs_flash.h"

/* NimBLE host and port */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"

/* GAP service */
#include "services/gap/ble_svc_gap.h"

/* BLE store config (for host key storage) */
#include "store/config/ble_store_config.h"

/* Library function declarations */
void ble_store_config_init(void);

static const char *TAG = "ble_beacon";

#define BEACON_DEVICE_NAME  "atilla"

/* Determined at sync time; used when starting advertising */
static uint8_t own_addr_type;

/* ------------------------------------------------------------------ */
/*  Advertising                                                         */
/* ------------------------------------------------------------------ */

static void start_advertising(void)
{
    int rc;
    const char *name;
    struct ble_hs_adv_fields adv_fields  = {0};
    struct ble_gap_adv_params adv_params = {0};

    /* General discoverable mode, BLE-only (no BR/EDR) */
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    /* Complete local name */
    name = ble_svc_gap_device_name();
    adv_fields.name             = (uint8_t *)name;
    adv_fields.name_len         = strlen(name);
    adv_fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set adv fields, rc=%d", rc);
        return;
    }

    /* Non-connectable undirected, general discoverable */
    adv_params.conn_mode = BLE_GAP_CONN_MODE_NON;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, NULL, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start advertising, rc=%d", rc);
        return;
    }

    ESP_LOGI(TAG, "BLE beacon advertising as \"%s\"", BEACON_DEVICE_NAME);
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

    /* Register GAP service and set device name */
    ble_svc_gap_init();
    ESP_ERROR_CHECK(ble_svc_gap_device_name_set(BEACON_DEVICE_NAME));

    /* Configure host callbacks */
    ble_hs_cfg.reset_cb        = on_stack_reset;
    ble_hs_cfg.sync_cb         = on_stack_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Initialize BLE host storage (for bonding / key material) */
    ble_store_config_init();

    /* Start the NimBLE host task — advertising begins inside on_stack_sync */
    nimble_port_freertos_init(nimble_host_task);
}
