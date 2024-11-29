#ifndef LVGL_DEMO_nv6001_H
#define LVGL_DEMO_nv6001_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif
#include "../lvgl_helpers.h"

#include "sdkconfig.h"

// 定义您的SPI和GPIO配置
#define SPI_BUS TFT_SPI_HOST

#define CS_PIN CONFIG_LV_DISP_SPI_CS
#define DC_PIN CONFIG_LV_DISP_PIN_DC
#define RST_PIN CONFIG_LV_DISP_PIN_RST
#define MOSI_PIN DISP_SPI_MOSI
#define SCK_PIN DISP_SPI_CLK

#define COL CONFIG_LV_HOR_RES_MAX // x ↑
#define ROW CONFIG_LV_VER_RES_MAX// y →

#define PCLK_HZ SPI_TFT_CLOCK_SPEED_HZ
#define DISPLAY_ORIENTATION CONFIG_LV_DISPLAY_ORIENTATION

#define PARALLEL_LINES 1

typedef void(*lcd_flush_done_cb)(void* param);

/*
 The LCD needs a bunch of command/argument values to be initialized. They are stored in this struct.
*/
typedef struct {
    uint8_t cmd;
    uint8_t data[172];
    uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;

void write_line(int ypos,uint16_t color);
void display_color(uint16_t color);
void nv6001_init();
void nv6001_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif //LVGL_DEMO_nv6001_H