#include "display.h"

static const char *TAG = "display";

// --- LCD protocol constants ---
#define LCD_PIXEL_CLOCK_HZ            (400 * 1000)
#define I2C_HW_ADDR                   0x3C

// --- Display resolution ---
#define LCD_H_RES                     128
#define LCD_V_RES                     64

// --- LCD command/param bit widths ---
#define LCD_CMD_BITS                  8
#define LCD_PARAM_BITS                8

// --- LVGL UI objects ---
static lv_obj_t *label_list;

void display_init(i2c_master_bus_handle_t i2c_bus)
{
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = I2C_HW_ADDR,
        .scl_speed_hz = LCD_PIXEL_CLOCK_HZ,
        .control_phase_bytes = 1,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .dc_bit_offset = 6,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io_handle));

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .bits_per_pixel = 1,
        .reset_gpio_num = PIN_NUM_RST,
    };
    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = LCD_V_RES,
    };
    panel_config.vendor_config = &ssd1306_config;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "Initialize LVGL");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.task_stack = 6144;
#if CONFIG_SOC_CPU_CORES_NUM > 1
    port_cfg.task_affinity = 1;
#endif
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding OLED display");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = LCD_H_RES * LCD_V_RES,
        .double_buffer = false,
        .trans_size = 0,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = true,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };
    lvgl_port_add_disp(&disp_cfg);

    if (lvgl_port_lock(0)) {
        label_list = lv_label_create(lv_screen_active());
        lv_label_set_long_mode(label_list, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(label_list, LCD_H_RES);
        lv_obj_align(label_list, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_set_style_text_font(label_list, &lv_font_montserrat_14, 0);
        lv_label_set_text(label_list, "Waiting for UWB...");
        lvgl_port_unlock();
    }
}

void display_update(const char *text)
{
    if (lvgl_port_lock(0)) {
        lv_label_set_text(label_list, text);
        lvgl_port_unlock();
    }
}
