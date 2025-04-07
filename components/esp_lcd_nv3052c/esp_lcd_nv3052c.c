/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "soc/soc_caps.h"

#if SOC_LCD_RGB_SUPPORTED
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_lcd_nv3052c.h"
// #include "esp_lcd_nv3052_interface.h"
typedef struct {
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    uint8_t madctl_val; // Save current value of LCD_CMD_MADCTL register
    uint8_t colmod_val; // Save current value of LCD_CMD_COLMOD register
    const nv3052_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
    struct {
        unsigned int mirror_by_cmd: 1;
        unsigned int enable_io_multiplex: 1;
        unsigned int display_on_off_use_cmd: 1;
        unsigned int reset_level: 1;
    } flags;
    // To save the original functions of RGB panel
    esp_err_t (*init)(esp_lcd_panel_t *panel);
    esp_err_t (*del)(esp_lcd_panel_t *panel);
    esp_err_t (*reset)(esp_lcd_panel_t *panel);
    esp_err_t (*mirror)(esp_lcd_panel_t *panel, bool x_axis, bool y_axis);
    esp_err_t (*disp_on_off)(esp_lcd_panel_t *panel, bool on_off);
} nv3052_panel_t;

static const char *TAG = "nv3052_rgb";

static esp_err_t panel_nv3052_send_init_cmds(nv3052_panel_t *nv3052);

static esp_err_t panel_nv3052_init(esp_lcd_panel_t *panel);
static esp_err_t panel_nv3052_del(esp_lcd_panel_t *panel);
static esp_err_t panel_nv3052_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_nv3052_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_nv3052_disp_on_off(esp_lcd_panel_t *panel, bool off);

esp_err_t esp_lcd_new_panel_nv3052_rgb(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                       esp_lcd_panel_handle_t *ret_panel)
{
    ESP_RETURN_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");
    nv3052_vendor_config_t *vendor_config = (nv3052_vendor_config_t *)panel_dev_config->vendor_config;
    ESP_RETURN_ON_FALSE(vendor_config && vendor_config->rgb_config, ESP_ERR_INVALID_ARG, TAG, "`verndor_config` and `rgb_config` are necessary");
    ESP_RETURN_ON_FALSE(!vendor_config->flags.enable_io_multiplex || !vendor_config->flags.mirror_by_cmd,
                        ESP_ERR_INVALID_ARG, TAG, "`mirror_by_cmd` and `enable_io_multiplex` cannot work together");

    esp_err_t ret = ESP_OK;
    nv3052_panel_t *nv3052 = (nv3052_panel_t *)calloc(1, sizeof(nv3052_panel_t));
    ESP_RETURN_ON_FALSE(nv3052, ESP_ERR_NO_MEM, TAG, "no mem for nv3052 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    switch (panel_dev_config->rgb_ele_order) {
    case LCD_RGB_ELEMENT_ORDER_RGB:
        nv3052->madctl_val = 0;
        break;
    case LCD_RGB_ELEMENT_ORDER_BGR:
        nv3052->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color element order");
        break;
    }

    nv3052->colmod_val = 0;
    switch (panel_dev_config->bits_per_pixel) {
    case 16: // RGB565
        nv3052->colmod_val = 0x50;
        break;
    case 18: // RGB666
        nv3052->colmod_val = 0x60;
        break;
    case 24: // RGB888
        nv3052->colmod_val = 0x70;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
        break;
    }

    nv3052->io = io;
    nv3052->init_cmds = vendor_config->init_cmds;
    nv3052->init_cmds_size = vendor_config->init_cmds_size;
    nv3052->reset_gpio_num = panel_dev_config->reset_gpio_num;
    nv3052->flags.mirror_by_cmd = vendor_config->flags.mirror_by_cmd;
    nv3052->flags.display_on_off_use_cmd = (vendor_config->rgb_config->disp_gpio_num >= 0) ? 0 : 1;
    nv3052->flags.enable_io_multiplex = vendor_config->flags.enable_io_multiplex;
    nv3052->flags.reset_level = panel_dev_config->flags.reset_active_high;

    if (nv3052->flags.enable_io_multiplex) {
        if (nv3052->reset_gpio_num >= 0) {  // Perform hardware reset
            gpio_set_level(nv3052->reset_gpio_num, nv3052->flags.reset_level);
            vTaskDelay(pdMS_TO_TICKS(10));
            gpio_set_level(nv3052->reset_gpio_num, !nv3052->flags.reset_level);
        } else { // Perform software reset
            ESP_GOTO_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), err, TAG, "send command failed");
        }
        vTaskDelay(pdMS_TO_TICKS(120));

        /**
         * In order to enable the 3-wire SPI interface pins (such as SDA and SCK) to share other pins of the RGB interface
         * (such as HSYNC) and save GPIOs, we need to send LCD initialization commands via the 3-wire SPI interface before
         * `esp_lcd_new_rgb_panel()` is called.
         */
        ESP_GOTO_ON_ERROR(panel_nv3052_send_init_cmds(nv3052), err, TAG, "send init commands failed");
        // After sending the initialization commands, the 3-wire SPI interface can be deleted
        // ESP_GOTO_ON_ERROR(esp_lcd_panel_io_del(io), err, TAG, "delete panel IO failed");
        // nv3052->io = NULL;
        ESP_LOGE(TAG, "delete panel IO");
    }

    // Create RGB panel
    ESP_GOTO_ON_ERROR(esp_lcd_new_rgb_panel(vendor_config->rgb_config, ret_panel), err, TAG, "create RGB panel failed");
    ESP_LOGW(TAG, "new RGB panel @%p", ret_panel);

    // Save the original functions of RGB panel
    nv3052->init = (*ret_panel)->init;
    nv3052->del = (*ret_panel)->del;
    nv3052->reset = (*ret_panel)->reset;
    nv3052->mirror = (*ret_panel)->mirror;
    nv3052->disp_on_off = (*ret_panel)->disp_on_off;
    // Overwrite the functions of RGB panel
    (*ret_panel)->init = panel_nv3052_init;
    (*ret_panel)->del = panel_nv3052_del;
    (*ret_panel)->reset = panel_nv3052_reset;
    (*ret_panel)->mirror = panel_nv3052_mirror;
    (*ret_panel)->disp_on_off = panel_nv3052_disp_on_off;
    (*ret_panel)->user_data = nv3052;
    ESP_LOGW(TAG, "new nv3052 panel @%p", nv3052);

    return ESP_OK;

err:
    if (nv3052) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(nv3052);
    }
    return ret;
}

static const nv3052_lcd_init_cmd_t rgb_lcd_init_cmds[] = {
//  {cmd, { data }, data_size, delay_ms}
    {0xFF, (uint8_t []){0x30}, 1, 0},
    {0xFF, (uint8_t []){0x52}, 1, 0},
    {0xFF, (uint8_t []){0x01}, 1, 0},
    {0xE3, (uint8_t []){0x00}, 1, 0},
    {0x0A, (uint8_t []){0x01}, 1, 0},
    {0x23, (uint8_t []){0xA2}, 1, 0},//RGB MODE
    {0x25, (uint8_t []){0x14}, 1, 0},
    {0x29, (uint8_t []){0x02}, 1, 0},
    {0x2A, (uint8_t []){0xCF}, 1, 0},
    {0x38, (uint8_t []){0x9C}, 1, 0},
    {0x39, (uint8_t []){0xA7}, 1, 0},
    {0x3A, (uint8_t []){0x33}, 1, 0},//VCOM
    {0x91, (uint8_t []){0x77}, 1, 0},
    {0x92, (uint8_t []){0x77}, 1, 0},
    {0x99, (uint8_t []){0x52}, 1, 0},
    {0x9B, (uint8_t []){0x5B}, 1, 0},
    {0xA0, (uint8_t []){0x55}, 1, 0},
    {0xA1, (uint8_t []){0x50}, 1, 0},
    {0xA4, (uint8_t []){0x9C}, 1, 0},
    {0xA7, (uint8_t []){0x02}, 1, 0},
    {0xA8, (uint8_t []){0x01}, 1, 0},
    {0xA9, (uint8_t []){0x01}, 1, 0},
    {0xAA, (uint8_t []){0xFC}, 1, 0},
    {0xAB, (uint8_t []){0x28}, 1, 0},
    {0xAC, (uint8_t []){0x06}, 1, 0},
    {0xAD, (uint8_t []){0x06}, 1, 0},
    {0xAE, (uint8_t []){0x06}, 1, 0},
    {0xAF, (uint8_t []){0x03}, 1, 0},
    {0xB0, (uint8_t []){0x08}, 1, 0},
    {0xB1, (uint8_t []){0x26}, 1, 0},
    {0xB2, (uint8_t []){0x28}, 1, 0},
    {0xB3, (uint8_t []){0x28}, 1, 0},
    {0xB4, (uint8_t []){0x03}, 1, 0},
    {0xB5, (uint8_t []){0x08}, 1, 0},
    {0xB6, (uint8_t []){0x26}, 1, 0},
    {0xB7, (uint8_t []){0x08}, 1, 0},
    {0xB8, (uint8_t []){0x26}, 1, 0},
    {0xFF, (uint8_t []){0x30}, 1, 0},
    {0xFF, (uint8_t []){0x52}, 1, 0},
    {0xFF, (uint8_t []){0x02}, 1, 0},
    {0xB0, (uint8_t []){0x02}, 1, 0},
    {0xB1, (uint8_t []){0x31}, 1, 0},
    {0xB2, (uint8_t []){0x24}, 1, 0},
    {0xB3, (uint8_t []){0x30}, 1, 0},
    {0xB4, (uint8_t []){0x38}, 1, 0},
    {0xB5, (uint8_t []){0x3E}, 1, 0},
    {0xB6, (uint8_t []){0x26}, 1, 0},
    {0xB7, (uint8_t []){0x3E}, 1, 0},
    {0xB8, (uint8_t []){0x0a}, 1, 0},
    {0xB9, (uint8_t []){0x00}, 1, 0},
    {0xBA, (uint8_t []){0x11}, 1, 0},
    {0xBB, (uint8_t []){0x11}, 1, 0},
    {0xBC, (uint8_t []){0x13}, 1, 0},
    {0xBD, (uint8_t []){0x14}, 1, 0},
    {0xBE, (uint8_t []){0x18}, 1, 0},
    {0xBF, (uint8_t []){0x11}, 1, 0},
    {0xC0, (uint8_t []){0x16}, 1, 0},
    {0xC1, (uint8_t []){0x00}, 1, 0},
    {0xD0, (uint8_t []){0x05}, 1, 0},
    {0xD1, (uint8_t []){0x30}, 1, 0},
    {0xD2, (uint8_t []){0x25}, 1, 0},
    {0xD3, (uint8_t []){0x35}, 1, 0},
    {0xD4, (uint8_t []){0x34}, 1, 0},
    {0xD5, (uint8_t []){0x3B}, 1, 0},
    {0xD6, (uint8_t []){0x26}, 1, 0},
    {0xD7, (uint8_t []){0x3D}, 1, 0},
    {0xD8, (uint8_t []){0x0a}, 1, 0},
    {0xD9, (uint8_t []){0x00}, 1, 0},
    {0xDA, (uint8_t []){0x12}, 1, 0},
    {0xDB, (uint8_t []){0x10}, 1, 0},
    {0xDC, (uint8_t []){0x12}, 1, 0},
    {0xDD, (uint8_t []){0x14}, 1, 0},
    {0xDE, (uint8_t []){0x18}, 1, 0},
    {0xDF, (uint8_t []){0x11}, 1, 0},
    {0xE0, (uint8_t []){0x15}, 1, 0},
    {0xE1, (uint8_t []){0x00}, 1, 0},
    {0xFF, (uint8_t []){0x30}, 1, 0},
    {0xFF, (uint8_t []){0x52}, 1, 0},
    {0xFF, (uint8_t []){0x03}, 1, 0},
    {0x08, (uint8_t []){0x09}, 1, 0},
    {0x09, (uint8_t []){0x0A}, 1, 0},
    {0x0A, (uint8_t []){0x0B}, 1, 0},
    {0x0B, (uint8_t []){0x0C}, 1, 0},
    {0x28, (uint8_t []){0x22}, 1, 0},
    {0x2A, (uint8_t []){0xEC}, 1, 0},
    {0x2B, (uint8_t []){0xEC}, 1, 0},
    {0x30, (uint8_t []){0x00}, 1, 0},
    {0x31, (uint8_t []){0x00}, 1, 0},
    {0x32, (uint8_t []){0x00}, 1, 0},
    {0x33, (uint8_t []){0x00}, 1, 0},
    {0x34, (uint8_t []){0x61}, 1, 0},
    {0x35, (uint8_t []){0xD4}, 1, 0},
    {0x36, (uint8_t []){0x24}, 1, 0},
    {0x37, (uint8_t []){0x03}, 1, 0},
    {0x40, (uint8_t []){0x0D}, 1, 0},
    {0x41, (uint8_t []){0x0E}, 1, 0},
    {0x42, (uint8_t []){0x0F}, 1, 0},
    {0x43, (uint8_t []){0x10}, 1, 0},
    {0x44, (uint8_t []){0x22}, 1, 0},
    {0x45, (uint8_t []){0xE1}, 1, 0},
    {0x46, (uint8_t []){0xE2}, 1, 0},
    {0x47, (uint8_t []){0x22}, 1, 0},
    {0x48, (uint8_t []){0xE3}, 1, 0},
    {0x49, (uint8_t []){0xE4}, 1, 0},
    {0x50, (uint8_t []){0x11}, 1, 0},
    {0x51, (uint8_t []){0x12}, 1, 0},
    {0x52, (uint8_t []){0x13}, 1, 0},
    {0x53, (uint8_t []){0x14}, 1, 0},
    {0x54, (uint8_t []){0x22}, 1, 0},
    {0x55, (uint8_t []){0xE5}, 1, 0},
    {0x56, (uint8_t []){0xE6}, 1, 0},
    {0x57, (uint8_t []){0x22}, 1, 0},
    {0x58, (uint8_t []){0xE7}, 1, 0},
    {0x59, (uint8_t []){0xE8}, 1, 0},
    {0x80, (uint8_t []){0x05}, 1, 0},
    {0x81, (uint8_t []){0x1E}, 1, 0},
    {0x82, (uint8_t []){0x02}, 1, 0},
    {0x83, (uint8_t []){0x04}, 1, 0},
    {0x84, (uint8_t []){0x1E}, 1, 0},
    {0x85, (uint8_t []){0x1E}, 1, 0},
    {0x86, (uint8_t []){0x1f}, 1, 0},
    {0x87, (uint8_t []){0x1f}, 1, 0},
    {0x88, (uint8_t []){0x0E}, 1, 0},
    {0x89, (uint8_t []){0x10}, 1, 0},
    {0x8A, (uint8_t []){0x0A}, 1, 0},
    {0x8B, (uint8_t []){0x0C}, 1, 0},
    {0x96, (uint8_t []){0x05}, 1, 0},
    {0x97, (uint8_t []){0x1E}, 1, 0},
    {0x98, (uint8_t []){0x01}, 1, 0},
    {0x99, (uint8_t []){0x03}, 1, 0},
    {0x9A, (uint8_t []){0x1E}, 1, 0},
    {0x9B, (uint8_t []){0x1E}, 1, 0},
    {0x9C, (uint8_t []){0x1f}, 1, 0},
    {0x9D, (uint8_t []){0x1f}, 1, 0},
    {0x9E, (uint8_t []){0x0D}, 1, 0},
    {0x9F, (uint8_t []){0x0F}, 1, 0},
    {0xA0, (uint8_t []){0x09}, 1, 0},
    {0xA1, (uint8_t []){0x0B}, 1, 0},
    {0xB0, (uint8_t []){0x05}, 1, 0},
    {0xB1, (uint8_t []){0x1F}, 1, 0},
    {0xB2, (uint8_t []){0x03}, 1, 0},
    {0xB3, (uint8_t []){0x01}, 1, 0},
    {0xB4, (uint8_t []){0x1E}, 1, 0},
    {0xB5, (uint8_t []){0x1E}, 1, 0},
    {0xB6, (uint8_t []){0x1f}, 1, 0},
    {0xB7, (uint8_t []){0x1E}, 1, 0},
    {0xB8, (uint8_t []){0x0B}, 1, 0},
    {0xB9, (uint8_t []){0x09}, 1, 0},
    {0xBA, (uint8_t []){0x0F}, 1, 0},
    {0xBB, (uint8_t []){0x0D}, 1, 0},
    {0xC6, (uint8_t []){0x05}, 1, 0},
    {0xC7, (uint8_t []){0x1F}, 1, 0},
    {0xC8, (uint8_t []){0x04}, 1, 0},
    {0xC9, (uint8_t []){0x02}, 1, 0},
    {0xCA, (uint8_t []){0x1E}, 1, 0},
    {0xCB, (uint8_t []){0x1E}, 1, 0},
    {0xCC, (uint8_t []){0x1f}, 1, 0},
    {0xCD, (uint8_t []){0x1E}, 1, 0},
    {0xCE, (uint8_t []){0x0C}, 1, 0},
    {0xCF, (uint8_t []){0x0A}, 1, 0},
    {0xD0, (uint8_t []){0x10}, 1, 0},
    {0xD1, (uint8_t []){0x0E}, 1, 0},
    {0xFF, (uint8_t []){0x30}, 1, 0},
    {0xFF, (uint8_t []){0x52}, 1, 0},
    {0xFF, (uint8_t []){0x00}, 1, 0},
    {0x36, (uint8_t []){0x02}, 1, 0},//反扫09
    {0x3A, (uint8_t []){0x55}, 1, 0},//16BIT
    {0x11, (uint8_t []){0x00}, 1, 200},
    {0x29, (uint8_t []){0x00}, 1, 100},
};

static esp_err_t panel_nv3052_send_init_cmds(nv3052_panel_t *nv3052)
{
    esp_lcd_panel_io_handle_t io = nv3052->io;
    const nv3052_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;
    bool is_command2_disable = true;
    bool is_cmd_overwritten = false;

    // ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, NV3052_CMD_CND2BKxSEL, (uint8_t []) {
    //     NV3052_CMD_BKxSEL_BYTE0, NV3052_CMD_BKxSEL_BYTE1, NV3052_CMD_BKxSEL_BYTE2, NV3052_CMD_BKxSEL_BYTE3, 0x00
    // }, 5), TAG, "Write cmd failed");
    // Set color format
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t []) {
        nv3052->madctl_val
    }, 1), TAG, "Write cmd failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_COLMOD, (uint8_t []) {
        nv3052->colmod_val
    }, 1), TAG, "Write cmd failed");

    // vendor specific initialization, it can be different between manufacturers
    // should consult the LCD supplier for initialization sequence code
    if (nv3052->init_cmds) {
        init_cmds = nv3052->init_cmds;
        init_cmds_size = nv3052->init_cmds_size;
    } else {
        init_cmds = rgb_lcd_init_cmds;
        init_cmds_size = sizeof(rgb_lcd_init_cmds) / sizeof(nv3052_lcd_init_cmd_t);
    }

    for (int i = 0; i < init_cmds_size; i++) {
        // Check if the command has been used or conflicts with the internal only when command2 is disable
        if (is_command2_disable && (init_cmds[i].data_bytes > 0)) {
            switch (init_cmds[i].cmd) {
            case LCD_CMD_MADCTL:
                is_cmd_overwritten = true;
                nv3052->madctl_val = ((uint8_t *)init_cmds[i].data)[0];
                break;
            case LCD_CMD_COLMOD:
                is_cmd_overwritten = true;
                nv3052->colmod_val = ((uint8_t *)init_cmds[i].data)[0];
                break;
            default:
                is_cmd_overwritten = false;
                break;
            }

            if (is_cmd_overwritten) {
                is_cmd_overwritten = false;
                ESP_LOGW(TAG, "The %02Xh command has been used and will be overwritten by external initialization sequence",
                         init_cmds[i].cmd);
            }
        }

        // Send command
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_bytes),
                            TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));

        // Check if the current cmd is the command2 disable cmd
        // if ((init_cmds[i].cmd == nv3052_CMD_CND2BKxSEL) && (init_cmds[i].data_bytes > 4)) {
        //     is_command2_disable = !(((uint8_t *)init_cmds[i].data)[4] & nv3052_CMD_CN2_BIT);
        // }
        ESP_LOGW(TAG,"send commands %d/%d",i,init_cmds_size);
    }
    ESP_LOGI(TAG, "send init commands success");

    return ESP_OK;
}

static esp_err_t panel_nv3052_init(esp_lcd_panel_t *panel)
{
    nv3052_panel_t *nv3052 = (nv3052_panel_t *)panel->user_data;

    if (!nv3052->flags.enable_io_multiplex) {
        ESP_RETURN_ON_ERROR(panel_nv3052_send_init_cmds(nv3052), TAG, "send init commands failed");
    }
    // Init RGB panel
    ESP_RETURN_ON_ERROR(nv3052->init(panel), TAG, "init RGB panel failed");

    return ESP_OK;
}

static esp_err_t panel_nv3052_del(esp_lcd_panel_t *panel)
{
    nv3052_panel_t *nv3052 = (nv3052_panel_t *)panel->user_data;

    if (nv3052->reset_gpio_num >= 0) {
        gpio_reset_pin(nv3052->reset_gpio_num);
    }
    // Delete RGB panel
    nv3052->del(panel);
    free(nv3052);
    ESP_LOGD(TAG, "del nv3052 panel @%p", nv3052);
    return ESP_OK;
}

static esp_err_t panel_nv3052_reset(esp_lcd_panel_t *panel)
{
    nv3052_panel_t *nv3052 = (nv3052_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = nv3052->io;

    // Perform hardware reset
    if (nv3052->reset_gpio_num >= 0) {
        gpio_set_level(nv3052->reset_gpio_num, nv3052->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(nv3052->reset_gpio_num, !nv3052->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(120));
    } else if (io) { // Perform software reset
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(120));
    }
    // Reset RGB panel
    ESP_RETURN_ON_ERROR(nv3052->reset(panel), TAG, "reset RGB panel failed");

    return ESP_OK;
}

static esp_err_t panel_nv3052_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    nv3052_panel_t *nv3052 = (nv3052_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = nv3052->io;
    uint8_t sdir_val = 0;

    if (nv3052->flags.mirror_by_cmd) {
        ESP_RETURN_ON_FALSE(io, ESP_FAIL, TAG, "Panel IO is deleted, cannot send command");
        // Control mirror through LCD command
        if (mirror_x) {
            sdir_val = 1<<2;
        } else {
            sdir_val = 0;
        }
        if (mirror_y) {
            nv3052->madctl_val |= LCD_CMD_ML_BIT;
        } else {
            nv3052->madctl_val &= ~LCD_CMD_ML_BIT;
        }
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, NV3052_CMD_SDIR, (uint8_t[]) {
            sdir_val,
        }, 1), TAG, "send command failed");;
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
            nv3052->madctl_val,
        }, 1), TAG, "send command failed");;
    } else {
        // Control mirror through RGB panel
        ESP_RETURN_ON_ERROR(nv3052->mirror(panel, mirror_x, mirror_y), TAG, "RGB panel mirror failed");
    }
    return ESP_OK;
}

static esp_err_t panel_nv3052_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    nv3052_panel_t *nv3052 = (nv3052_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = nv3052->io;
    int command = 0;

    if (nv3052->flags.display_on_off_use_cmd) {
        ESP_RETURN_ON_FALSE(io, ESP_FAIL, TAG, "Panel IO is deleted, cannot send command");
        // Control display on/off through LCD command
        if (on_off) {
            command = LCD_CMD_DISPON;
        } else {
            command = LCD_CMD_DISPOFF;
        }
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");
    } else {
        // Control display on/off through display control signal
        ESP_RETURN_ON_ERROR(nv3052->disp_on_off(panel, on_off), TAG, "RGB panel disp_on_off failed");
    }
    return ESP_OK;
}
#endif
