#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "nv6001.h"

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void nv6001_set_orientation(uint8_t orientation);

/*********************
 *      DEFINES
 *********************/
#define TAG "nv6001"

spi_device_handle_t spi;

SemaphoreHandle_t xMutex; // 创建互斥量

// lcd操作句柄
static esp_lcd_panel_io_handle_t lcd_io_handle = NULL;

// 刷新完成回调函数
static lcd_flush_done_cb s_flush_done_cb = NULL;

static const lcd_init_cmd_t lcd_init_cmds[] = {
	{0xC0, {0x5A, 0x5A}, 2},
	{0xC1, {0x5A, 0x5A}, 2},
	{0xD0, {0x10}, 1},
	{0xB1, {0xEA, 0xF0, 0x00, 0x78, 0x00, 0x0C, 0x00, 0x08, 0x00, 0x1C, 0x00, 0x1C, 0x00, 0x0C, 0x00, 0x08, 0x00, 0x1C, 0x00, 0x1C, 0x00, 0x00, 0x0C, 0x00, 0x5F, 0x00, 0x1C, 0x00, 0x1C, 0x00, 0x0C, 0x00, 0x5F, 0x00, 0x1C, 0x00, 0x1C, 0x00, 0x00, 0x20, 0x00, 0x00, 0x77, 0x00, 0x00, 0x00, 0xEF, 0x00, 0x00, 0x00, 0x10}, 51},
	{0xB2, {0x13, 0x13, 0x04, 0x04, 0x13, 0x04, 0x04, 0x13, 0x13, 0x04, 0x04, 0x04, 0x04, 0x0F, 0x00, 0x00, 0x5E, 0x00, 0xA6, 0x00, 0x5E, 0x00, 0xA9, 0x00, 0x5E, 0x00, 0x03, 0x00, 0x5E, 0x00, 0x00, 0x00, 0x5E, 0x00, 0xA6, 0x00, 0x5E, 0x00, 0xA9, 0x00, 0x5E, 0x00, 0x03, 0x00, 0x5E, 0x00, 0x02, 0x1F, 0x58, 0x75, 0x00, 0x01, 0x00, 0x00, 0x31, 0x5A, 0x1A, 0xD5, 0x3D, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x77}, 85},
	{0xB3, {0x00, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00, 0x00, 0x00, 0x14, 0x13, 0x12, 0x00, 0x00, 0x00, 0x00, 0x06, 0x05, 0x04, 0x00, 0x00, 0x00, 0x0F, 0x0E, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 32},
	{0xB4, {0x20, 0x03, 0xC0, 0x00, 0x08, 0x03, 0x03, 0x03, 0x03, 0x03, 0x00, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x14, 0x41, 0x25, 0x52, 0x36, 0x63, 0x41, 0x14, 0x52, 0x25, 0x63, 0x36, 0x14, 0x41, 0x25, 0x52, 0x36, 0x63, 0x41, 0x14, 0x52, 0x25, 0x63, 0x36, 0x00, 0x00, 0x2A, 0x15, 0x2A, 0x15, 0x15, 0x2A, 0x15, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 58},
	{0xB5, {0x00, 0x00, 0x09, 0x09, 0x01, 0x29, 0x10, 0x00, 0x01, 0x32, 0x00, 0x32, 0x00, 0x63, 0x00, 0xAD, 0x00, 0x63, 0x00, 0xAD, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x02, 0x01, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x55, 0x00, 0x0B, 0x00, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 81},
	{0xB8, {0x00, 0xE7, 0x30, 0x00, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x22, 0x22, 0x22, 0x00, 0x00, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 27},
	{0xBB, {0x01, 0xC8, 0xC8, 0xC8, 0x80, 0x80, 0x80, 0x4C, 0x59, 0x66}, 10},
	{0xBD, {0x01, 0x00, 0x0A, 0x00, 0x32, 0x00, 0x04, 0x01, 0x05, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x01, 0x00, 0x00, 0x00, 0x10, 0x03, 0x03, 0x00}, 26},
	{0xBE, {0x00, 0x00, 0x04, 0x01, 0x04, 0x01, 0x04, 0x01, 0x04, 0x01, 0x04, 0x01, 0x04, 0x01, 0x04, 0x01, 0x04, 0x01, 0x04, 0x01, 0x00, 0x00, 0x04, 0x01, 0x04, 0x01, 0x04, 0x01, 0x04, 0x01, 0x04, 0x01, 0x04, 0x01, 0x04, 0x01, 0x04, 0x01, 0x04, 0x01, 0x00, 0x00, 0x04, 0x01, 0x04, 0x01, 0x04, 0x01, 0x04, 0x01, 0x04, 0x01, 0x04, 0x01, 0x04, 0x01, 0x04, 0x01, 0x04, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x10, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 172},
	{0xC2, {0xFC}, 1},
	{0xC3, {0x00, 0x00, 0xFF, 0x07, 0x00, 0x00, 0xFF, 0x07, 0x00, 0x00, 0xFF, 0x04}, 12},
	{0xC5, {0x80, 0x05, 0x32, 0x05, 0x3F, 0x06, 0x51, 0x05, 0x08, 0x05, 0x08, 0x06, 0x21, 0x05, 0xDC, 0x04, 0xCF, 0x05, 0xF1, 0x04, 0xB0, 0x04, 0x96, 0x05, 0xC0, 0x04, 0x83, 0x04, 0x5E, 0x05, 0x8E, 0x04, 0x55, 0x04, 0x25, 0x05, 0x5B, 0x04, 0x25, 0x04, 0xEB, 0x04, 0x26, 0x04, 0xF4, 0x03, 0xAF, 0x04, 0xEF, 0x03, 0xC0, 0x03, 0x70, 0x04, 0xB4, 0x03, 0x8A, 0x03, 0x2E, 0x04, 0x76, 0x03, 0x50, 0x03, 0xE8, 0x03, 0x34, 0x03, 0x11, 0x03, 0x9D, 0x03, 0xEB, 0x02, 0xCE, 0x02, 0x4A, 0x03, 0xC5, 0x02, 0xAA, 0x02, 0x1D, 0x03, 0x9B, 0x02, 0x83, 0x02, 0xED, 0x02, 0x6C, 0x02, 0x56, 0x02, 0xBB, 0x02, 0x51, 0x02, 0x3D, 0x02, 0xA0, 0x02, 0x35, 0x02, 0x23, 0x02, 0x84, 0x02, 0x16, 0x02, 0x06, 0x02, 0x66, 0x02, 0xF4, 0x01, 0xE6, 0x01, 0x47, 0x02, 0xCD, 0x01, 0xC2, 0x01, 0x25, 0x02, 0x9C, 0x01, 0x98, 0x01, 0x03, 0x02, 0x56, 0x01, 0x64, 0x01, 0xDE, 0x01, 0x10, 0x01, 0x30, 0x01, 0xB9, 0x01, 0xCA, 0x00, 0xFC, 0x00, 0x94, 0x01, 0xA7, 0x00, 0xE2, 0x00, 0x82, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 162},
	{0xC6, {0xFD, 0x05, 0x87, 0x05, 0xC2, 0x06, 0xB6, 0x05, 0x51, 0x05, 0x79, 0x06, 0x75, 0x05, 0x1C, 0x05, 0x33, 0x06, 0x38, 0x05, 0xE8, 0x04, 0xED, 0x05, 0xFC, 0x04, 0xB2, 0x04, 0xA6, 0x05, 0xC1, 0x04, 0x7D, 0x04, 0x61, 0x05, 0x84, 0x04, 0x46, 0x04, 0x1D, 0x05, 0x47, 0x04, 0x0F, 0x04, 0xD7, 0x04, 0x0A, 0x04, 0xD6, 0x03, 0x91, 0x04, 0xC9, 0x03, 0x9A, 0x03, 0x48, 0x04, 0x85, 0x03, 0x5B, 0x03, 0xFB, 0x03, 0x3E, 0x03, 0x18, 0x03, 0xAA, 0x03, 0xF1, 0x02, 0xD1, 0x02, 0x52, 0x03, 0xCA, 0x02, 0xAC, 0x02, 0x22, 0x03, 0x9E, 0x02, 0x83, 0x02, 0xEF, 0x02, 0x6E, 0x02, 0x56, 0x02, 0xBA, 0x02, 0x54, 0x02, 0x3D, 0x02, 0x9E, 0x02, 0x37, 0x02, 0x22, 0x02, 0x80, 0x02, 0x19, 0x02, 0x06, 0x02, 0x5F, 0x02, 0xF8, 0x01, 0xE7, 0x01, 0x3D, 0x02, 0xD2, 0x01, 0xC3, 0x01, 0x18, 0x02, 0xA6, 0x01, 0x9C, 0x01, 0xF1, 0x01, 0x6F, 0x01, 0x6A, 0x01, 0xC5, 0x01, 0x38, 0x01, 0x38, 0x01, 0x99, 0x01, 0x01, 0x01, 0x06, 0x01, 0x6D, 0x01, 0xE6, 0x00, 0xED, 0x00, 0x57, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 162},
	{0xD4, {0x00, 0x78, 0x00, 0xF0, 0x00, 0x08, 0x08, 0x14, 0x14, 0x00, 0x00}, 11},
	{0xD5, {0x01, 0x00, 0x37, 0x00, 0x01, 0x00, 0x04, 0x00, 0xBF, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0xBF, 0x00, 0x04, 0x00, 0x01, 0x00, 0x37, 0x00, 0x01, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x05, 0x00, 0xEC, 0x00, 0x05, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00}, 54},
	{0xD6, {0x01, 0x00, 0x37, 0x00, 0x01, 0x00, 0x04, 0x00, 0xBF, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0xBF, 0x00, 0x04, 0x00, 0x01, 0x00, 0x37, 0x00, 0x01, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x05, 0x00, 0xEC, 0x00, 0x05, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00}, 54},
	{0xD7, {0x01, 0x00, 0x37, 0x00, 0x01, 0x00, 0x04, 0x00, 0xBF, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0xBF, 0x00, 0x04, 0x00, 0x01, 0x00, 0x37, 0x00, 0x01, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x05, 0x00, 0xEC, 0x00, 0x05, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00}, 54},
	{0xD8, {0x01, 0x00, 0x37, 0x00, 0x01, 0x00, 0x04, 0x00, 0xBF, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0xBF, 0x00, 0x04, 0x00, 0x01, 0x00, 0x37, 0x00, 0x01, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x05, 0x00, 0xEC, 0x00, 0x05, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00}, 54},
	{0xDE, {0x00, 0x00, 0x00}, 3},
	{0xDF, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x06, 0x55, 0x50, 0xFF, 0xFF, 0x08, 0x08, 0x08}, 25},
	{0xEE, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 44},
	{0xF0, {0x75, 0x2A, 0x70, 0x70, 0x70, 0x58, 0x2F, 0x1C, 0x1C, 0x8A, 0x8A, 0x10, 0x10, 0x75, 0x2A, 0x70, 0x70, 0x70, 0x58, 0x25, 0x1C, 0x1C, 0x8A, 0x8A, 0x10, 0x10}, 26},
	{0xF1, {0x11, 0x6E, 0x49, 0xD0, 0x95, 0x15, 0xA5, 0x55, 0x0E, 0x10, 0xC6}, 11},
	{0xF2, {0x75, 0x2A, 0x08, 0x70, 0x70, 0x58, 0x2F, 0x03, 0x1A, 0x8A, 0x8A, 0x10, 0x0B}, 13},
	{0xF3, {0x11, 0x60, 0x41, 0x10, 0x95, 0x15, 0xA5, 0x55, 0x0E, 0x10}, 10},
	{0xF4, {0x0B, 0x00, 0x00, 0x75, 0x2A, 0x78, 0xF0, 0xF0, 0x41, 0x33, 0x10, 0x95, 0x15, 0xA5, 0x55, 0x10, 0x0E, 0x0E}, 18},
	{0xF5, {0x3F, 0x53, 0x01, 0x11, 0x19, 0x0A, 0x29, 0x01, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x0F, 0x11, 0x00, 0x01, 0x01, 0x3F, 0x53, 0x31, 0x11, 0x19, 0x0A, 0x29, 0x01, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x0F, 0x11, 0x01, 0x10, 0x10}, 38},
	{0xF6, {0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x02, 0x02, 0x02, 0x03, 0x04, 0x04, 0x07, 0x07, 0x08, 0x08, 0x08, 0x02, 0x02, 0x04, 0x04, 0x0B, 0x0B, 0x0B, 0x0B, 0x4B, 0x4B, 0x0B, 0x0C, 0x0C, 0x0D, 0x0E, 0x10, 0x03, 0x04, 0x42, 0x02, 0x11, 0x00}, 39},
	{0xF7, {0x55, 0x55, 0x55, 0x55, 0x44, 0x34, 0x23, 0x22, 0x41, 0x34, 0x13, 0x11, 0x11, 0x11, 0x00, 0x00, 0x05, 0x00, 0x00, 0x02, 0x00, 0x04, 0x01, 0x02}, 24},
	{0xF8, {0x55, 0x55, 0x55, 0x55, 0x44, 0x34, 0x23, 0x22, 0x41, 0x34, 0x13, 0x11, 0x11, 0x11, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 24},
	{0xFE, {0x00, 0x01, 0x02, 0x03}, 4},
	{0x8E, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 20},
	{0x8F, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 33},
	{0x90, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 6},
	{0x91, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 20},
	{0x92, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 20},
	{0x93, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 20},
	{0x94, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 20},
	{0x95, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 17},
	{0x96, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 17},
	{0x97, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 17},
	{0x98, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 17},
	{0x99, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 17},
	{0x9A, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 17},
	{0x9C, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 96},
	{0x9D, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 128},
	{0xB6, {0x00, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x00, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F}, 14},
	{0xB9, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 97},
	{0xBC, {0x00, 0x01, 0x23, 0x45, 0x67, 0x01, 0x23, 0x45, 0x67, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 16},
	{0xBF, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 30},
	{0xC8, {0x00, 0x00, 0x00}, 3},
	{0xCF, {0x01}, 1},
	{0xDD, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8},
	{0xE5, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 6},
	{0xE6, {0x8C, 0x00, 0x00}, 3},
	{0xE8, {0x00}, 1},
	{0xEF, {0x00, 0x11, 0x32, 0xFF, 0x01, 0x32, 0xFF, 0x01}, 8},
	{0xFB, {0x10, 0x05, 0x01, 0x00, 0x05, 0x00}, 6},
	{0xFC, {0x00, 0x00, 0x00}, 3},
	{0x11, {0}, 0x80},
	{0x2A, {0x00, 0x77, 0x00, 0xEF}, 4},
	{0x2B, {0x00, 0xEF, 0x01, 0x3F}, 4},
	{0x44, {0x00, 0xEF}, 2},
	{0x35, {0x00}, 1},
	{0x53, {0x28}, 1},
	{0x36, {0xc0}, 1},
	{0x3a, {0x55}, 1},
	{0x29, {0}, 0x80},
	{0x39, {0}, 0x80},
	{0x2c, {0}, 0},
	{0x2c, {0}, 0},
	{0, {0}, 0xff}};

// OLED软件初始化函数
void lcd_init(spi_device_handle_t spi)
{
	int cmd = 0;

	// Initialize non-SPI GPIOs
	gpio_config_t io_conf = {};
	io_conf.pin_bit_mask = ((1ULL << DC_PIN) | (1ULL << RST_PIN));
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pull_up_en = true;
	gpio_config(&io_conf);

	// Reset the display
	gpio_set_level(RST_PIN, 0);
	vTaskDelay(100 / portTICK_PERIOD_MS);
	gpio_set_level(RST_PIN, 1);

	// Send all the commands
	while (lcd_init_cmds[cmd].databytes != 0xff)
	{
		if (lcd_init_cmds[cmd].databytes & 0x80)
		{
			esp_lcd_panel_io_tx_param(lcd_io_handle, lcd_init_cmds[cmd].cmd, NULL, 0);
		}
		else
		{
			esp_lcd_panel_io_tx_param(lcd_io_handle, lcd_init_cmds[cmd].cmd, lcd_init_cmds[cmd].data, lcd_init_cmds[cmd].databytes & 0xFF);
		}
		cmd++;
	}
	nv6001_set_orientation(DISPLAY_ORIENTATION);
}

/** nv6001写入显示数据
 */
void nv6001_flush_lines(int ypos, uint8_t *data, size_t len)
{
	// define an area of frame memory where MCU can access
	if (ypos > ROW)
	{
		if (s_flush_done_cb)
			s_flush_done_cb(NULL);
		return;
	}
	// for (int y = ypos; y < ypos + PARALLEL_LINES - 1; y++)
	// {

	// vTaskSuspendAll();
	esp_lcd_panel_io_tx_param(lcd_io_handle,
							  LCD_CMD_CASET,
							  (uint8_t[]){
								  0,
								  0,
								  ((COL - 1) >> 8) & 0xFF,
								  (COL - 1) & 0xFF,
							  },
							  4);
	esp_lcd_panel_io_tx_param(lcd_io_handle,
							  LCD_CMD_RASET,
							  (uint8_t[]){
								  (ypos >> 8) & 0xFF,
								  ypos & 0xFF,
								  ((ypos + PARALLEL_LINES - 1) >> 8) & 0xFF,
								  (ypos + PARALLEL_LINES - 1) & 0xFF,
							  },
							  4);
	esp_lcd_panel_io_tx_param(lcd_io_handle, LCD_CMD_RAMWR, NULL, 0);
	esp_lcd_panel_io_tx_param(lcd_io_handle, LCD_CMD_RAMWR, NULL, 0);
	// for (size_t i = 0; i < COL * 2; i += 2)
	// {
	// 	size_t p = i;
	// 	uint8_t param[2] = {data[p], data[p + 1]};
	// 	vTaskSuspendAll();
	// 	esp_lcd_panel_io_tx_param(lcd_io_handle, -1, param, 2);
	// 	xTaskResumeAll();
	// }
	esp_lcd_panel_io_tx_param(lcd_io_handle, -1, data, len);
	// }
	// xTaskResumeAll();
	return;
}

void write_line(int ypos, uint16_t color)
{
	// 计算数组的大小
	size_t array_size = COL * PARALLEL_LINES * sizeof(uint16_t);

	// 动态分配 uint8_t 数组
	uint8_t *data = (uint8_t *)malloc(array_size);

	// 填充 data 数组
	for (size_t i = 0; i < array_size; i += 2)
	{
		data[i] = (uint8_t)(color >> 8);	   // 高位字节
		data[i + 1] = (uint8_t)(color & 0xFF); // 低位字节
	}

	nv6001_flush_lines(ypos, data, array_size);

	// 释放内存
	free(data);
}

// 填充屏幕颜色
void display_color(uint16_t color)
{
	for (size_t ypos = 0; ypos < COL * 2; ypos += PARALLEL_LINES)
	{
		write_line(ypos, color);
	}
}

static bool notify_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
	if (s_flush_done_cb)
		s_flush_done_cb(user_ctx);
	return false;
}

// OLED屏幕整体初始化(包括软硬件)函数
void nv6001_init()
{
	esp_err_t ret;

	spi_bus_config_t buscfg = {
		.mosi_io_num = MOSI_PIN,
		.sclk_io_num = SCK_PIN,
		.miso_io_num = -1,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = DISP_BUF_SIZE * sizeof(uint16_t),
		.flags = SPICOMMON_BUSFLAG_MASTER, // SPI主模式
	};

	// Initialize the SPI bus
	ret = spi_bus_initialize(SPI_BUS, &buscfg, SPI_DMA_CH_AUTO);
	ESP_ERROR_CHECK(ret);

	// 创建基于spi的lcd操作句柄
	esp_lcd_panel_io_spi_config_t io_config = {
		.dc_gpio_num = DC_PIN,					   // DC引脚
		.cs_gpio_num = CS_PIN,					   // CS引脚
		.pclk_hz = PCLK_HZ,						   // SPI时钟频率
		.lcd_cmd_bits = 8,						   // 命令长度
		.lcd_param_bits = 8,					   // 参数长度
		.spi_mode = 0,							   // 使用SPI0模式
		.trans_queue_depth = 10,				   // 表示可以缓存的spi传输事务队列深度
		.on_color_trans_done = notify_flush_ready, // 刷新完成回调函数
		.user_ctx = NULL,						   // 回调函数参数
		.flags = {
			// 以下为 SPI 时序的相关参数，需根据 LCD 驱动 IC 的数据手册以及硬件的配置确定
			.sio_mode = 0, // 通过一根数据线（MOSI）读写数据，0: Interface I 型，1: Interface II 型
		},
	};
	// Attach the LCD to the SPI bus
	ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI_BUS, &io_config, &lcd_io_handle);
	ESP_ERROR_CHECK(ret);

	// Initialize the LCD
	lcd_init(spi);

	display_color(0xFFFF);

	ESP_LOGI(TAG, "NV6001 initialization.\n");
}

void write_line_map(int ypos, size_t x1, size_t x2, uint8_t *color_map)
{
	// 尝试获取互斥量
	// xMutex = xSemaphoreCreateMutex(); // 创建互斥量
	// if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE)
	// {
	size_t array_size = COL * sizeof(uint16_t);
	uint8_t *data = (uint8_t *)malloc(array_size);
	memset(data, 0xFF, array_size);
	memset(data, 0x30, COL);
	// 进入临界区
	for (size_t x = 0; x < COL; x++)
	{
		if (x >= x1 && x <= x2)
		{
			uint16_t color = color_map[1] << 8 | color_map[0];
			// ESP_LOGI(TAG, "write_line_map color_map: %d", color);
			data[x * 2] = color >> 8;
			data[x * 2 + 1] = color;
			color_map += 2;
		}
	}
	nv6001_flush_lines(ypos, data, array_size);
	free(data);
	// 退出临界区
	// 	xSemaphoreGive(xMutex);
	// }
}

static void nv6001_set_orientation(uint8_t orientation)
{
	// ESP_ASSERT(orientation < 4);

	const char *orientation_str[] = {
		"PORTRAIT", "PORTRAIT_INVERTED", "LANDSCAPE", "LANDSCAPE_INVERTED"};

	ESP_LOGI(TAG, "Display orientation: %s", orientation_str[orientation]);

#if defined(CONFIG_LV_PREDEFINED_DISPLAY_NONE)
	uint8_t data[] = {0x00, 0x08, 0x40, 0x48};
#endif

	ESP_LOGI(TAG, "0x36 command value: 0x%02X", data[orientation]);
	esp_lcd_panel_io_tx_param(lcd_io_handle, 0x36, (void *)&data[orientation], 1);
}

uint8_t *rev_color_map(uint8_t *color_map, size_t size){
	// 高低取反
	uint8_t *data = (uint8_t *)malloc(size * 2);
	
	for (size_t x = 0; x < size * 2; x+=2)
	{
			data[x] = color_map[x + 1];
			data[x + 1] = color_map[x];
	}
	return data;
}

void nv6001_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
	uint8_t data[4];

	/*Column addresses*/
	data[0] = (area->x1 >> 8) & 0xFF;
	data[1] = area->x1 & 0xFF;
	data[2] = (area->x2 >> 8) & 0xFF;
	data[3] = area->x2 & 0xFF;
	esp_lcd_panel_io_tx_param(lcd_io_handle, LCD_CMD_CASET, (void *)data, 4);

	/*Page addresses*/
	data[0] = (area->y1 >> 8) & 0xFF;
	data[1] = area->y1 & 0xFF;
	data[2] = (area->y2 >> 8) & 0xFF;
	data[3] = area->y2 & 0xFF;
	esp_lcd_panel_io_tx_param(lcd_io_handle, LCD_CMD_RASET, (void *)data, 4);

	/*Memory write*/
	esp_lcd_panel_io_tx_param(lcd_io_handle, LCD_CMD_RAMWR, NULL, 0);
	esp_lcd_panel_io_tx_param(lcd_io_handle, LCD_CMD_RAMWR, NULL, 0);

	uint32_t size = lv_area_get_width(area) * lv_area_get_height(area);
	uint8_t *color_rev = rev_color_map(color_map,size);
	esp_lcd_panel_io_tx_param(lcd_io_handle, -1, (void *)color_rev , size * 2);
	lv_disp_flush_ready(drv);
}

// 刷新显示缓冲区的回调函数
void nv6001_flush_1(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
	ESP_LOGI(TAG, "nv6001_lv_fb_flush");
	ESP_LOGI(TAG, "Display y: %d, %d", area->y1, area->y2);
	ESP_LOGI(TAG, "Display x: %d, %d", area->x1, area->x2);
	int x1 = area->x1;
	int x2 = area->x2;
	int y1 = area->y1;
	int y2 = area->y2;

	if (y2 > y1 && x2 > x1)
	{
		for (int ypos = y1; ypos <= y2; ypos += 1)
			write_line_map(ypos, x1, x2, color_map);
	}

	lv_disp_flush_ready(drv);
}
