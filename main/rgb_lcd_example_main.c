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

#include "lvgl.h"
#include "lcd_config.h"
#include "esp_io_expander.h"
#include "esp_lcd_panel_io_additions.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_gt911.h"

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

static void example_lvgl_touch_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    uint16_t touchpad_x[1] = { 0 };
    uint16_t touchpad_y[1] = { 0 };
    uint8_t touchpad_cnt = 0;
    esp_lcd_touch_read_data(touch_handle);

    bool touch_pressed = esp_lcd_touch_get_coordinates(touch_handle, touchpad_x, touchpad_y, NULL, &touchpad_cnt, 1);

    if (touch_pressed && touchpad_cnt > 0) {
        data->point.x = touchpad_x[0];
        data->point.y = touchpad_y[0];
        data->state = LV_INDEV_STATE_PRESSED;
        // ESP_LOGI(TAG,"%d,%d",touchpad_x[0],touchpad_y[0]);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// LVGL library is not thread-safe, this example will call LVGL APIs from different tasks, so use a mutex to protect it
static _lock_t lvgl_api_lock;

extern void example_lvgl_demo_ui(lv_display_t *disp);

static bool example_notify_lvgl_flush_ready(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *event_data, void *user_ctx) {
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

static void example_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // pass the draw buffer to the driver
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);
}

static void example_increase_lvgl_tick(void *arg) {
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(2);
}

static void example_lvgl_port_task(void *arg) {
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t time_till_next_ms = 0;
    while (1) {
        _lock_acquire(&lvgl_api_lock);
        time_till_next_ms = lv_timer_handler();
        _lock_release(&lvgl_api_lock);

        // in case of task watch dog timeout, set the minimal delay to 10ms
        if (time_till_next_ms < 10) {
            time_till_next_ms = 10;
        }

        usleep(1000 * time_till_next_ms);
    }
}

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
static esp_err_t lvgl_init(void) {
ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    // create a lvgl display
    display = lv_display_create(EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
    // associate the rgb panel handle to the display
    lv_display_set_user_data(display, panel_handle);
    // set color depth
    lv_display_set_color_format(display, EXAMPLE_LV_COLOR_FORMAT);
    // create draw buffers
    void *buf1 = NULL;
    void *buf2 = NULL;

    ESP_LOGI(TAG, "Allocate LVGL draw buffers");
    // it's recommended to allocate the draw buffer from internal memory, for better performance
    size_t draw_buffer_sz = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * EXAMPLE_PIXEL_SIZE * 2;
    buf1 = heap_caps_malloc(draw_buffer_sz, MALLOC_CAP_SPIRAM);
    assert(buf1);
    buf2 = heap_caps_malloc(draw_buffer_sz, MALLOC_CAP_SPIRAM);
    assert(buf2);
    // set LVGL draw buffers and partial mode
    lv_display_set_buffers(display, buf1, buf2, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);

    // set the callback which can copy the rendered image to an area of the display
    lv_display_set_flush_cb(display, example_lvgl_flush_cb);

    ESP_LOGI(TAG, "Register event callbacks");
    esp_lcd_rgb_panel_event_callbacks_t cbs = {
        .on_color_trans_done = example_notify_lvgl_flush_ready,
    };
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel_handle, &cbs, display));

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, 2 * 1000));

#if CONFIG_EXAMPLE_LCD_USE_TOUCH_ENABLED
    static lv_indev_t *touch_indev;
    touch_indev = lv_indev_create();
    lv_indev_set_type(touch_indev,LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(touch_indev,display);
    lv_indev_set_read_cb(touch_indev,example_lvgl_touch_cb);
#endif

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

    xTaskCreate(example_lvgl_port_task, "LVGL", 12 * 1024, NULL, 5, NULL);

    ESP_LOGI(TAG, "Display LVGL UI");
    // Lock the mutex due to the LVGL APIs are not thread-safe
    _lock_acquire(&lvgl_api_lock);
    example_lvgl_demo_ui(display);
    _lock_release(&lvgl_api_lock);

    //Turn On Lcd Backlight
    lcd_set_backlight(EXAMPLE_LCD_BACKLIGHT_ON);

}
