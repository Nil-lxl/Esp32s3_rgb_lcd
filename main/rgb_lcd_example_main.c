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
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "lvgl.h"
#include "lcd_defines.h"
#include "esp_io_expander.h"
#include "esp_lcd_panel_io_additions.h"

// #include "driver/i2c_master.h"

// i2c_master_bus_config_t i2c_config={
//     .clk_source=I2C_CLK_SRC_DEFAULT,
//     .i2c_port=I2C_NUM_0,
//     .sda_io_num=GPIO_NUM_21,
//     .scl_io_num=GPIO_NUM_40,
//     .glitch_ignore_cnt=7,
//     .flags.enable_internal_pullup=true,
// };
// i2c_master_bus_handle_t i2c_handle;

// i2c_device_config_t i2c_dev_config={
//     .dev_addr_length=I2C_ADDR_BIT_7,
//     .device_address=0x5D,
//     .scl_speed_hz=10000,
// };
// i2c_master_dev_handle_t i2c_dev_handle;

// void i2c_init(void){
//     uint8_t reg=0x8140;
//     uint8_t buf[2];
//     uint8_t buffer[2];
//     ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_config,&i2c_handle));
//     ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_handle,&i2c_dev_config,&i2c_dev_handle));
//     ESP_ERROR_CHECK(i2c_master_probe(i2c_handle,0x5D,-1));

// }
Vernon_GT911 vernonGT911;
#if CONFIG_EXAMPLE_LCD_USE_TOUCH_ENABLED
static void example_lvgl_touch_cb(lv_indev_t* indev,lv_indev_data_t* data){
    uint16_t x,y;
    if(GT911_touched(&vernonGT911)){
        GT911_read_pos(&vernonGT911,&x,&y,0);
        data->point.x=x;
        data->point.y=y;
        data->state=LV_INDEV_STATE_PRESSED;
    }else{
        data->state=LV_INDEV_STATE_RELEASED;
    }
}
#endif
// LVGL library is not thread-safe, this example will call LVGL APIs from different tasks, so use a mutex to protect it
static _lock_t lvgl_api_lock;

extern void example_lvgl_demo_ui(lv_display_t *disp);

static bool example_notify_lvgl_flush_ready(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *event_data, void *user_ctx)
{
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

static void example_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // pass the draw buffer to the driver
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);
}

static void example_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

static void example_lvgl_port_task(void *arg)
{
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

static void example_bsp_init_lcd_backlight(void)
{
#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_BK_LIGHT
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
#endif
}

static void example_bsp_set_lcd_backlight(uint32_t level)
{
#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
    gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, level);
#endif
}

void GT911_test(void *param){
    uint16_t x,y;
    while (1){
        if(GT911_touched(&vernonGT911)){
            //
            // for(int i=0;i<5;i++){
                GT911_read_pos(&vernonGT911,&x,&y,0);
                ESP_LOGW(TAG,"No: %d, touched x: %d, touched y: %d\n", 0,  x, y);
            // }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

}

void app_main(void)
{
    xTaskCreate(GT911_test,"i2c",1024*4,NULL,3,NULL);
#if CONFIG_EXAMPLE_LCD_USE_TOUCH_ENABLED
    GT911_init(&vernonGT911, TOUCH_I2C_SDA,TOUCH_I2C_SCL,TOUCH_PIN_INT,
               TOUCH_PIN_RTN, I2C_NUM_0,GT911_ADDR1,
               TOUCH_PAD_WIDTH, TOUCH_PAD_HEIGHT);

    GT911_setRotation(&vernonGT911,ROTATION_NORMAL);
    ESP_LOGW(TAG,"GT911 TouchPad Init");
#endif
    ESP_LOGI(TAG, "Turn off LCD backlight");
    example_bsp_init_lcd_backlight();
    example_bsp_set_lcd_backlight(EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL);

    ESP_LOGI(TAG,"Initialize 3-Wire SPI Panel IO");
    spi_line_config_t line_config={
        .cs_io_type=IO_TYPE_GPIO,
        .cs_gpio_num=PIN_NUM_CS,
        .scl_io_type=IO_TYPE_GPIO,
        .scl_gpio_num=PIN_NUM_SCL,
        .sda_io_type=IO_TYPE_GPIO,
        .sda_gpio_num=PIN_NUM_SDA,
        .io_expander=NULL,
    };
#ifdef CONFIG_EXAMPLE_LCD_CONTROLLER_ST7701S
    esp_lcd_panel_io_3wire_spi_config_t io_config=ST7701_PANEL_IO_3WIRE_SPI_CONFIG(line_config,0);
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_NV3052C
    esp_lcd_panel_io_3wire_spi_config_t io_config=NV3052_PANEL_IO_3WIRE_SPI_CONFIG(line_config,0);
#elif CONFIG_EXAMPLE_LCD_H040A18
    esp_lcd_panel_io_3wire_spi_config_t io_config=H040A18_PANEL_IO_3WIRE_SPI_CONFIG(line_config,0);
#elif CONFIG_EXAMPLE_LCD_H035A17
    esp_lcd_panel_io_3wire_spi_config_t io_config=H035A17_PANEL_IO_3WIRE_SPI_CONFIG(line_config,0);
#endif
    esp_lcd_panel_io_handle_t io_handle=NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_3wire_spi(&io_config,&io_handle));

    ESP_LOGI(TAG, "Install RGB LCD panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_rgb_panel_config_t panel_config = {
        .data_width = EXAMPLE_DATA_BUS_WIDTH,
        .dma_burst_size = 64,
        .num_fbs = EXAMPLE_LCD_NUM_FB,
#if CONFIG_EXAMPLE_USE_BOUNCE_BUFFER
        .bounce_buffer_size_px = 20 * EXAMPLE_LCD_H_RES,
#endif
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
#if CONFIG_EXAMPLE_LCD_DATA_LINES > 16
            EXAMPLE_PIN_NUM_DATA16,
            EXAMPLE_PIN_NUM_DATA17,
            EXAMPLE_PIN_NUM_DATA18,
            EXAMPLE_PIN_NUM_DATA19,
            EXAMPLE_PIN_NUM_DATA20,
            EXAMPLE_PIN_NUM_DATA21,
            EXAMPLE_PIN_NUM_DATA22,
            EXAMPLE_PIN_NUM_DATA23
#endif
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
    
#ifdef CONFIG_EXAMPLE_LCD_CONTROLLER_ST7701S
    st7701_vendor_config_t vendor_config={
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_NV3052C
    nv3052_vendor_config_t vendor_config={
#elif CONFIG_EXAMPLE_LCD_H040A18
    h040a18_vendor_config_t vendor_config={
#elif CONFIG_EXAMPLE_LCD_H035A17  
    h035a17_vendor_config_t vendor_config={      
#endif
        .rgb_config=&panel_config,
        .flags={
            .mirror_by_cmd=1,
            .enable_io_multiplex=0,
        }
    };
    
    esp_lcd_panel_dev_config_t panel_dev_config={
        .reset_gpio_num=PIN_NUM_RST,
        .rgb_ele_order=LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel=16,
        .vendor_config=&vendor_config,
    };

#ifdef CONFIG_EXAMPLE_LCD_CONTROLLER_ST7701S
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7701_rgb(io_handle,&panel_dev_config,&panel_handle));
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_NV3052C
    ESP_ERROR_CHECK(esp_lcd_new_panel_nv3052_rgb(io_handle,&panel_dev_config,&panel_handle));
#elif CONFIG_EXAMPLE_LCD_H040A18
    ESP_ERROR_CHECK(esp_lcd_new_panel_h040a18(io_handle,&panel_dev_config,&panel_handle));
#elif CONFIG_EXAMPLE_LCD_H035A17
    ESP_ERROR_CHECK(esp_lcd_new_panel_h035a17(io_handle,&panel_dev_config,&panel_handle));
#endif

    ESP_LOGI(TAG, "Initialize RGB LCD panel");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    // ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "Turn on LCD backlight");
    example_bsp_set_lcd_backlight(EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    // create a lvgl display
    lv_display_t *display = lv_display_create(EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
    // associate the rgb panel handle to the display
    lv_display_set_user_data(display, panel_handle);
    // set color depth
    lv_display_set_color_format(display, EXAMPLE_LV_COLOR_FORMAT);
    // create draw buffers
    void *buf1 = NULL;
    void *buf2 = NULL;
#if CONFIG_EXAMPLE_USE_DOUBLE_FB
    ESP_LOGI(TAG, "Use frame buffers as LVGL draw buffers");
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(panel_handle, 2, &buf1, &buf2));
    // set LVGL draw buffers and direct mode
    lv_display_set_buffers(display, buf1, buf2, EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * EXAMPLE_PIXEL_SIZE, LV_DISPLAY_RENDER_MODE_DIRECT);
#else
    ESP_LOGI(TAG, "Allocate LVGL draw buffers");
    // it's recommended to allocate the draw buffer from internal memory, for better performance
    size_t draw_buffer_sz = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES/10 * EXAMPLE_PIXEL_SIZE;
    buf1 = heap_caps_malloc(draw_buffer_sz, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    assert(buf1);
    // set LVGL draw buffers and partial mode
    lv_display_set_buffers(display, buf1, buf2, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
#endif // CONFIG_EXAMPLE_USE_DOUBLE_FB

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
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

#if CONFIG_EXAMPLE_LCD_USE_TOUCH_ENABLED
    static lv_indev_t* touch_indev;
    touch_indev=lv_indev_create();
    lv_indev_set_type(touch_indev,LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(touch_indev,display);
    lv_indev_set_read_cb(touch_indev,example_lvgl_touch_cb);
#endif
    ESP_LOGI(TAG, "Create LVGL task");
    xTaskCreate(example_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "Display LVGL UI");
    // Lock the mutex due to the LVGL APIs are not thread-safe
    _lock_acquire(&lvgl_api_lock);
    example_lvgl_demo_ui(display);
    _lock_release(&lvgl_api_lock);
}
