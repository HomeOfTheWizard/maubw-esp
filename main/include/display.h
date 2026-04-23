#pragma once

#include "common.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

/**
 * Initialize SSD1306 LCD panel and LVGL port using an already-initialized I2C bus.
 * Must be called once from `app_main` after creating the I2C master bus.
 * @param i2c_bus  Handle to an initialized I2C master bus
 */
void display_init(i2c_master_bus_handle_t i2c_bus);

/**
 * Update the OLED label text.
 * Thread-safe: acquires lvgl_port_lock internally.
 * @param text  Null-terminated string to display.
 */
void display_update(const char *text);
