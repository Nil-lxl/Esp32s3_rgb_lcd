/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/lock.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"

#include "lvgl.h"
#include "demos/lv_demos.h"
#include "lcd_config.h"
#include "esp_io_expander.h"
#include "esp_lcd_panel_io_additions.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lvgl_port.h"

static i2c_master_bus_handle_t i2c_bus_handle;
static esp_lcd_touch_handle_t touch_handle;
static esp_lcd_panel_io_handle_t touch_io_handle;

#if CONFIG_EXAMPLE_LCD_USE_TOUCH_ENABLED
static esp_err_t lcd_touch_init() {

    i2c_master_bus_config_t i2c_bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .sda_io_num = TOUCH_I2C_SDA,
        .scl_io_num = TOUCH_I2C_SCL,
        .i2c_port = I2C_NUM_0,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_handle));
    const esp_lcd_touch_config_t touch_config = {
        .x_max = EXAMPLE_LCD_H_RES,
        .y_max = EXAMPLE_LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
        },
    };
    esp_lcd_panel_io_i2c_config_t touch_io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    touch_io_cfg.scl_speed_hz = 400000;

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(i2c_bus_handle, &touch_io_cfg, &touch_io_handle));
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(touch_io_handle, &touch_config, &touch_handle));

    return ESP_OK;
}
#endif

// LVGL library is not thread-safe, this example will call LVGL APIs from different tasks, so use a mutex to protect it
static _lock_t lvgl_api_lock;

extern void lvgl_demo_ui();

static void lcd_backlight_init(void) {
#if EXAMPLE_PIN_BACKLIGHT >= 0
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << EXAMPLE_PIN_BACKLIGHT
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
#endif
}

static void lcd_set_backlight(uint32_t level) {
#if EXAMPLE_PIN_BACKLIGHT >= 0
    gpio_set_level(EXAMPLE_PIN_BACKLIGHT, level);
#endif
}

static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_err_t lcd_init(void) {
    lcd_backlight_init();
    lcd_set_backlight(EXAMPLE_LCD_BACKLIGHT_OFF);

    ESP_LOGI(TAG, "Initialize 3-Wire SPI Panel IO");
    spi_line_config_t line_config = {
        .cs_io_type = IO_TYPE_GPIO,
        .cs_gpio_num = PIN_NUM_CS,
        .scl_io_type = IO_TYPE_GPIO,
        .scl_gpio_num = PIN_NUM_SCL,
        .sda_io_type = IO_TYPE_GPIO,
        .sda_gpio_num = PIN_NUM_SDA,
        .io_expander = NULL,
    };
#ifdef CONFIG_EXAMPLE_LCD_CONTROLLER_NV3052C
    esp_lcd_panel_io_3wire_spi_config_t io_config = NV3052_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
#elif CONFIG_EXAMPLE_LCD_H030A10
    esp_lcd_panel_io_3wire_spi_config_t io_config = H030A10_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
#elif CONFIG_EXAMPLE_LCD_H040A18
    esp_lcd_panel_io_3wire_spi_config_t io_config = H040A18_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
#elif CONFIG_EXAMPLE_LCD_H035A17
    esp_lcd_panel_io_3wire_spi_config_t io_config = H035A17_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
#endif
    esp_lcd_panel_io_handle_t io_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_3wire_spi(&io_config, &io_handle));

    ESP_LOGI(TAG, "Install RGB LCD panel driver");
    esp_lcd_rgb_panel_config_t panel_config = {
        .data_width = EXAMPLE_DATA_BUS_WIDTH,
        .dma_burst_size = 64,
        .num_fbs = EXAMPLE_LCD_NUM_FB,
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .disp_gpio_num = EXAMPLE_PIN_NUM_DISP_EN,
        .pclk_gpio_num = EXAMPLE_PIN_NUM_PCLK,
        .vsync_gpio_num = EXAMPLE_PIN_NUM_VSYNC,
        .hsync_gpio_num = EXAMPLE_PIN_NUM_HSYNC,
        .de_gpio_num = EXAMPLE_PIN_NUM_DE,
        .data_gpio_nums = {
            EXAMPLE_PIN_NUM_DATA0,
            EXAMPLE_PIN_NUM_DATA1,
            EXAMPLE_PIN_NUM_DATA2,
            EXAMPLE_PIN_NUM_DATA3,
            EXAMPLE_PIN_NUM_DATA4,
            EXAMPLE_PIN_NUM_DATA5,
            EXAMPLE_PIN_NUM_DATA6,
            EXAMPLE_PIN_NUM_DATA7,
            EXAMPLE_PIN_NUM_DATA8,
            EXAMPLE_PIN_NUM_DATA9,
            EXAMPLE_PIN_NUM_DATA10,
            EXAMPLE_PIN_NUM_DATA11,
            EXAMPLE_PIN_NUM_DATA12,
            EXAMPLE_PIN_NUM_DATA13,
            EXAMPLE_PIN_NUM_DATA14,
            EXAMPLE_PIN_NUM_DATA15,
        },
        .timings = {
            .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
            .h_res = EXAMPLE_LCD_H_RES,
            .v_res = EXAMPLE_LCD_V_RES,
            .hsync_back_porch = EXAMPLE_LCD_HBP,
            .hsync_front_porch = EXAMPLE_LCD_HFP,
            .hsync_pulse_width = EXAMPLE_LCD_HSYNC,
            .vsync_back_porch = EXAMPLE_LCD_VBP,
            .vsync_front_porch = EXAMPLE_LCD_VFP,
            .vsync_pulse_width = EXAMPLE_LCD_VSYNC,
            .flags = {
                .pclk_active_neg = true,
            },
        },
        .flags.fb_in_psram = true, // allocate frame buffer in PSRAM
    };

#ifdef CONFIG_EXAMPLE_LCD_CONTROLLER_NV3052C
    nv3052_vendor_config_t vendor_config = {
#elif CONFIG_EXAMPLE_LCD_H030A10
    h030a10_vendor_config_t vendor_config = {
#elif CONFIG_EXAMPLE_LCD_H040A18
    h040a18_vendor_config_t vendor_config = {
#elif CONFIG_EXAMPLE_LCD_H035A17  
    h035a17_vendor_config_t vendor_config = {
#endif
        .rgb_config = &panel_config,
        .flags = {
            .mirror_by_cmd = 1,
            .enable_io_multiplex = 0,
        }
    };

    esp_lcd_panel_dev_config_t panel_dev_config = {
        .reset_gpio_num = PIN_NUM_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_config,
    };

#ifdef CONFIG_EXAMPLE_LCD_CONTROLLER_NV3052C
    ESP_ERROR_CHECK(esp_lcd_new_panel_nv3052_rgb(io_handle,&panel_dev_config,&panel_handle));
#elif CONFIG_EXAMPLE_LCD_H030A10
    ESP_ERROR_CHECK(esp_lcd_new_panel_h030a10(io_handle,&panel_dev_config,&panel_handle));
#elif CONFIG_EXAMPLE_LCD_H040A18
    ESP_ERROR_CHECK(esp_lcd_new_panel_h040a18(io_handle,&panel_dev_config,&panel_handle));
#elif CONFIG_EXAMPLE_LCD_H035A17
    ESP_ERROR_CHECK(esp_lcd_new_panel_h035a17(io_handle,&panel_dev_config,&panel_handle));
#endif

    ESP_LOGI(TAG, "Initialize RGB LCD panel");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    // ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    return ESP_OK;
}

static lv_display_t *display;
static lv_indev_t *lvgl_touch_indev = NULL;
static esp_err_t lvgl_init(void) {
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,
        .task_stack = 8 * 1024,
        .task_affinity = -1,
        .task_max_sleep_ms = 500,
        .timer_period_ms = 5,
    };

    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port initialization failed");
    uint32_t buf_size = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * 2;

    const lvgl_port_display_cfg_t disp_cfg = {
        .panel_handle = panel_handle,
        .buffer_size = buf_size,
        .double_buffer = 0,
        .hres = EXAMPLE_LCD_H_RES,
        .vres = EXAMPLE_LCD_V_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = false,
            .buff_spiram = false,
            .direct_mode=true,
            // .full_refresh=true,
            .swap_bytes = false,
        },
    };
    const lvgl_port_display_rgb_cfg_t rgb_cfg={
        .flags={
            .bb_mode=false,
            .avoid_tearing=true,
        }
    };
    display=lvgl_port_add_disp_rgb(&disp_cfg,&rgb_cfg);

    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = display,
        .handle = touch_handle,
    };
    lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);
    
    return ESP_OK;
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

#if CONFIG_EXAMPLE_LCD_USE_TOUCH_ENABLED
    ESP_ERROR_CHECK(lcd_touch_init());
#endif
    ESP_ERROR_CHECK(lcd_init());
    ESP_ERROR_CHECK(lvgl_init());


    lvgl_port_lock(0);
    lvgl_demo_ui();
    lvgl_port_unlock();

    //Turn On Lcd Backlight
    lcd_set_backlight(EXAMPLE_LCD_BACKLIGHT_ON);

}
