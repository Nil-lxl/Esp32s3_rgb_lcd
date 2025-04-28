#include "soc/soc_caps.h"

#if SOC_LCD_RGB_SUPPORTED
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lcd_h035a17.h"

typedef struct {
    esp_lcd_panel_io_handle_t io_handle;
    int reset_gpio_num;
    uint8_t madctl_val;
    uint8_t colmod_val;
    const h035a17_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
    struct {
        unsigned int mirror_by_cmd : 1;
        unsigned int enable_io_multiplex : 1;
        unsigned int display_on_off_use_cmd : 1;
        unsigned int reset_level : 1;
    } flags;

    esp_err_t (*init)(esp_lcd_panel_t *panel);
    esp_err_t (*del)(esp_lcd_panel_t *panel);
    esp_err_t (*reset)(esp_lcd_panel_t *panel);
    esp_err_t (*mirror)(esp_lcd_panel_t *panel, bool x_axis, bool y_axis);
    esp_err_t (*disp_on_off)(esp_lcd_panel_t *panel, bool on_off);

} h035a17_panel_t;

static const char *TAG = "h035a17_rgb";

static esp_err_t panel_h035a17_send_init_cmds(h035a17_panel_t *h035a17);
static esp_err_t panel_h035a17_init(esp_lcd_panel_t *panel);
static esp_err_t panel_h035a17_del(esp_lcd_panel_t *panel);
static esp_err_t panel_h035a17_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_h035a17_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_h035a17_disp_on_off(esp_lcd_panel_t *panel, bool off);

esp_err_t esp_lcd_new_panel_h035a17(const esp_lcd_panel_io_handle_t io_handle, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel) {
    ESP_RETURN_ON_FALSE(io_handle && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");
    h035a17_vendor_config_t *vendor_config = (h035a17_vendor_config_t *)panel_dev_config->vendor_config;
    ESP_RETURN_ON_FALSE(vendor_config && vendor_config->rgb_config, ESP_ERR_INVALID_ARG, TAG, "`vendor_config` and `rgb_config` are necessary");
    ESP_RETURN_ON_FALSE(!vendor_config->flags.enable_io_multiplex || !vendor_config->flags.mirror_by_cmd, ESP_ERR_INVALID_ARG, TAG, "`mirror_by_cmd` and `enable_io_multiplex` can't work together");

    esp_err_t ret = ESP_OK;
    h035a17_panel_t *h035a17 = (h035a17_panel_t *)calloc(1, sizeof(h035a17_panel_t));
    ESP_RETURN_ON_FALSE(h035a17, ESP_ERR_NO_MEM, TAG, "no mem for h035a17 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_config = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_config), err, TAG, "configure GPIO for RST sign failed");
    }

    h035a17->io_handle = io_handle;
    h035a17->init_cmds = vendor_config->init_cmds;
    h035a17->init_cmds_size = vendor_config->init_cmds_size;
    h035a17->reset_gpio_num = panel_dev_config->reset_gpio_num;
    h035a17->flags.mirror_by_cmd = vendor_config->flags.mirror_by_cmd;
    h035a17->flags.display_on_off_use_cmd = (vendor_config->rgb_config->disp_gpio_num >= 0) ? 0 : 1;
    h035a17->flags.enable_io_multiplex = vendor_config->flags.enable_io_multiplex;
    h035a17->flags.reset_level = panel_dev_config->flags.reset_active_high;

    if (h035a17->flags.enable_io_multiplex) {
        if (h035a17->reset_gpio_num >= 0) {
            gpio_set_level(h035a17->reset_gpio_num, h035a17->flags.reset_level);
            vTaskDelay(pdMS_TO_TICKS(1));
            gpio_set_level(h035a17->reset_gpio_num, !h035a17->flags.reset_level);
            vTaskDelay(pdMS_TO_TICKS(10));
            gpio_set_level(h035a17->reset_gpio_num, !h035a17->flags.reset_level);
        } else { /* Rst gpio is not used */
            ESP_GOTO_ON_ERROR(esp_lcd_panel_io_tx_param(io_handle, 0x01, NULL, 0), err, TAG, "send soft reset command failed");
        }
        vTaskDelay(pdMS_TO_TICKS(120));
        ESP_GOTO_ON_ERROR(panel_h035a17_send_init_cmds(h035a17), err, TAG, "send init cmds failed");
    }
    /*  Create RGB panel   */
    ESP_GOTO_ON_ERROR(esp_lcd_new_rgb_panel(vendor_config->rgb_config, ret_panel), err, TAG, "create new RGB panel failed");
    ESP_LOGI(TAG, "new RGB panel h035a17");

    /*  Save the original functions of RGB panel  */
    h035a17->init = (*ret_panel)->init;
    h035a17->del = (*ret_panel)->del;
    h035a17->reset = (*ret_panel)->reset;
    h035a17->mirror = (*ret_panel)->mirror;
    h035a17->disp_on_off = (*ret_panel)->disp_on_off;

    /*  Overwrite the functions of RGB panel   */
    (*ret_panel)->init = panel_h035a17_init;
    (*ret_panel)->del = panel_h035a17_del;
    (*ret_panel)->reset = panel_h035a17_reset;
    (*ret_panel)->mirror = panel_h035a17_mirror;
    (*ret_panel)->disp_on_off = panel_h035a17_disp_on_off;
    (*ret_panel)->user_data = h035a17;
    ESP_LOGI(TAG, "new lcd panel h035a17");

    return ESP_OK;
err:
    if (h035a17) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(h035a17);
    }
    return ret;
}

static const h035a17_lcd_init_cmd_t rgb_lcd_init_cmds [] = {
    {0xFF, (uint8_t []){0x30}, 1, 0},
    {0xFF, (uint8_t []){0x52}, 1, 0},
    {0xFF, (uint8_t []){0x01}, 1, 0},
    {0xE3, (uint8_t []){0x00}, 1, 0},
    {0x40, (uint8_t []){0x00}, 1, 0},
    {0x03, (uint8_t []){0x40}, 1, 0},
    {0x04, (uint8_t []){0x00}, 1, 0},
    {0x05, (uint8_t []){0x03}, 1, 0},
    {0x08, (uint8_t []){0x00}, 1, 0},
    {0x09, (uint8_t []){0x07}, 1, 0},
    {0x0A, (uint8_t []){0x01}, 1, 0},
    {0x0B, (uint8_t []){0x32}, 1, 0},
    {0x0C, (uint8_t []){0x32}, 1, 0},
    {0x0D, (uint8_t []){0x0B}, 1, 0},
    {0x0E, (uint8_t []){0x00}, 1, 0},
    {0x23, (uint8_t []){0xA2}, 1, 0},
    {0x24, (uint8_t []){0x0c}, 1, 0},
    {0x25, (uint8_t []){0x06}, 1, 0},
    {0x26, (uint8_t []){0x14}, 1, 0},
    {0x27, (uint8_t []){0x14}, 1, 0},
    {0x38, (uint8_t []){0x9C}, 1, 0},
    {0x39, (uint8_t []){0xA7}, 1, 0},
    {0x28, (uint8_t []){0x40}, 1, 0},
    {0x29, (uint8_t []){0x01}, 1, 0},
    {0x2A, (uint8_t []){0xdf}, 1, 0},
    {0x49, (uint8_t []){0x3C}, 1, 0},
    {0x91, (uint8_t []){0x57}, 1, 0},
    {0x92, (uint8_t []){0x57}, 1, 0},
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
    {0xF0, (uint8_t []){0x00}, 1, 0},
    {0xF6, (uint8_t []){0xC0}, 1, 0},
    {0xFF, (uint8_t []){0x30}, 1, 0},
    {0xFF, (uint8_t []){0x52}, 1, 0},
    {0xFF, (uint8_t []){0x02}, 1, 0},
    {0xB0, (uint8_t []){0x0B}, 1, 0},
    {0xB1, (uint8_t []){0x16}, 1, 0},
    {0xB2, (uint8_t []){0x17}, 1, 0},
    {0xB3, (uint8_t []){0x2C}, 1, 0},
    {0xB4, (uint8_t []){0x32}, 1, 0},
    {0xB5, (uint8_t []){0x3B}, 1, 0},
    {0xB6, (uint8_t []){0x29}, 1, 0},
    {0xB7, (uint8_t []){0x40}, 1, 0},
    {0xB8, (uint8_t []){0x0d}, 1, 0},
    {0xB9, (uint8_t []){0x05}, 1, 0},
    {0xBA, (uint8_t []){0x12}, 1, 0},
    {0xBB, (uint8_t []){0x10}, 1, 0},
    {0xBC, (uint8_t []){0x12}, 1, 0},
    {0xBD, (uint8_t []){0x15}, 1, 0},
    {0xBE, (uint8_t []){0x19}, 1, 0},
    {0xBF, (uint8_t []){0x0E}, 1, 0},
    {0xC0, (uint8_t []){0x16}, 1, 0},
    {0xC1, (uint8_t []){0x0A}, 1, 0},
    {0xD0, (uint8_t []){0x0C}, 1, 0},
    {0xD1, (uint8_t []){0x17}, 1, 0},
    {0xD2, (uint8_t []){0x14}, 1, 0},
    {0xD3, (uint8_t []){0x2E}, 1, 0},
    {0xD4, (uint8_t []){0x32}, 1, 0},
    {0xD5, (uint8_t []){0x3C}, 1, 0},
    {0xD6, (uint8_t []){0x22}, 1, 0},
    {0xD7, (uint8_t []){0x3D}, 1, 0},
    {0xD8, (uint8_t []){0x0D}, 1, 0},
    {0xD9, (uint8_t []){0x07}, 1, 0},
    {0xDA, (uint8_t []){0x13}, 1, 0},
    {0xDB, (uint8_t []){0x13}, 1, 0},
    {0xDC, (uint8_t []){0x11}, 1, 0},
    {0xDD, (uint8_t []){0x15}, 1, 0},
    {0xDE, (uint8_t []){0x19}, 1, 0},
    {0xDF, (uint8_t []){0x10}, 1, 0},
    {0xE0, (uint8_t []){0x17}, 1, 0},
    {0xE1, (uint8_t []){0x0A}, 1, 0},
    {0xFF, (uint8_t []){0x30}, 1, 0},
    {0xFF, (uint8_t []){0x52}, 1, 0},
    {0xFF, (uint8_t []){0x03}, 1, 0},
    {0x00, (uint8_t []){0x2A}, 1, 0},
    {0x01, (uint8_t []){0x2A}, 1, 0},
    {0x02, (uint8_t []){0x2A}, 1, 0},
    {0x03, (uint8_t []){0x2A}, 1, 0},
    {0x04, (uint8_t []){0x61}, 1, 0},
    {0x05, (uint8_t []){0x80}, 1, 0},
    {0x06, (uint8_t []){0xc7}, 1, 0},
    {0x07, (uint8_t []){0x01}, 1, 0},
    {0x08, (uint8_t []){0x03}, 1, 0},
    {0x09, (uint8_t []){0x04}, 1, 0},
    {0x70, (uint8_t []){0x22}, 1, 0},
    {0x71, (uint8_t []){0x80}, 1, 0},
    {0x30, (uint8_t []){0x2A}, 1, 0},
    {0x31, (uint8_t []){0x2A}, 1, 0},
    {0x32, (uint8_t []){0x2A}, 1, 0},
    {0x33, (uint8_t []){0x2A}, 1, 0},
    {0x34, (uint8_t []){0x61}, 1, 0},
    {0x35, (uint8_t []){0xc5}, 1, 0},
    {0x36, (uint8_t []){0x80}, 1, 0},
    {0x37, (uint8_t []){0x23}, 1, 0},
    {0x40, (uint8_t []){0x03}, 1, 0},
    {0x41, (uint8_t []){0x04}, 1, 0},
    {0x42, (uint8_t []){0x05}, 1, 0},
    {0x43, (uint8_t []){0x06}, 1, 0},
    {0x44, (uint8_t []){0x11}, 1, 0},
    {0x45, (uint8_t []){0xe8}, 1, 0},
    {0x46, (uint8_t []){0xe9}, 1, 0},
    {0x47, (uint8_t []){0x11}, 1, 0},
    {0x48, (uint8_t []){0xea}, 1, 0},
    {0x49, (uint8_t []){0xeb}, 1, 0},
    {0x50, (uint8_t []){0x07}, 1, 0},
    {0x51, (uint8_t []){0x08}, 1, 0},
    {0x52, (uint8_t []){0x09}, 1, 0},
    {0x53, (uint8_t []){0x0a}, 1, 0},
    {0x54, (uint8_t []){0x11}, 1, 0},
    {0x55, (uint8_t []){0xec}, 1, 0},
    {0x56, (uint8_t []){0xed}, 1, 0},
    {0x57, (uint8_t []){0x11}, 1, 0},
    {0x58, (uint8_t []){0xef}, 1, 0},
    {0x59, (uint8_t []){0xf0}, 1, 0},
    {0xB1, (uint8_t []){0x01}, 1, 0},
    {0xB4, (uint8_t []){0x15}, 1, 0},
    {0xB5, (uint8_t []){0x16}, 1, 0},
    {0xB6, (uint8_t []){0x09}, 1, 0},
    {0xB7, (uint8_t []){0x0f}, 1, 0},
    {0xB8, (uint8_t []){0x0d}, 1, 0},
    {0xB9, (uint8_t []){0x0b}, 1, 0},
    {0xBA, (uint8_t []){0x00}, 1, 0},
    {0xC7, (uint8_t []){0x02}, 1, 0},
    {0xCA, (uint8_t []){0x17}, 1, 0},
    {0xCB, (uint8_t []){0x18}, 1, 0},
    {0xCC, (uint8_t []){0x0a}, 1, 0},
    {0xCD, (uint8_t []){0x10}, 1, 0},
    {0xCE, (uint8_t []){0x0e}, 1, 0},
    {0xCF, (uint8_t []){0x0c}, 1, 0},
    {0xD0, (uint8_t []){0x00}, 1, 0},
    {0x81, (uint8_t []){0x00}, 1, 0},
    {0x84, (uint8_t []){0x15}, 1, 0},
    {0x85, (uint8_t []){0x16}, 1, 0},
    {0x86, (uint8_t []){0x10}, 1, 0},
    {0x87, (uint8_t []){0x0a}, 1, 0},
    {0x88, (uint8_t []){0x0c}, 1, 0},
    {0x89, (uint8_t []){0x0e}, 1, 0},
    {0x8A, (uint8_t []){0x02}, 1, 0},
    {0x97, (uint8_t []){0x00}, 1, 0},
    {0x9A, (uint8_t []){0x17}, 1, 0},
    {0x9B, (uint8_t []){0x18}, 1, 0},
    {0x9C, (uint8_t []){0x0f}, 1, 0},
    {0x9D, (uint8_t []){0x09}, 1, 0},
    {0x9E, (uint8_t []){0x0b}, 1, 0},
    {0x9F, (uint8_t []){0x0d}, 1, 0},
    {0xA0, (uint8_t []){0x01}, 1, 0},
    {0xFF, (uint8_t []){0x30}, 1, 0},
    {0xFF, (uint8_t []){0x52}, 1, 0},
    {0xFF, (uint8_t []){0x02}, 1, 0},
    {0x01, (uint8_t []){0x01}, 1, 0},
    {0x02, (uint8_t []){0xDA}, 1, 0},
    {0x03, (uint8_t []){0xBA}, 1, 0},
    {0x04, (uint8_t []){0xA8}, 1, 0},
    {0x05, (uint8_t []){0x9A}, 1, 0},
    {0x06, (uint8_t []){0x70}, 1, 0},
    {0x07, (uint8_t []){0xFF}, 1, 0},
    {0x08, (uint8_t []){0x91}, 1, 0},
    {0x09, (uint8_t []){0x90}, 1, 0},
    {0x0A, (uint8_t []){0xFF}, 1, 0},
    {0x0B, (uint8_t []){0x8F}, 1, 0},
    {0x0C, (uint8_t []){0x60}, 1, 0},
    {0x0D, (uint8_t []){0x58}, 1, 0},
    {0x0E, (uint8_t []){0x48}, 1, 0},
    {0x0F, (uint8_t []){0x38}, 1, 0},
    {0x10, (uint8_t []){0x2B}, 1, 0},
    {0xFF, (uint8_t []){0x30}, 1, 0},
    {0xFF, (uint8_t []){0x52}, 1, 0},
    {0xFF, (uint8_t []){0x00}, 1, 0},
    {0x3A, (uint8_t []){0x77}, 1, 0},//RGB 565 format
    {0x36, (uint8_t []){0x0a}, 1, 0},

    {0x11, (uint8_t []){0x00}, 0, 200},

    {0x29, (uint8_t []){0x00}, 0, 20},
};

static esp_err_t panel_h035a17_send_init_cmds(h035a17_panel_t *h035a17) {
    esp_lcd_panel_io_handle_t io_handle = h035a17->io_handle;
    const h035a17_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;

    if (h035a17->init_cmds) {
        init_cmds = h035a17->init_cmds;
        init_cmds_size = h035a17->init_cmds_size;
    } else {
        init_cmds = rgb_lcd_init_cmds;
        init_cmds_size = sizeof(rgb_lcd_init_cmds) / sizeof(h035a17_lcd_init_cmd_t);
    }

    for (int i = 0; i < init_cmds_size; i++) {
        /*  Send Initialization Commands   */
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io_handle, init_cmds [i].cmd, init_cmds [i].data, init_cmds [i].data_bytes), TAG, "send Initialization commands failed");
        vTaskDelay(pdMS_TO_TICKS(init_cmds [i].delay_ms));

        // ESP_LOGW(TAG, "send commands %d/%d", i, init_cmds_size);
    }
    ESP_LOGI(TAG, "send initialization cmds success");

    return ESP_OK;
}

static esp_err_t panel_h035a17_init(esp_lcd_panel_t *panel) {
    h035a17_panel_t *h035a17 = (h035a17_panel_t *)panel->user_data;
    if (!h035a17->flags.enable_io_multiplex) {
        ESP_RETURN_ON_ERROR(panel_h035a17_send_init_cmds(h035a17), TAG, "send init cmds failed");
    }
    /*  Init RGB panel  */
    ESP_RETURN_ON_ERROR(h035a17->init(panel), TAG, "init RGB panel failed");

    return ESP_OK;
}
static esp_err_t panel_h035a17_del(esp_lcd_panel_t *panel) {
    h035a17_panel_t *h035a17 = (h035a17_panel_t *)panel->user_data;

    if (h035a17->reset_gpio_num >= 0) {
        gpio_reset_pin(h035a17->reset_gpio_num);
    }
    /*  Delete RGB panel  */
    h035a17->del(panel);
    free(h035a17);
    ESP_LOGD(TAG, "deleted h035a17 panel");
    return ESP_OK;
}
static esp_err_t panel_h035a17_reset(esp_lcd_panel_t *panel) {
    h035a17_panel_t *h035a17 = (h035a17_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io_handle = h035a17->io_handle;

    /*  Perform hardware reset  */
    if (h035a17->reset_gpio_num >= 0) {
        gpio_set_level(h035a17->reset_gpio_num, h035a17->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(h035a17->reset_gpio_num, !h035a17->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(120));
        // gpio_set_level(h035a17->reset_gpio_num, h035a17->flags.reset_level);
        // vTaskDelay(pdMS_TO_TICKS(120));
    } else if (io_handle) {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io_handle, 0x01, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(120));
    }
    ESP_RETURN_ON_ERROR(h035a17->reset(panel), TAG, "reset RGB panel failed");

    return ESP_OK;
}

static esp_err_t panel_h035a17_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y) {
    // h035a17_panel_t *h035a17 = (h035a17_panel_t *)panel->user_data;
    // esp_lcd_panel_io_handle_t io_handle = h035a17->io_handle;
    // uint8_t sdir_val = 0;

    // if (h035a17->flags.mirror_by_cmd) {
    //     ESP_RETURN_ON_FALSE(io_handle, ESP_FAIL, TAG, "Panel IO is deleted, cannot send command");
    //     // Control mirror through LCD command
    //     if (mirror_x) {
    //         sdir_val = 1<<2;
    //     } else {
    //         sdir_val = 0;
    //     }
    //     if (mirror_y) {
    //         h035a17->madctl_val |= LCD_CMD_ML_BIT;
    //     } else {
    //         h035a17->madctl_val &= ~LCD_CMD_ML_BIT;
    //     }
    //     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io_handle, 0xc7, (uint8_t[]) {sdir_val,}, 1), TAG, "send command failed");
    //     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io_handle, 0x36, (uint8_t[]) {h035a17->madctl_val,}, 1), TAG, "send command failed");
    // } else {
    //     // Control mirror through RGB panel
    //     ESP_RETURN_ON_ERROR(h035a17->mirror(panel, mirror_x, mirror_y), TAG, "RGB panel mirror failed");
    // }
    return ESP_OK;
}

static esp_err_t panel_h035a17_disp_on_off(esp_lcd_panel_t *panel, bool on_off) {
    // h035a17_panel_t *h035a17 = (h035a17_panel_t *)panel->user_data;
    // esp_lcd_panel_io_handle_t io_handle = h035a17->io_handle;
    // int command = 0;

    // if (h035a17->flags.display_on_off_use_cmd) {
    //     ESP_RETURN_ON_FALSE(io_handle, ESP_FAIL, TAG, "Panel IO is deleted, cannot send commands");

    //     /*  Control display on/off through LCD command  */
    //     if (on_off) {
    //         command = 0x29; // display on
    //     } else {
    //         command = 0x28; // display off
    //     }
    //     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io_handle, command, NULL, 0), TAG, "send commands failed");

    // } else {
    //     /*  Control display on/off through display control signal  */
    //     ESP_RETURN_ON_ERROR(h035a17->disp_on_off(panel, on_off), TAG, "RGB panel disp_on_off failed");
    // }
    return ESP_OK;
}

#endif
