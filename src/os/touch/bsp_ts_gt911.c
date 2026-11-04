/*!
    \file    src/os/touch/bsp_ts_gt911.c
    \brief   GT911 5-Point Capacitive Touch Controller Driver for GD32H759I-EVAL
             Official GigaDevice Port.
*/

#include "bsp_ts_gt911.h"
#include "bsp_i2c_touch.h"
#include "systick.h"
#include <string.h>

/* 480x272 GT911 configuration table for GD32H759I-EVAL */
const uint8_t gt911_cfg_params[] = {
    0x5A,0xE0,0x01,0x10,0x01,TOUCH_POINT,0x8D,0x00,0x01,0x08,0x28,0x05,
    0x55,0x32,0x03,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x89,0x2A,0x0B,0x17,0x15,0x31,0x0D,0x00,0x00,
    0x02,0xB9,0x04,0x2C,0x00,0x00,0x00,0x00,0x00,0x03,0x64,0x32,
    0x00,0x00,0x00,0x0F,0x94,0x94,0xC5,0x02,0x07,0x00,0x00,0x04,
    0x8D,0x13,0x00,0x5C,0x1E,0x00,0x3C,0x30,0x00,0x29,0x4C,0x00,
    0x20,0x78,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0E,0x10,
    0x12,0x14,0x16,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x24,0x22,
    0x21,0x20,0x1F,0x1E,0x1D,0x1C,0x18,0x16,0x12,0x10,0x0F,0x0A,
    0x08,0x06,0x04,0x02,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0x21,0x01
};

/* Structure for storing GT911 data */
gt911_struct gt911 = {0};
uint8_t clear_flag = 0;
uint8_t touch_buf[48] = {0};
static uint8_t cfg_buf[sizeof(gt911_cfg_params)] = {0};

ErrStatus gt911_init(void)
{
    ErrStatus err = SUCCESS;
    uint8_t checksum = 0;
    uint32_t id;
    uint32_t resolution;
    uint8_t cfg_num = GT911_CFG_NUM;

    memset(&gt911, 0, sizeof(gt911));

    /* Initialize I2C GPIOs and reset GT911 hardware */
    i2c_touch_init();

    /* Calculate configuration checksum */
    memcpy(cfg_buf, gt911_cfg_params, cfg_num);
    for (uint8_t i = 0; i < cfg_num; i++) {
        checksum += cfg_buf[i];
    }
    cfg_buf[sizeof(gt911_cfg_params) - 2] = (~checksum) + 1;
    cfg_buf[sizeof(gt911_cfg_params) - 1] = 1;

    delay_1ms(100);

    /* Read chip identification */
    id = gt911_read_id();
    gt911.product_id[0] = (char)(id & 0xFFU);
    gt911.product_id[1] = (char)((id >> 8) & 0xFFU);
    gt911.product_id[2] = (char)((id >> 16) & 0xFFU);
    gt911.product_id[3] = (char)((id >> 24) & 0xFFU);
    gt911.product_id[4] = '\0';
    gt911.fw_version = gt911_read_version();
    resolution = gt911_read_resolution();
    gt911.res_x = (uint16_t)(resolution & 0xFFFFU);
    gt911.res_y = (uint16_t)((resolution >> 16) & 0xFFFFU);

    /* Write configuration to GT911 */
    if (ERROR != gt911_writereg(GT911_CFG_START_ADDR, cfg_buf, sizeof(gt911_cfg_params))) {
        delay_1ms(10);
        gt911.enable = 1;
        err = SUCCESS;
    } else {
        gt911.enable = 0;
        err = ERROR;
    }

    return err;
}

uint32_t gt911_read_id(void)
{
    uint8_t id_buf[4];
    if (SUCCESS != gt911_readreg(0x8140, id_buf, 4)) {
        return 0;
    }
    return ((uint32_t)id_buf[3] << 24) |
           ((uint32_t)id_buf[2] << 16) |
           ((uint32_t)id_buf[1] << 8)  |
           (uint32_t)id_buf[0];
}

uint16_t gt911_read_version(void)
{
    uint8_t tmp_buf[2];
    if (SUCCESS != gt911_readreg(0x8144, tmp_buf, 2)) {
        return 0;
    }
    return ((uint16_t)tmp_buf[1] << 8) | tmp_buf[0];
}

uint32_t gt911_read_resolution(void)
{
    uint8_t tmp_buf[4];
    if (SUCCESS != gt911_readreg(0x8146, tmp_buf, 4)) {
        return 0;
    }
    return (((uint32_t)tmp_buf[3] << 24) | ((uint32_t)tmp_buf[2] << 16) | ((uint32_t)tmp_buf[1] << 8) | ((uint32_t)tmp_buf[0]));
}

/* Polling handler retained for OS-Debug canvas */
ErrStatus gt911_handler(void)
{
    if (gt911.enable == 0) {
        return ERROR;
    }

    if (gt911_readreg(GT911_PRESSED_INFO_REG, touch_buf, 1) != SUCCESS) {
        return ERROR;
    }

    /* Bit 7 indicates if buffer data is ready */
    if ((touch_buf[0] & 0x80U) == 0U) {
        return SUCCESS;
    }

    gt911.pressed_info = touch_buf[0];
    uint8_t pts = gt911.pressed_info & 0x0FU;
    if (pts > 5U) {
        pts = 5U;
    }

    if (pts > 0U) {
        if (gt911_readreg(GT911_COORDINATE_REG, touch_buf, (uint8_t)(8U * pts)) == SUCCESS) {
            for (uint8_t i = 0; i < pts; i++) {
                uint16_t rx = ((uint16_t)touch_buf[2 + i * 8] << 8) + touch_buf[1 + i * 8];
                uint16_t ry = ((uint16_t)touch_buf[4 + i * 8] << 8) + touch_buf[3 + i * 8];
                if (rx >= 480U) rx = 479U;
                if (ry >= 272U) ry = 271U;
                gt911.x[i] = rx;
                gt911.y[i] = ry;
                gt911.s[i] = ((uint16_t)touch_buf[6 + i * 8] << 8) + touch_buf[5 + i * 8];
            }
            gt911.count = pts;
            gt911.pressed = 1;
        }
    } else {
        gt911.count = 0;
        gt911.pressed = 0;
    }

    clear_flag = 0;
    gt911_writereg(GT911_PRESSED_INFO_REG, &clear_flag, 1);

    return SUCCESS;
}
