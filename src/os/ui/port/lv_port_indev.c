/*!
    \file    src/os/ui/port/lv_port_indev.c
    \brief   Touch input device driver port for LVGL
*/

#include "lv_port_indev.h"
#include "lvgl.h"
#include "bsp_ts_gt911.h"
#include "bsp_i2c_touch.h"

#define GT911_MAX_POINT         5U
#define GT911_BUF_READY         0x80U
#define GT911_PRESS_TIMEOUT     200U
#define GT911_DEBOUNCE_DIST     40

static lv_indev_drv_t indev_drv;
lv_indev_t *indev_touchpad = NULL;

static void touchpad_init(void)
{
    gt911_init();
}

static void touchpad_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    LV_UNUSED(drv);

    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;
    static lv_indev_state_t last_state = LV_INDEV_STATE_REL;
    static uint32_t last_update = 0;

    static uint8_t  cand_valid = 0;
    static lv_coord_t cand_x = 0;
    static lv_coord_t cand_y = 0;

    if (gt911.enable == 0) {
        data->point.x = last_x;
        data->point.y = last_y;
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    if (SUCCESS == gt911_readreg(GT911_PRESSED_INFO_REG, touch_buf, 1)) {
        uint8_t status = touch_buf[0];

        if (status & GT911_BUF_READY) {
            gt911.pressed_info = status;

            uint8_t cnt = status & 0x0FU;
            if (cnt > GT911_MAX_POINT) {
                cnt = GT911_MAX_POINT;
            }

            lv_indev_state_t new_state = LV_INDEV_STATE_REL;

            if (cnt > 0U) {
                if (SUCCESS == gt911_readreg(GT911_COORDINATE_REG, touch_buf, (uint8_t)(8U * cnt))) {
                    for (uint8_t i = 0; i < cnt; i++) {
                        gt911.x[i] = ((uint16_t)touch_buf[2 + i * 8] << 8) + touch_buf[1 + i * 8];
                        gt911.y[i] = ((uint16_t)touch_buf[4 + i * 8] << 8) + touch_buf[3 + i * 8];
                        gt911.s[i] = ((uint16_t)touch_buf[6 + i * 8] << 8) + touch_buf[5 + i * 8];
                    }
                    gt911.count = cnt;

                    uint16_t x = gt911.x[0];
                    uint16_t y = gt911.y[0];

                    if ((x < 480U) && (y < 272U)) {
                        gt911.pressed = 2;
                        if (last_state == LV_INDEV_STATE_PR) {
                            last_x = (lv_coord_t)x;
                            last_y = (lv_coord_t)y;
                            new_state = LV_INDEV_STATE_PR;
                        }
                        else if (cand_valid &&
                                 (LV_ABS((lv_coord_t)x - cand_x) < GT911_DEBOUNCE_DIST) &&
                                 (LV_ABS((lv_coord_t)y - cand_y) < GT911_DEBOUNCE_DIST)) {
                            last_x = (lv_coord_t)x;
                            last_y = (lv_coord_t)y;
                            new_state = LV_INDEV_STATE_PR;
                        }
                        else {
                            cand_valid = 1;
                            cand_x = (lv_coord_t)x;
                            cand_y = (lv_coord_t)y;
                        }
                    }
                } else {
                    new_state = last_state;
                }
            } else {
                cand_valid = 0;
                gt911.count = 0;
            }

            if (new_state == LV_INDEV_STATE_REL) {
                gt911.pressed = 0;
            }
            last_state = new_state;
            last_update = lv_tick_get();

            (void)gt911_writereg(GT911_PRESSED_INFO_REG, &clear_flag, 1);
        }
    }

    if ((last_state == LV_INDEV_STATE_PR) && (lv_tick_elaps(last_update) > GT911_PRESS_TIMEOUT)) {
        last_state = LV_INDEV_STATE_REL;
    }

    data->point.x = last_x;
    data->point.y = last_y;
    data->state   = last_state;
}

void lv_port_indev_init(void)
{
    touchpad_init();

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    indev_touchpad = lv_indev_drv_register(&indev_drv);
}
