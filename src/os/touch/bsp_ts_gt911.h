/*!
    \file    src/os/touch/bsp_ts_gt911.h
    \brief   GT911 Capacitive Touch Controller Driver for GD32H759I-EVAL
             Official GigaDevice Port structure definition.
*/

#ifndef BSP_GT911_H
#define BSP_GT911_H

#include "gd32h7xx.h"

/* Constants for touch point and refresh rate */
#define TOUCH_POINT            (uint8_t)0x05
#define TOUCH_REFRESH          (uint8_t)15

/* GT911 register addresses */
#define GT911_PRESSED_INFO_REG 0x814EU
#define GT911_COORDINATE_REG   0x814FU
#define GT911_CFG_START_ADDR   0x8047U
#define GT911_CFG_NUM          (0x80FE - 0x8047 + 1)

/* Structure to store GT911 touch data */
typedef struct {
    uint8_t  enable;
    uint8_t  pressed;
    uint8_t  pressed_info;
    uint8_t  count;
    uint16_t x[5];
    uint16_t y[5];
    uint16_t s[5];
    char     product_id[5];
    uint16_t fw_version;
    uint16_t res_x;
    uint16_t res_y;
} gt911_struct;

extern gt911_struct gt911;
extern uint8_t touch_buf[48];
extern uint8_t clear_flag;

ErrStatus gt911_init(void);
uint32_t  gt911_read_id(void);
uint16_t  gt911_read_version(void);
uint32_t  gt911_read_resolution(void);
ErrStatus gt911_handler(void);

#endif /* BSP_GT911_H */
