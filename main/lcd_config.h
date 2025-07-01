#ifndef _LCD_DEFINES_H
#define _LCD_DEFINES_H

#ifdef __cplusplus
extern "C"{
#endif

#ifdef CONFIG_EXAMPLE_LCD_CONTROLLER_NV3052C
#include "esp_lcd_nv3052c.h"
#elif CONFIG_EXAMPLE_LCD_H030A10
#include "lcd_h030a10.h"
#elif CONFIG_EXAMPLE_LCD_H040A18
#include "lcd_h040a18.h"
#elif CONFIG_EXAMPLE_LCD_H035A17
#include "lcd_h035a17.h"
#endif

static const char *TAG = "example";


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if CONFIG_EXAMPLE_LCD_USE_TOUCH_ENABLED
#define TOUCH_I2C_SDA   21
#define TOUCH_I2C_SCL   40
#define TOUCH_PIN_RTN   38
#define TOUCH_PIN_INT   39
#endif

#ifdef CONFIG_EXAMPLE_LCD_CONTROLLER_NV3052C
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ     (18 * 1000 * 1000)
#define EXAMPLE_LCD_H_RES              720
#define EXAMPLE_LCD_V_RES              720
#define EXAMPLE_LCD_HSYNC              2
#define EXAMPLE_LCD_HBP                44
#define EXAMPLE_LCD_HFP                46
#define EXAMPLE_LCD_VSYNC              5
#define EXAMPLE_LCD_VBP                15
#define EXAMPLE_LCD_VFP                16
#define TOUCH_PAD_WIDTH                720
#define TOUCH_PAD_HEIGHT               720
#elif CONFIG_EXAMPLE_LCD_H030A10
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ     (19 * 1000 * 1000)
#define EXAMPLE_LCD_H_RES              480
#define EXAMPLE_LCD_V_RES              854
#define EXAMPLE_LCD_HSYNC              80
#define EXAMPLE_LCD_HBP                40
#define EXAMPLE_LCD_HFP                40
#define EXAMPLE_LCD_VSYNC              4
#define EXAMPLE_LCD_VBP                20
#define EXAMPLE_LCD_VFP                20
#elif CONFIG_EXAMPLE_LCD_H040A18
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ     (19 * 1000 * 1000)
#define EXAMPLE_LCD_H_RES              400
#define EXAMPLE_LCD_V_RES              960
#define EXAMPLE_LCD_HSYNC              8
#define EXAMPLE_LCD_HBP                50
#define EXAMPLE_LCD_HFP                50
#define EXAMPLE_LCD_VSYNC              8
#define EXAMPLE_LCD_VBP                20
#define EXAMPLE_LCD_VFP                20
#elif CONFIG_EXAMPLE_LCD_H035A17
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ     (18 * 1000 * 1000)
#define EXAMPLE_LCD_H_RES              640
#define EXAMPLE_LCD_V_RES              480
#define EXAMPLE_LCD_HSYNC              2
#define EXAMPLE_LCD_HBP                20
#define EXAMPLE_LCD_HFP                20
#define EXAMPLE_LCD_VSYNC              2
#define EXAMPLE_LCD_VBP                6
#define EXAMPLE_LCD_VFP                20
#define TOUCH_PAD_WIDTH                640
#define TOUCH_PAD_HEIGHT               480
#endif

#define EXAMPLE_LCD_BACKLIGHT_ON        1
#define EXAMPLE_LCD_BACKLIGHT_OFF       0

#define EXAMPLE_PIN_BACKLIGHT          -1
#define EXAMPLE_PIN_NUM_DISP_EN        -1

#define PIN_NUM_SDA     11
#define PIN_NUM_SCL     12
#define PIN_NUM_CS      10
#define PIN_NUM_RST     9

#define EXAMPLE_PIN_NUM_HSYNC          48
#define EXAMPLE_PIN_NUM_VSYNC          45
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

#define EXAMPLE_LCD_NUM_FB             2

#define EXAMPLE_DATA_BUS_WIDTH         16
#define EXAMPLE_PIXEL_SIZE             2
#define EXAMPLE_LV_COLOR_FORMAT        LV_COLOR_FORMAT_RGB565

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your Application ///////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#ifdef __cplusplus
}
#endif

#endif