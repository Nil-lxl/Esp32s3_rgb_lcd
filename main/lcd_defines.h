#ifndef _LCD_DEFINES_H
#define _LCD_DEFINES_H

#ifdef __cplusplus
extern "C"{
#endif

#ifdef CONFIG_EXAMPLE_LCD_CONTROLLER_ST7701S
#include "esp_lcd_st7701.h"
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_NV3052C
#include "esp_lcd_nv3052c.h"
#elif CONFIG_EXAMPLE_LCD_H040A18
#include "lcd_h040a18.h"
#endif

static const char *TAG = "example";


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Refresh Rate = 18000000/(1+40+20+800)/(1+10+5+480) = 42Hz
#ifdef CONFIG_EXAMPLE_LCD_CONTROLLER_ST7701S
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ     (20 * 1000 * 1000)
#define EXAMPLE_LCD_H_RES              480
#define EXAMPLE_LCD_V_RES              854
#define EXAMPLE_LCD_HSYNC              80
#define EXAMPLE_LCD_HBP                40
#define EXAMPLE_LCD_HFP                40
#define EXAMPLE_LCD_VSYNC              4
#define EXAMPLE_LCD_VBP                20
#define EXAMPLE_LCD_VFP                20
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_NV3052C
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ     (15 * 1000 * 1000)
#define EXAMPLE_LCD_H_RES              720
#define EXAMPLE_LCD_V_RES              720
#define EXAMPLE_LCD_HSYNC              2
#define EXAMPLE_LCD_HBP                44
#define EXAMPLE_LCD_HFP                46
#define EXAMPLE_LCD_VSYNC              5
#define EXAMPLE_LCD_VBP                15
#define EXAMPLE_LCD_VFP                16
#elif CONFIG_EXAMPLE_LCD_H040A18
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ     (20 * 1000 * 1000)
#define EXAMPLE_LCD_H_RES              400
#define EXAMPLE_LCD_V_RES              960
#define EXAMPLE_LCD_HSYNC              8
#define EXAMPLE_LCD_HBP                50
#define EXAMPLE_LCD_HFP                50
#define EXAMPLE_LCD_VSYNC              8
#define EXAMPLE_LCD_VBP                20
#define EXAMPLE_LCD_VFP                20
#endif

#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL  1
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL
#define EXAMPLE_PIN_NUM_BK_LIGHT       -1
#define EXAMPLE_PIN_NUM_DISP_EN        -1

#define PIN_NUM_SDA     11
#define PIN_NUM_SCL     12
#define PIN_NUM_CS      10
#define PIN_NUM_RST     9

#define EXAMPLE_PIN_NUM_HSYNC          45
#define EXAMPLE_PIN_NUM_VSYNC          48
#define EXAMPLE_PIN_NUM_DE             47
#define EXAMPLE_PIN_NUM_PCLK           46

#define EXAMPLE_PIN_NUM_DATA0          1   //B0
#define EXAMPLE_PIN_NUM_DATA1          2   //B1
#define EXAMPLE_PIN_NUM_DATA2          3   //B2
#define EXAMPLE_PIN_NUM_DATA3          4   //B3    
#define EXAMPLE_PIN_NUM_DATA4          5   //B4

#define EXAMPLE_PIN_NUM_DATA5          6    //G0
#define EXAMPLE_PIN_NUM_DATA6          7    //G1
#define EXAMPLE_PIN_NUM_DATA7          8    //G2
#define EXAMPLE_PIN_NUM_DATA8          13   //G3
#define EXAMPLE_PIN_NUM_DATA9          14   //G4
#define EXAMPLE_PIN_NUM_DATA10         15   //G5

#define EXAMPLE_PIN_NUM_DATA11         16   //R0
#define EXAMPLE_PIN_NUM_DATA12         17   //R1
#define EXAMPLE_PIN_NUM_DATA13         18   //R2
#define EXAMPLE_PIN_NUM_DATA14         19   //R3
#define EXAMPLE_PIN_NUM_DATA15         20   //R4

#if CONFIG_EXAMPLE_LCD_DATA_LINES > 16
#define EXAMPLE_PIN_NUM_DATA16         1   
#define EXAMPLE_PIN_NUM_DATA17         2   
#define EXAMPLE_PIN_NUM_DATA18         42
#define EXAMPLE_PIN_NUM_DATA19         41
#define EXAMPLE_PIN_NUM_DATA20         40
#define EXAMPLE_PIN_NUM_DATA21         39
#define EXAMPLE_PIN_NUM_DATA22         38
#define EXAMPLE_PIN_NUM_DATA23         37
#endif

#if CONFIG_EXAMPLE_USE_DOUBLE_FB
#define EXAMPLE_LCD_NUM_FB             2
#else
#define EXAMPLE_LCD_NUM_FB             1
#endif // CONFIG_EXAMPLE_USE_DOUBLE_FB

#if CONFIG_EXAMPLE_LCD_DATA_LINES_16
#define EXAMPLE_DATA_BUS_WIDTH         16
#define EXAMPLE_PIXEL_SIZE             2
#define EXAMPLE_LV_COLOR_FORMAT        LV_COLOR_FORMAT_RGB565
#elif CONFIG_EXAMPLE_LCD_DATA_LINES_24
#define EXAMPLE_DATA_BUS_WIDTH         24
#define EXAMPLE_PIXEL_SIZE             3
#define EXAMPLE_LV_COLOR_FORMAT        LV_COLOR_FORMAT_RGB888
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your Application ///////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define EXAMPLE_LVGL_DRAW_BUF_LINES    50 // number of display lines in each draw buffer
#define EXAMPLE_LVGL_TICK_PERIOD_MS    2
#define EXAMPLE_LVGL_TASK_STACK_SIZE   (5 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY     2

#ifdef __cplusplus
}
#endif

#endif