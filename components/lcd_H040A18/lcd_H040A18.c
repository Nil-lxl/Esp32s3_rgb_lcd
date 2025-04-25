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

#include "lcd_h040a18.h"

typedef struct {
    esp_lcd_panel_io_handle_t io_handle;
    int reset_gpio_num;
    uint8_t madctl_val;
    uint8_t colmod_val;
    const h040a18_lcd_init_cmd_t *init_cmds;
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

} h040a18_panel_t;

static const char *TAG = "h040a18_rgb";

static esp_err_t panel_h040a18_send_init_cmds(h040a18_panel_t *h040a18);
static esp_err_t panel_h040a18_init(esp_lcd_panel_t *panel);
static esp_err_t panel_h040a18_del(esp_lcd_panel_t *panel);
static esp_err_t panel_h040a18_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_h040a18_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_h040a18_disp_on_off(esp_lcd_panel_t *panel, bool off);

esp_err_t esp_lcd_new_panel_h040a18(const esp_lcd_panel_io_handle_t io_handle, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel) {
    ESP_RETURN_ON_FALSE(io_handle && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");
    h040a18_vendor_config_t *vendor_config = (h040a18_vendor_config_t *)panel_dev_config->vendor_config;
    ESP_RETURN_ON_FALSE(vendor_config && vendor_config->rgb_config, ESP_ERR_INVALID_ARG, TAG, "`vendor_config` and `rgb_config` are necessary");
    ESP_RETURN_ON_FALSE(!vendor_config->flags.enable_io_multiplex || !vendor_config->flags.mirror_by_cmd, ESP_ERR_INVALID_ARG, TAG, "`mirror_by_cmd` and `enable_io_multiplex` can't work together");

    esp_err_t ret = ESP_OK;
    h040a18_panel_t *h040a18 = (h040a18_panel_t *)calloc(1, sizeof(h040a18_panel_t));
    ESP_RETURN_ON_FALSE(h040a18, ESP_ERR_NO_MEM, TAG, "no mem for h040a18 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_config = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_config), err, TAG, "configure GPIO for RST sign failed");
    }

    h040a18->io_handle = io_handle;
    h040a18->init_cmds = vendor_config->init_cmds;
    h040a18->init_cmds_size = vendor_config->init_cmds_size;
    h040a18->reset_gpio_num = panel_dev_config->reset_gpio_num;
    h040a18->flags.mirror_by_cmd = vendor_config->flags.mirror_by_cmd;
    h040a18->flags.display_on_off_use_cmd = (vendor_config->rgb_config->disp_gpio_num >= 0) ? 0 : 1;
    h040a18->flags.enable_io_multiplex = vendor_config->flags.enable_io_multiplex;
    h040a18->flags.reset_level = panel_dev_config->flags.reset_active_high;

    if (h040a18->flags.enable_io_multiplex) {
        if (h040a18->reset_gpio_num >= 0) {
            gpio_set_level(h040a18->reset_gpio_num, h040a18->flags.reset_level);
            vTaskDelay(pdMS_TO_TICKS(1));
            gpio_set_level(h040a18->reset_gpio_num, !h040a18->flags.reset_level);
            vTaskDelay(pdMS_TO_TICKS(10));
            gpio_set_level(h040a18->reset_gpio_num, !h040a18->flags.reset_level);
        } else { /* Rst gpio is not used */
            ESP_GOTO_ON_ERROR(esp_lcd_panel_io_tx_param(io_handle, 0x01, NULL, 0), err, TAG, "send soft reset command failed");
        }
        vTaskDelay(pdMS_TO_TICKS(120));
        ESP_GOTO_ON_ERROR(panel_h040a18_send_init_cmds(h040a18), err, TAG, "send init cmds failed");
    }
    /*  Create RGB panel   */
    ESP_GOTO_ON_ERROR(esp_lcd_new_rgb_panel(vendor_config->rgb_config, ret_panel), err, TAG, "create new RGB panel failed");
    ESP_LOGI(TAG, "new RGB panel h040a18");

    /*  Save the original functions of RGB panel  */
    h040a18->init = (*ret_panel)->init;
    h040a18->del = (*ret_panel)->del;
    h040a18->reset = (*ret_panel)->reset;
    h040a18->mirror = (*ret_panel)->mirror;
    h040a18->disp_on_off = (*ret_panel)->disp_on_off;

    /*  Overwrite the functions of RGB panel   */
    (*ret_panel)->init = panel_h040a18_init;
    (*ret_panel)->del = panel_h040a18_del;
    (*ret_panel)->reset = panel_h040a18_reset;
    (*ret_panel)->mirror = panel_h040a18_mirror;
    (*ret_panel)->disp_on_off = panel_h040a18_disp_on_off;
    (*ret_panel)->user_data = h040a18;
    ESP_LOGI(TAG, "new lcd panel h040a18");

    return ESP_OK;
err:
    if (h040a18) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(h040a18);
    }
    return ret;
}

static const h040a18_lcd_init_cmd_t rgb_lcd_init_cmds [] = {
    {0x3A, (uint8_t []){0x77}, 1, 0},
    {0x36, (uint8_t []){0x00}, 1, 0},
    // {0x36, (uint8_t []){0x10}, 1, 0}, //vertical direction reverse
    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xEF, (uint8_t []){0x08}, 1, 0},
    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, (uint8_t []){0x77, 0x00}, 2, 0},
    {0xC1, (uint8_t []){0x0E, 0x0C}, 2, 0},
    {0xC2, (uint8_t []){0x07, 0x02}, 2, 0},
    {0xCC, (uint8_t []){0x30}, 1, 0},
    {0xB0, (uint8_t []){0x00, 0x13, 0x1E, 0x0D, 0x11, 0x06, 0x0F, 0x07, 0x0F, 0x2C, 0x05, 0x17, 0x1E, 0x2D, 0x34, 0x1D}, 16, 0},
    {0xB1, (uint8_t []){0x00, 0x1A, 0x1F, 0x0F, 0x12, 0x08, 0x0B, 0x0A, 0x03, 0x22, 0x03, 0x0F, 0x09, 0x28, 0x33, 0x1F}, 16, 0},
    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (uint8_t []){0x5C}, 1, 0},
    {0xB1, (uint8_t []){0x69}, 1, 0},
    {0xB2, (uint8_t []){0x87}, 1, 0},
    {0xB3, (uint8_t []){0x80}, 1, 0},
    {0xB5, (uint8_t []){0x4A}, 1, 0},
    {0xB7, (uint8_t []){0x85}, 1, 0},
    {0xB8, (uint8_t []){0x48}, 1, 0},
    {0xB9, (uint8_t []){0x10, 0x1F}, 2, 0},
    {0xBB, (uint8_t []){0x03}, 1, 0},
    {0xC0, (uint8_t []){0x80}, 1, 0},
    {0xC1, (uint8_t []){0x08}, 1, 0},
    {0xC2, (uint8_t []){0x08}, 1, 0},
    // {0xC3, (uint8_t []){0x80, 0x10, 0x18}, 1, 0}, // RGBCTRL
    {0xD0, (uint8_t []){0x88}, 1, 0},
    {0xE0, (uint8_t []){0x00, 0x00, 0x02, 0x00, 0x00, 0x0C}, 6, 0},
    {0xE1, (uint8_t []){0x03, 0x96, 0x05, 0x96, 0x02, 0x96, 0x04, 0x96, 0x00, 0x44, 0x44}, 11, 0},
    {0xE2, (uint8_t []){0x00, 0x00, 0x03, 0x03, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00}, 12, 0},
    {0xE3, (uint8_t []){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE4, (uint8_t []){0x44, 0x44}, 2, 0},
    {0xE5, (uint8_t []){0x0B, 0xD4, 0x28, 0x8C, 0x0D, 0xD6, 0x28, 0x8C, 0x07, 0xD0, 0x28, 0x8C, 0x09, 0xD2, 0x28, 0x8C}, 16, 0},
    {0xE6, (uint8_t []){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE7, (uint8_t []){0x44, 0x44}, 2, 0},
    {0xE8, (uint8_t []){0x0A, 0xD5, 0x28, 0x8C, 0x0C, 0xD7, 0x28, 0x8C, 0x06, 0xD1, 0x28, 0x8C, 0x08, 0xD3, 0x28, 0x8C}, 16, 0},
    {0xEB, (uint8_t []){0x00, 0x01, 0xE4, 0xE4, 0x44, 0x00}, 6, 0},
    {0xED, (uint8_t []){0xFF, 0x45, 0x67, 0xFC, 0x01, 0x3F, 0xAB, 0xFF, 0xFF, 0xBA, 0xF3, 0x10, 0xCF, 0x76, 0x54, 0xFF}, 16, 0},
    {0xEF, (uint8_t []){0x10, 0x0D, 0x04, 0x08, 0x3F, 0x1F}, 6, 0},
    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xE8, (uint8_t []){0x00, 0x0E}, 2, 0},

    {0x11, (uint8_t []){0x00}, 0, 120},

    {0xE8, (uint8_t []){0x00, 0x0C}, 2, 20},
    {0xE8, (uint8_t []){0x40, 0x00}, 2, 0},
    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},
    {0x35, (uint8_t []){0x00}, 1, 0},

    {0x29, (uint8_t []){0x00}, 0, 20},
};

static esp_err_t panel_h040a18_send_init_cmds(h040a18_panel_t *h040a18) {
    esp_lcd_panel_io_handle_t io_handle = h040a18->io_handle;
    const h040a18_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;

    if (h040a18->init_cmds) {
        init_cmds = h040a18->init_cmds;
        init_cmds_size = h040a18->init_cmds_size;
    } else {
        init_cmds = rgb_lcd_init_cmds;
        init_cmds_size = sizeof(rgb_lcd_init_cmds) / sizeof(h040a18_lcd_init_cmd_t);
    }

    for (int i = 0; i < init_cmds_size; i++) {
        /*  Send Initialization Commands   */
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io_handle, init_cmds [i].cmd, init_cmds [i].data, init_cmds [i].data_bytes), TAG, "send Initialization commands failed");
        vTaskDelay(pdMS_TO_TICKS(init_cmds [i].delay_ms));

        ESP_LOGW(TAG, "send commands %d/%d", i, init_cmds_size);
    }
    ESP_LOGI(TAG, "send initialization cmds success");

    return ESP_OK;
}

static esp_err_t panel_h040a18_init(esp_lcd_panel_t *panel) {
    h040a18_panel_t *h040a18 = (h040a18_panel_t *)panel->user_data;
    if (!h040a18->flags.enable_io_multiplex) {
        ESP_RETURN_ON_ERROR(panel_h040a18_send_init_cmds(h040a18), TAG, "send init cmds failed");
    }
    /*  Init RGB panel  */
    ESP_RETURN_ON_ERROR(h040a18->init(panel), TAG, "init RGB panel failed");

    return ESP_OK;
}
static esp_err_t panel_h040a18_del(esp_lcd_panel_t *panel) {
    h040a18_panel_t *h040a18 = (h040a18_panel_t *)panel->user_data;

    if (h040a18->reset_gpio_num >= 0) {
        gpio_reset_pin(h040a18->reset_gpio_num);
    }
    /*  Delete RGB panel  */
    h040a18->del(panel);
    free(h040a18);
    ESP_LOGD(TAG, "deleted h040a18 panel");
    return ESP_OK;
}
static esp_err_t panel_h040a18_reset(esp_lcd_panel_t *panel) {
    h040a18_panel_t *h040a18 = (h040a18_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io_handle = h040a18->io_handle;

    /*  Perform hardware reset  */
    if (h040a18->reset_gpio_num >= 0) {
        gpio_set_level(h040a18->reset_gpio_num, h040a18->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(h040a18->reset_gpio_num, !h040a18->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(120));
        // gpio_set_level(h040a18->reset_gpio_num, h040a18->flags.reset_level);
        // vTaskDelay(pdMS_TO_TICKS(120));
    } else if (io_handle) {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io_handle, 0x01, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(120));
    }
    ESP_RETURN_ON_ERROR(h040a18->reset(panel), TAG, "reset RGB panel failed");

    return ESP_OK;
}

static esp_err_t panel_h040a18_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y) {
    // h040a18_panel_t *h040a18 = (h040a18_panel_t *)panel->user_data;
    // esp_lcd_panel_io_handle_t io_handle = h040a18->io_handle;
    // uint8_t sdir_val = 0;

    // if (h040a18->flags.mirror_by_cmd) {
    //     ESP_RETURN_ON_FALSE(io_handle, ESP_FAIL, TAG, "Panel IO is deleted, cannot send command");
    //     // Control mirror through LCD command
    //     if (mirror_x) {
    //         sdir_val = 1<<2;
    //     } else {
    //         sdir_val = 0;
    //     }
    //     if (mirror_y) {
    //         h040a18->madctl_val |= LCD_CMD_ML_BIT;
    //     } else {
    //         h040a18->madctl_val &= ~LCD_CMD_ML_BIT;
    //     }
    //     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io_handle, 0xc7, (uint8_t[]) {sdir_val,}, 1), TAG, "send command failed");
    //     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io_handle, 0x36, (uint8_t[]) {h040a18->madctl_val,}, 1), TAG, "send command failed");
    // } else {
    //     // Control mirror through RGB panel
    //     ESP_RETURN_ON_ERROR(h040a18->mirror(panel, mirror_x, mirror_y), TAG, "RGB panel mirror failed");
    // }
    return ESP_OK;
}

static esp_err_t panel_h040a18_disp_on_off(esp_lcd_panel_t *panel, bool on_off) {
    // h040a18_panel_t *h040a18 = (h040a18_panel_t *)panel->user_data;
    // esp_lcd_panel_io_handle_t io_handle = h040a18->io_handle;
    // int command = 0;

    // if (h040a18->flags.display_on_off_use_cmd) {
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
    //     ESP_RETURN_ON_ERROR(h040a18->disp_on_off(panel, on_off), TAG, "RGB panel disp_on_off failed");
    // }
    return ESP_OK;
}

#endif
