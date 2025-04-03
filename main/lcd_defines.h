#ifndef _LCD_DEFINES_H
#define _LCD_DEFINES_H

#ifdef __cplusplus
extern "C"{
#endif

static const char *TAG = "example";


//SPI_RST   19
//SPI_CS    20
//SPI_SCL   21
//SPI_SDA   47
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Refresh Rate = 18000000/(1+40+20+800)/(1+10+5+480) = 42Hz
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ     (18 * 1000 * 1000)
#define EXAMPLE_LCD_H_RES              480
#define EXAMPLE_LCD_V_RES              854
#define EXAMPLE_LCD_HSYNC              10
#define EXAMPLE_LCD_HBP                10
#define EXAMPLE_LCD_HFP                43
#define EXAMPLE_LCD_VSYNC              20
#define EXAMPLE_LCD_VBP                20
#define EXAMPLE_LCD_VFP                41

#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL  1
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL
#define EXAMPLE_PIN_NUM_BK_LIGHT       -1
#define EXAMPLE_PIN_NUM_DISP_EN        -1

#define EXAMPLE_PIN_NUM_HSYNC          11
#define EXAMPLE_PIN_NUM_VSYNC          12
#define EXAMPLE_PIN_NUM_DE             13
#define EXAMPLE_PIN_NUM_PCLK           10

#define EXAMPLE_PIN_NUM_DATA0          15    //B
#define EXAMPLE_PIN_NUM_DATA1          16
#define EXAMPLE_PIN_NUM_DATA2          17
#define EXAMPLE_PIN_NUM_DATA3          18
#define EXAMPLE_PIN_NUM_DATA4          8
#define EXAMPLE_PIN_NUM_DATA5          3
#define EXAMPLE_PIN_NUM_DATA6          46
#define EXAMPLE_PIN_NUM_DATA7          9

#define EXAMPLE_PIN_NUM_DATA8          36    //G
#define EXAMPLE_PIN_NUM_DATA9          35    
#define EXAMPLE_PIN_NUM_DATA10         45
#define EXAMPLE_PIN_NUM_DATA11         48
#define EXAMPLE_PIN_NUM_DATA12         4
#define EXAMPLE_PIN_NUM_DATA13         5
#define EXAMPLE_PIN_NUM_DATA14         6
#define EXAMPLE_PIN_NUM_DATA15         7
#if CONFIG_EXAMPLE_LCD_DATA_LINES > 16
#define EXAMPLE_PIN_NUM_DATA16         1   //R
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