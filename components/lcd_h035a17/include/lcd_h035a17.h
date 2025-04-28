#pragma once

#include "stdint.h"

#include "hal/lcd_types.h"
#include "esp_lcd_panel_vendor.h"

#if SOC_LCD_RGB_SUPPORTED
#include "esp_lcd_panel_rgb.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LCD panel initialization cmds.
 * 
 */
typedef struct{
    int cmd;
    const void* data;
    size_t data_bytes;
    unsigned int delay_ms;
} h035a17_lcd_init_cmd_t;

typedef struct{
    const h035a17_lcd_init_cmd_t *init_cmds;

    uint16_t init_cmds_size;
    union{
        const esp_lcd_rgb_panel_config_t *rgb_config;
    };
    struct{
        unsigned int use_mipi_interface: 1;
        unsigned int mirror_by_cmd: 1;

        union{
            unsigned int auto_del_panel_io: 1;
            unsigned int enable_io_multiplex: 1;
        };
    }flags;
}h035a17_vendor_config_t;

/**
 * @brief Create LCD panel for model h035a17
 *
 * @note  When `enable_io_multiplex` is set to 1, this function will first initialize the nv3052 with vendor specific initialization and then calls `esp_lcd_new_rgb_panel()` to create an RGB LCD panel. And the `esp_lcd_panel_init()` function will only initialize RGB.
 * @note  When `enable_io_multiplex` is set to 0, this function will only call `esp_lcd_new_rgb_panel()` to create an RGB LCD panel. And the `esp_lcd_panel_init()` function will initialize both the nv3052 and RGB.
 * @note  Vendor specific initialization can be different between manufacturers, should consult the LCD supplier for initialization sequence code.
 *
 * @param[in]  io LCD panel IO handle
 * @param[in]  panel_dev_config General panel device configuration (`vendor_config` and `rgb_config` are necessary)
 * @param[out] ret_panel Returned LCD panel handle
 * @return
 *      - ESP_ERR_INVALID_ARG   if parameter is invalid
 *      - ESP_OK                on success
 *      - Otherwise             on fail
 */
esp_err_t esp_lcd_new_panel_h035a17(const esp_lcd_panel_io_handle_t io_handle,const esp_lcd_panel_dev_config_t *panel_dev_config,esp_lcd_panel_handle_t *ret_panel);

/**
 * @brief 3-wire SPI panel IO configuration structure
 *
 * @param[in] line_cfg SPI line configuration
 * @param[in] scl_active_edge SCL signal active edge, 0: rising edge, 1: falling edge
 *
 */
#define H035A17_PANEL_IO_3WIRE_SPI_CONFIG(line_cfg,scl_active_edge) \
    {                                                               \    
    .line_config=line_cfg,                                          \
    .expect_clk_speed=PANEL_IO_3WIRE_SPI_CLK_MAX,                   \
        .spi_mode = scl_active_edge ? 1 : 0,                        \
        .lcd_cmd_bytes = 1,                                         \
        .lcd_param_bytes = 1,                                       \
        .flags = {                                                  \
            .use_dc_bit = 1,                                        \
            .dc_zero_on_data = 0,                                   \
            .lsb_first = 0,                                         \
            .cs_high_active = 0,                                    \
            .del_keep_cs_inactive = 1,                              \
        },                                                          \
    }
    
#ifdef __cplusplus
}
#endif