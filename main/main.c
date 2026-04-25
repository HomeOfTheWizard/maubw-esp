#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "common.h"
#include "display.h"
#include "ble_beacon.h"
#include "uwb_parser.h"
#include "uwb_handler.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"



// UART bus pins (used to initialize the communication with the UWB module and the PC)
#define UWB_UART_NUM    UART_NUM_2
#define PC_UART_NUM     UART_NUM_0

#define UWB_RX_PIN      15
#define UWB_TX_PIN      16

// I2C bus pins (used to initialize the I2C master bus)
#define I2C_BUS_PORT    0
#define PIN_NUM_SDA     39
#define PIN_NUM_SCL     38

// Max Frame is 239 bytes. 1024 is enough for ~4 frames.
#define BUF_SIZE        1024 

// DSTP frame structure constants — protocol values live in uwb_parser.h
#define DSTO_HEAD      UWB_FRAME_HEAD
#define DSTO_FOOT      UWB_FRAME_FOOT
#define MAX_PAYLOAD_LEN UWB_MAX_PAYLOAD_LEN


static const char *TAG = "uwb_main";

static void uwb_process_packet(uint8_t *data, uint16_t payload_len) {
    uwb_parse_result_t result = uwb_parse_frame(data, payload_len);

    if (result.status == UWB_PARSE_ERR_CRC) {
        ESP_LOGW(TAG, "CRC error in UWB frame");
        return;
    }

    ESP_LOGI(TAG, "Range Frame: DSTO ID: %d", result.dsto_id);
    uwb_handler_process_result(&result);
}

static void pc_rx_task(void *arg) {
    uint8_t *data = (uint8_t *) malloc(1024);
    while (1) {
        int len = uart_read_bytes(PC_UART_NUM, data, 1024 - 1, pdMS_TO_TICKS(100));
        if (len > 0) {
            ESP_LOG_BUFFER_HEX(TAG, data, len);
            uart_write_bytes(UWB_UART_NUM, data, len);
        }
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
    free(data);
}

static void uwb_rx_task(void *arg) {
    static uint8_t rx_buffer[BUF_SIZE];
    static int current_len = 0;

    while (1) {
        int free_space = BUF_SIZE - current_len;
        if (free_space <= 0) {
            ESP_LOGE(TAG, "Buffer overflow, clearing");
            current_len = 0;
            free_space = BUF_SIZE;
        }

        int len = uart_read_bytes(UWB_UART_NUM, rx_buffer + current_len, free_space, pdMS_TO_TICKS(20));

        if (len > 0) {
            uart_write_bytes(PC_UART_NUM, rx_buffer + current_len, len);
            
            current_len += len;

            int offset = 0;
            while (offset < current_len) {
                int remain = current_len - offset;

                if (rx_buffer[offset] != DSTO_HEAD) {
                    offset++;
                    continue; 
                }

                if (remain < 3) {
                    break; 
                }

                uint16_t payload_len = rx_buffer[offset + 1] | (rx_buffer[offset + 2] << 8);
                
                if (payload_len > MAX_PAYLOAD_LEN) {
                    ESP_LOGW(TAG, "Invalid payload len %d, skipping fake header", payload_len);
                    offset++;
                    continue;
                }

                int frame_total_len = 1 + 2 + payload_len + 1 + 1;

                if (remain < frame_total_len) {
                    break;
                }

                if (rx_buffer[offset + frame_total_len - 1] != DSTO_FOOT) {
                    ESP_LOGW(TAG, "Header found but Footer mismatch, skipping byte");
                    offset++;
                    continue;
                }

                uwb_process_packet(&rx_buffer[offset], payload_len);

                offset += frame_total_len;
            }

            if (offset > 0) {
                int remaining_data = current_len - offset;
                if (remaining_data > 0) {
                    memmove(rx_buffer, rx_buffer + offset, remaining_data);
                }
                current_len = remaining_data;
            }
        }
        
        if (len == 0) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

void app_main(void)
{
    i2c_master_bus_handle_t i2c_bus = NULL;
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .i2c_port = I2C_BUS_PORT,
        .sda_io_num = PIN_NUM_SDA,
        .scl_io_num = PIN_NUM_SCL,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));

    display_init(i2c_bus);

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(PC_UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(PC_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(PC_UART_NUM, 43, 44, -1, -1));

    uart_config_t uwb_uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(UWB_UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UWB_UART_NUM, &uwb_uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UWB_UART_NUM, UWB_TX_PIN, UWB_RX_PIN, -1, -1));

    xTaskCreate(pc_rx_task, "pc_rx_task", 4096, NULL, 10, NULL);
    xTaskCreate(uwb_rx_task, "uwb_rx_task", 4096, NULL, 12, NULL); 
}