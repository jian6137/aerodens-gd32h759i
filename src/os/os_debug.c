/*!
    \file    src/os/os_debug.c
    \brief   OS diagnostic mode and touch hardware validation interface
*/

#include "os_debug.h"
#include "gd32h7xx.h"
#include "gd32h759i_eval.h"
#include "gd32h759i_lcd.h"
#include "lcd_console.h"
#include "systick.h"
#include "touch/bsp_ts_gt911.h"
#include "sdcard.h"
#include "ff.h"
#include <rtthread.h>
#include <rthw.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define UI_BG_DARK          0x0842U
#define UI_PANEL_BG         0x10A4U
#define UI_PANEL_HEADER     0x0012U
#define UI_CANVAS_BG        0x0821U
#define UI_CANVAS_GRID      0x18E3U
#define UI_BORDER_COLOR     0x39E7U
#define UI_BTN_BG           0x1949U
#define UI_BTN_ACTIVE       0x03E0U

#define CANVAS_X0           238U
#define CANVAS_Y0           50U
#define CANVAS_X1           473U
#define CANVAS_Y1           230U
#define CANVAS_W            (CANVAS_X1 - CANVAS_X0 + 1U)
#define CANVAS_H            (CANVAS_Y1 - CANVAS_Y0 + 1U)

#define BTN_Y0              236U
#define BTN_Y1              268U
#define BTN_H               (BTN_Y1 - BTN_Y0 + 1U)

typedef struct {
    uint16_t x0;
    uint16_t x1;
} touch_btn_t;

static const touch_btn_t k_buttons[4] = {
    {   4U, 118U },
    { 123U, 237U },
    { 242U, 356U },
    { 361U, 475U },
};

static const uint16_t k_brush_colors[6] = {
    CONSOLE_COLOR_CYAN,
    CONSOLE_COLOR_YELLOW,
    CONSOLE_COLOR_GREEN,
    CONSOLE_COLOR_RED,
    CONSOLE_COLOR_MAGENTA,
    CONSOLE_COLOR_WHITE
};

static const char *const k_brush_names[6] = {
    "CYN", "YEL", "GRN", "RED", "MAG", "WHT"
};

static const uint16_t k_led_periods[3] = { 250U, 80U, 500U };

static rt_mutex_t g_lcd_mutex   = RT_NULL;
static rt_mb_t    g_telem_mb    = RT_NULL;
static rt_sem_t   g_burst_sem   = RT_NULL;
static rt_timer_t g_uptime_tmr  = RT_NULL;

static volatile uint32_t g_uptime_sec       = 0U;
static volatile uint32_t g_ctx_per_sec      = 0U;
static volatile uint32_t g_last_ctx_count   = 0U;
static volatile uint32_t g_ipc_msg_count    = 0U;
static volatile uint32_t g_burst_count      = 0U;
static volatile uint32_t g_touch_cnt        = 0U;
static volatile uint32_t g_sensor_cnt       = 0U;
static volatile uint32_t g_worker_cnt       = 0U;
static volatile uint32_t g_ui_cnt           = 0U;
static volatile uint32_t g_led_cnt          = 0U;
static volatile uint32_t g_idle_hooks       = 0U;

static volatile int32_t  g_fpu_pitch_deg10  = 0;
static volatile int32_t  g_fpu_alt_m        = 100;

static volatile uint8_t  g_brush_idx        = 0U;
static volatile uint8_t  g_led_speed_idx    = 0U;
static volatile uint8_t  g_gt911_ok         = 0U;
static volatile uint32_t g_gt911_id         = 0U;
static volatile uint16_t g_last_touch_x     = 0U;
static volatile uint16_t g_last_touch_y     = 0U;
static volatile uint8_t  g_last_touch_pts   = 0U;
static volatile uint8_t  g_canvas_has_ink   = 0U;

static uint16_t *const s_fb = (uint16_t *)DISPLAY_FRAMEBUFFER;

static void draw_rect_outline(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    gd32h759i_lcd_fill_rect(x, y, w, 1U, color);
    gd32h759i_lcd_fill_rect(x, (uint16_t)(y + h - 1U), w, 1U, color);
    gd32h759i_lcd_fill_rect(x, y, 1U, h, color);
    gd32h759i_lcd_fill_rect((uint16_t)(x + w - 1U), y, 1U, h, color);
}

static void canvas_draw_brush(int32_t px, int32_t py, uint16_t color)
{
    for (int32_t dy = -1; dy <= 1; dy++) {
        for (int32_t dx = -1; dx <= 1; dx++) {
            int32_t x = px + dx;
            int32_t y = py + dy;
            if (x >= (int32_t)CANVAS_X0 && x <= (int32_t)CANVAS_X1 &&
                y >= (int32_t)CANVAS_Y0 && y <= (int32_t)CANVAS_Y1) {
                s_fb[(uint32_t)y * LCD_WIDTH + (uint32_t)x] = color;
            }
        }
    }
}

static void canvas_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    int32_t cx = (int32_t)x0;
    int32_t cy = (int32_t)y0;
    int32_t dx = abs((int32_t)x1 - (int32_t)x0);
    int32_t dy = -abs((int32_t)y1 - (int32_t)y0);
    int32_t sx = (x0 < x1) ? 1 : -1;
    int32_t sy = (y0 < y1) ? 1 : -1;
    int32_t err = dx + dy;

    while (1) {
        canvas_draw_brush(cx, cy, color);
        if (cx == (int32_t)x1 && cy == (int32_t)y1) {
            break;
        }
        int32_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            cx += sx;
        }
        if (e2 <= dx) {
            err += dx;
            cy += sy;
        }
    }
}

static void ui_clear_canvas(void)
{
    gd32h759i_lcd_fill_rect(CANVAS_X0, CANVAS_Y0, CANVAS_W, CANVAS_H, UI_CANVAS_BG);

    uint16_t mid_x = (CANVAS_X0 + CANVAS_X1) / 2U;
    uint16_t mid_y = (CANVAS_Y0 + CANVAS_Y1) / 2U;
    gd32h759i_lcd_fill_rect(CANVAS_X0 + 8U, mid_y, CANVAS_W - 16U, 1U, UI_CANVAS_GRID);
    gd32h759i_lcd_fill_rect(mid_x, CANVAS_Y0 + 8U, 1U, CANVAS_H - 16U, UI_CANVAS_GRID);

    lcd_draw_string_xy(280U, 124U, "TOUCH & DRAW HERE", CONSOLE_COLOR_GRAY, UI_CANVAS_BG);
    lcd_draw_string_xy(272U, 142U, "(5-Point MultiTouch)", CONSOLE_COLOR_GRAY, UI_CANVAS_BG);
    g_canvas_has_ink = 0U;
}

void os_debug_run_sd_test(void)
{
    g_canvas_has_ink = 1U;

    /* Clear canvas area */
    gd32h759i_lcd_fill_rect(CANVAS_X0, CANVAS_Y0, CANVAS_W, CANVAS_H, UI_CANVAS_BG);
    draw_rect_outline(CANVAS_X0, CANVAS_Y0, CANVAS_W, CANVAS_H, CONSOLE_COLOR_CYAN);

    uint16_t ty = CANVAS_Y0 + 4U;
    char buf[44];

    lcd_draw_string_xy(CANVAS_X0 + 8U, ty, "=== SD CARD DIAGNOSTICS ===", CONSOLE_COLOR_YELLOW, UI_CANVAS_BG);
    ty += 16U;

    rt_kprintf("\r\n================ [SD CARD DIAGNOSTICS] ================\r\n");

    /* 1. Pin Voltage / Pull-up Check */
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);

    uint8_t pb13_val = gpio_input_bit_get(GPIOB, GPIO_PIN_13); /* SDIO0_D0 (JP65) */
    uint8_t pd2_val  = gpio_input_bit_get(GPIOD, GPIO_PIN_2);  /* SDIO0_CMD */
    uint8_t pc12_val = gpio_input_bit_get(GPIOC, GPIO_PIN_12); /* SDIO0_CLK */
    uint8_t pc11_val = gpio_input_bit_get(GPIOC, GPIO_PIN_11); /* SDIO0_D3 (JP52) */
    uint8_t pc9_val  = gpio_input_bit_get(GPIOC, GPIO_PIN_9);  /* SDIO0_D1 (JP62) */

    rt_snprintf(buf, sizeof(buf), "D0(PB13)=%u CMD(PD2)=%u", (unsigned)pb13_val, (unsigned)pd2_val);
    lcd_draw_string_xy(CANVAS_X0 + 8U, ty, buf,
                       (pb13_val && pd2_val) ? CONSOLE_COLOR_GREEN : CONSOLE_COLOR_RED,
                       UI_CANVAS_BG);
    ty += 16U;

    rt_snprintf(buf, sizeof(buf), "JP65:D0=%u JP52:D3=%u JP62:D1=%u",
                (unsigned)pb13_val, (unsigned)pc11_val, (unsigned)pc9_val);
    lcd_draw_string_xy(CANVAS_X0 + 8U, ty, buf, CONSOLE_COLOR_LIGHTGRAY, UI_CANVAS_BG);
    ty += 16U;

    rt_kprintf("  Pins: D0[PB13/JP65]=%u CMD[PD2]=%u CLK[PC12]=%u D3[PC11/JP52]=%u D1[PC9/JP62]=%u\r\n",
               (unsigned)pb13_val, (unsigned)pd2_val, (unsigned)pc12_val, (unsigned)pc11_val, (unsigned)pc9_val);

    if (!pb13_val) {
        rt_kprintf("  WARNING: PB13 (SDIO0_D0) is LOW! Ensure JP65 is shorting pins 2-3!\r\n");
    }

    /* 2. Call sd_io_init() */
    rt_kprintf("  Running sd_io_init()...\r\n");
    sd_error_enum sd_err = sd_io_init();
    const char *err_str = sd_error_string(sd_err);

    rt_snprintf(buf, sizeof(buf), "Driver: %s (%d)", err_str, (int)sd_err);
    lcd_draw_string_xy(CANVAS_X0 + 8U, ty, buf,
                       (sd_err == SD_OK) ? CONSOLE_COLOR_GREEN : CONSOLE_COLOR_RED,
                       UI_CANVAS_BG);
    ty += 16U;
    rt_kprintf("  sd_io_init() status: %d (%s)\r\n", (int)sd_err, err_str);

    if (sd_err != SD_OK) {
        if (sd_err == SD_CMD_RESP_TIMEOUT) {
            lcd_draw_string_xy(CANVAS_X0 + 8U, ty, "ERR: No Card / CMD Timeout", CONSOLE_COLOR_RED, UI_CANVAS_BG);
            ty += 16U;
            lcd_draw_string_xy(CANVAS_X0 + 8U, ty, "* JP65 MUST short pins 2-3", CONSOLE_COLOR_YELLOW, UI_CANVAS_BG);
            ty += 16U;
            lcd_draw_string_xy(CANVAS_X0 + 8U, ty, "* Keep JP52 on pins 1-2(LCD)", CONSOLE_COLOR_YELLOW, UI_CANVAS_BG);
            ty += 16U;
        } else if (sd_err == SD_VOLTRANGE_INVALID) {
            lcd_draw_string_xy(CANVAS_X0 + 8U, ty, "ERR: Voltage range invalid", CONSOLE_COLOR_RED, UI_CANVAS_BG);
            ty += 16U;
        } else {
            rt_snprintf(buf, sizeof(buf), "ERR Code: 0x%02X", (unsigned int)sd_err);
            lcd_draw_string_xy(CANVAS_X0 + 8U, ty, buf, CONSOLE_COLOR_RED, UI_CANVAS_BG);
            ty += 16U;
        }
    } else {
        /* Card initialized successfully */
        const sd_card_info_struct *info = sd_card_info_get_ptr();
        uint32_t cap_mb = sd_card_capacity_get() / 1024U;

        rt_snprintf(buf, sizeof(buf), "Card: %lu MB | Blk: %u B",
                    (unsigned long)cap_mb, (unsigned int)info->card_blocksize);
        lcd_draw_string_xy(CANVAS_X0 + 8U, ty, buf, CONSOLE_COLOR_WHITE, UI_CANVAS_BG);
        ty += 16U;

        rt_snprintf(buf, sizeof(buf), "RCA:0x%04X Type:%u",
                    (unsigned int)info->card_rca, (unsigned int)info->card_type);
        lcd_draw_string_xy(CANVAS_X0 + 8U, ty, buf, CONSOLE_COLOR_CYAN, UI_CANVAS_BG);
        ty += 16U;

        rt_kprintf("  Card Info: %lu MB, Block: %u B, RCA: 0x%04X, Type: %u\r\n",
                   (unsigned long)cap_mb, (unsigned int)info->card_blocksize,
                   (unsigned int)info->card_rca, (unsigned int)info->card_type);

        /* 3. Raw Sector 0 Read Test */
        static uint32_t sec_buf[128];
        sd_error_enum rd_err = sd_block_read(sec_buf, 0U, 512U);
        uint8_t *b = (uint8_t *)sec_buf;
        if (rd_err == SD_OK) {
            uint16_t sig = ((uint16_t)b[511] << 8) | b[510];
            rt_snprintf(buf, sizeof(buf), "Sec0 Read: OK (Sig: 0x%04X)", sig);
            lcd_draw_string_xy(CANVAS_X0 + 8U, ty, buf,
                               (sig == 0xAA55U) ? CONSOLE_COLOR_GREEN : CONSOLE_COLOR_YELLOW,
                               UI_CANVAS_BG);
            ty += 16U;
            rt_kprintf("  Sector 0 Read OK! Signature: 0x%04X\r\n", sig);
            rt_kprintf("  Sector 0 Bytes: %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                       b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
        } else {
            rt_snprintf(buf, sizeof(buf), "Sec0 Read: ERR (%s)", sd_error_string(rd_err));
            lcd_draw_string_xy(CANVAS_X0 + 8U, ty, buf, CONSOLE_COLOR_RED, UI_CANVAS_BG);
            ty += 16U;
            rt_kprintf("  Sector 0 Read FAILED: %s\r\n", sd_error_string(rd_err));
        }

        /* 4. FatFs Mount Test ("0:") */
        static FATFS diag_fs;
        (void)f_mount(NULL, "0:", 0);
        FRESULT fr = f_mount(&diag_fs, "0:", 1);

        const char *fr_name = "UNKNOWN";
        switch (fr) {
            case FR_OK: fr_name = "FR_OK (Mounted!)"; break;
            case FR_NO_FILESYSTEM: fr_name = "NO_FS (13: exFAT/FAT32)"; break;
            case FR_NOT_READY: fr_name = "NOT_READY (3)"; break;
            case FR_DISK_ERR: fr_name = "DISK_ERR (1)"; break;
            default: break;
        }

        rt_snprintf(buf, sizeof(buf), "f_mount: %s", fr_name);
        lcd_draw_string_xy(CANVAS_X0 + 8U, ty, buf,
                           (fr == FR_OK) ? CONSOLE_COLOR_GREEN : CONSOLE_COLOR_RED,
                           UI_CANVAS_BG);
        ty += 16U;
        rt_kprintf("  f_mount(\"0:\", 1) = %d (%s), fs_type=%u\r\n",
                   (int)fr, fr_name, (unsigned int)diag_fs.fs_type);

        if (fr == FR_OK) {
            DIR dir;
            FILINFO fno;
            uint32_t fcount = 0U;
            if (f_opendir(&dir, "0:/") == FR_OK) {
                while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != '\0' && fcount < 2U) {
                    rt_snprintf(buf, sizeof(buf), "%s%s",
                                fno.fname, (fno.fattrib & AM_DIR) ? "/" : "");
                    lcd_draw_string_xy(CANVAS_X0 + 8U, ty, buf, CONSOLE_COLOR_LIGHTGRAY, UI_CANVAS_BG);
                    ty += 16U;
                    rt_kprintf("    * %s (%lu bytes)\r\n", fno.fname, (unsigned long)fno.fsize);
                    fcount++;
                }
                f_closedir(&dir);
            }
            if (fcount == 0U) {
                lcd_draw_string_xy(CANVAS_X0 + 8U, ty, "[Root directory is empty]", CONSOLE_COLOR_GRAY, UI_CANVAS_BG);
                ty += 16U;
            }
        }
    }

    rt_kprintf("========================================================\r\n\r\n");
    gd32h759i_lcd_flush();
}

static void ui_draw_buttons(int active_btn)
{
    char label[20];

    for (int i = 0; i < 4; i++) {
        uint16_t x0 = k_buttons[i].x0;
        uint16_t w  = (uint16_t)(k_buttons[i].x1 - k_buttons[i].x0 + 1U);
        uint16_t bg = (i == active_btn) ? UI_BTN_ACTIVE : UI_BTN_BG;

        gd32h759i_lcd_fill_rect(x0, BTN_Y0, w, BTN_H, bg);
        draw_rect_outline(x0, BTN_Y0, w, BTN_H, CONSOLE_COLOR_CYAN);

        if (i == 0) {
            lcd_draw_string_xy((uint16_t)(x0 + 14U), BTN_Y0 + 8U, "CLEAR PAD", CONSOLE_COLOR_WHITE, bg);
        } else if (i == 1) {
            rt_snprintf(label, sizeof(label), "COLOR:%s", k_brush_names[g_brush_idx]);
            lcd_draw_string_xy((uint16_t)(x0 + 10U), BTN_Y0 + 8U, label, k_brush_colors[g_brush_idx], bg);
            gd32h759i_lcd_fill_rect((uint16_t)(x0 + w - 22U), BTN_Y0 + 8U, 14U, 16U, k_brush_colors[g_brush_idx]);
        } else if (i == 2) {
            lcd_draw_string_xy((uint16_t)(x0 + 18U), BTN_Y0 + 8U, "SD TEST", CONSOLE_COLOR_YELLOW, bg);
        } else {
            rt_snprintf(label, sizeof(label), "LED:%3ums", (unsigned)k_led_periods[g_led_speed_idx]);
            lcd_draw_string_xy((uint16_t)(x0 + 14U), BTN_Y0 + 8U, label, CONSOLE_COLOR_GREEN, bg);
        }
    }
}

static void ui_init_static_frame(void)
{
    gd32h759i_lcd_fill(UI_BG_DARK);

    gd32h759i_lcd_fill_rect(0U, 0U, LCD_WIDTH, 24U, CONSOLE_COLOR_NAVY);
    gd32h759i_lcd_fill_rect(0U, 24U, LCD_WIDTH, 1U, CONSOLE_COLOR_CYAN);
    lcd_draw_string_xy(6U, 4U, "AeroDENS OS-DEBUG | RT-Thread 3.1.5 | 600MHz", CONSOLE_COLOR_YELLOW, CONSOLE_COLOR_NAVY);

    gd32h759i_lcd_fill_rect(4U, 28U, 228U, 204U, UI_PANEL_BG);
    gd32h759i_lcd_fill_rect(4U, 28U, 228U, 20U, UI_PANEL_HEADER);
    draw_rect_outline(4U, 28U, 228U, 204U, UI_BORDER_COLOR);
    lcd_draw_string_xy(10U, 30U, "RT-THREAD KERNEL & TASKS", CONSOLE_COLOR_CYAN, UI_PANEL_HEADER);

    gd32h759i_lcd_fill_rect(6U, 132U, 224U, 16U, UI_PANEL_HEADER);
    lcd_draw_string_xy(8U, 132U, "THREAD  PRI STATE   COUNT", CONSOLE_COLOR_YELLOW, UI_PANEL_HEADER);

    gd32h759i_lcd_fill_rect(236U, 28U, 240U, 204U, UI_PANEL_BG);
    gd32h759i_lcd_fill_rect(236U, 28U, 240U, 20U, UI_PANEL_HEADER);
    draw_rect_outline(236U, 28U, 240U, 204U, UI_BORDER_COLOR);

    ui_draw_buttons(-1);
    os_debug_run_sd_test();
    gd32h759i_lcd_flush();
}

static void os_idle_hook(void)
{
    g_idle_hooks++;
}

static void uptime_timer_cb(void *parameter)
{
    (void)parameter;
    uint32_t cur_ctx = rt_scheduler_get_switch_count();
    g_ctx_per_sec    = cur_ctx - g_last_ctx_count;
    g_last_ctx_count = cur_ctx;
    g_uptime_sec++;
}

static void touch_thread_entry(void *parameter)
{
    (void)parameter;
    uint16_t prev_x[5] = {0};
    uint16_t prev_y[5] = {0};
    uint8_t  prev_valid[5] = {0};
    uint8_t  btn_latched = 0U;

    if (gt911_init() == SUCCESS) {
        g_gt911_id = gt911_read_id();
        g_gt911_ok = 1U;
        rt_kprintf("[touch] GT911 initialized (ID=0x%08lX)\n", (unsigned long)g_gt911_id);
    } else {
        g_gt911_id = gt911_read_id();
        if (g_gt911_id != 0U) {
            gt911.enable = 1U;
            g_gt911_ok = 1U;
        }
        rt_kprintf("[touch] GT911 probe ID=0x%08lX enable=%u\n",
                   (unsigned long)g_gt911_id, (unsigned)gt911.enable);
    }

    while (1) {
        g_touch_cnt++;

        if (gt911_handler() == SUCCESS && gt911.count > 0U) {
            uint8_t pts = gt911.count;
            g_last_touch_x   = gt911.x[0];
            g_last_touch_y   = gt911.y[0];
            g_last_touch_pts = pts;

            rt_mutex_take(g_lcd_mutex, RT_WAITING_FOREVER);

            uint16_t x0 = gt911.x[0];
            uint16_t y0 = gt911.y[0];

            if (y0 >= BTN_Y0 && y0 <= BTN_Y1) {
                if (!btn_latched) {
                    btn_latched = 1U;
                    for (int b = 0; b < 4; b++) {
                        if (x0 >= k_buttons[b].x0 && x0 <= k_buttons[b].x1) {
                            if (b == 0) {
                                ui_clear_canvas();
                                rt_kprintf("[touch] Button: CLEAR PAD\n");
                            } else if (b == 1) {
                                g_brush_idx = (uint8_t)((g_brush_idx + 1U) % 6U);
                                rt_kprintf("[touch] Button: COLOR -> %s\n", k_brush_names[g_brush_idx]);
                            } else if (b == 2) {
                                os_debug_run_sd_test();
                                rt_kprintf("[touch] Button: SD TEST triggered\n");
                            } else if (b == 3) {
                                g_led_speed_idx = (uint8_t)((g_led_speed_idx + 1U) % 3U);
                                rt_kprintf("[touch] Button: LED SPEED -> %u ms\n",
                                           (unsigned)k_led_periods[g_led_speed_idx]);
                            }
                            ui_draw_buttons(b);
                            break;
                        }
                    }
                    gd32h759i_lcd_flush();
                }
            } else {
                bool drew_any = false;
                for (uint8_t i = 0; i < pts && i < 5U; i++) {
                    uint16_t tx = gt911.x[i];
                    uint16_t ty = gt911.y[i];

                    if (tx >= CANVAS_X0 && tx <= CANVAS_X1 &&
                        ty >= CANVAS_Y0 && ty <= CANVAS_Y1) {
                        if (!g_canvas_has_ink) {
                            gd32h759i_lcd_fill_rect(CANVAS_X0, CANVAS_Y0, CANVAS_W, CANVAS_H, UI_CANVAS_BG);
                            g_canvas_has_ink = 1U;
                        }

                        uint16_t col = (i == 0U)
                                       ? k_brush_colors[g_brush_idx]
                                       : k_brush_colors[(g_brush_idx + i) % 6U];

                        if (prev_valid[i]) {
                            canvas_draw_line(prev_x[i], prev_y[i], tx, ty, col);
                        } else {
                            canvas_draw_brush((int32_t)tx, (int32_t)ty, col);
                        }
                        prev_x[i] = tx;
                        prev_y[i] = ty;
                        prev_valid[i] = 1U;
                        drew_any = true;
                    } else {
                        prev_valid[i] = 0U;
                    }
                }
                for (uint8_t i = pts; i < 5U; i++) {
                    prev_valid[i] = 0U;
                }
                if (drew_any) {
                    gd32h759i_lcd_flush();
                }
            }

            rt_mutex_release(g_lcd_mutex);
        } else {
            g_last_touch_pts = 0U;
            for (uint8_t i = 0; i < 5U; i++) {
                prev_valid[i] = 0U;
            }
            if (btn_latched) {
                btn_latched = 0U;
                rt_mutex_take(g_lcd_mutex, RT_WAITING_FOREVER);
                ui_draw_buttons(-1);
                gd32h759i_lcd_flush();
                rt_mutex_release(g_lcd_mutex);
            }
        }

        rt_thread_mdelay(15);
    }
}

static void burst_thread_entry(void *parameter)
{
    (void)parameter;

    while (1) {
        if (rt_sem_take(g_burst_sem, RT_WAITING_FOREVER) == RT_EOK) {
            for (int i = 0; i < 12; i++) {
                uint32_t *block = (uint32_t *)rt_malloc(128U);
                if (block != RT_NULL) {
                    block[0] = 0xAED00000UL | (uint32_t)i;
                    rt_mb_send(g_telem_mb, (rt_ubase_t)block[0]);
                    rt_free(block);
                }
            }
            g_burst_count++;
            rt_kprintf("[burst] Completed heap+IPC burst #%lu\n", (unsigned long)g_burst_count);
        }
    }
}

static void sensor_thread_entry(void *parameter)
{
    (void)parameter;
    float phase = 0.0f;

    while (1) {
        g_sensor_cnt++;
        phase += 0.08f;
        if (phase > 6.2831853f) {
            phase -= 6.2831853f;
        }

        float pitch = sinf(phase) * 25.0f;
        float alt   = 120.0f + cosf(phase * 0.5f) * 35.0f;

        int16_t pitch_d10 = (int16_t)(pitch * 10.0f);
        int16_t alt_m     = (int16_t)alt;

        uint32_t packed = ((uint32_t)(uint16_t)pitch_d10 << 16) | (uint16_t)alt_m;
        rt_mb_send(g_telem_mb, (rt_ubase_t)packed);

        rt_thread_mdelay(50);
    }
}

static void worker_thread_entry(void *parameter)
{
    (void)parameter;
    rt_ubase_t msg = 0;

    while (1) {
        if (rt_mb_recv(g_telem_mb, &msg, RT_WAITING_FOREVER) == RT_EOK) {
            g_worker_cnt++;
            g_ipc_msg_count++;

            if ((msg & 0xFFF00000UL) != 0xAED00000UL) {
                int16_t pitch_d10 = (int16_t)(uint16_t)(msg >> 16);
                int16_t alt_m     = (int16_t)(uint16_t)(msg & 0xFFFFU);
                g_fpu_pitch_deg10 = pitch_d10;
                g_fpu_alt_m       = alt_m;
            }
        }
    }
}

static void ui_thread_entry(void *parameter)
{
    (void)parameter;
    char line[36];

    while (1) {
        g_ui_cnt++;

        rt_uint32_t mem_total = 0, mem_used = 0, mem_max = 0;
        rt_memory_info(&mem_total, &mem_used, &mem_max);

        uint32_t up = g_uptime_sec;
        uint32_t hh = (up / 3600U) % 100U;
        uint32_t mm = (up / 60U) % 60U;
        uint32_t ss = up % 60U;

        rt_mutex_take(g_lcd_mutex, RT_WAITING_FOREVER);

        rt_snprintf(line, sizeof(line), "Up:%02lu:%02lu:%02lu",
                    (unsigned long)hh, (unsigned long)mm, (unsigned long)ss);
        lcd_draw_string_xy(384U, 4U, line, CONSOLE_COLOR_GREEN, CONSOLE_COLOR_NAVY);

        rt_snprintf(line, sizeof(line), "Tick : %-8lu (1000Hz)", (unsigned long)rt_tick_get());
        lcd_draw_string_xy(10U, 50U, line, CONSOLE_COLOR_WHITE, UI_PANEL_BG);

        rt_snprintf(line, sizeof(line), "CtxSw: %-7lu (%4lu/s)",
                    (unsigned long)rt_scheduler_get_switch_count(),
                    (unsigned long)g_ctx_per_sec);
        lcd_draw_string_xy(10U, 66U, line, CONSOLE_COLOR_GREEN, UI_PANEL_BG);

        rt_snprintf(line, sizeof(line), "Heap : %4lu / %4lu B Max",
                    (unsigned long)mem_used, (unsigned long)mem_max);
        lcd_draw_string_xy(10U, 82U, line, CONSOLE_COLOR_CYAN, UI_PANEL_BG);

        rt_snprintf(line, sizeof(line), "IPC  : %-6lu Burst:%-3lu",
                    (unsigned long)g_ipc_msg_count, (unsigned long)g_burst_count);
        lcd_draw_string_xy(10U, 98U, line, CONSOLE_COLOR_YELLOW, UI_PANEL_BG);

        int32_t p_int = g_fpu_pitch_deg10 / 10;
        int32_t p_dec = abs(g_fpu_pitch_deg10 % 10);
        rt_snprintf(line, sizeof(line), "FPU  : %+3ld.%1lddeg Alt:%3ldm",
                    (long)p_int, (long)p_dec, (long)g_fpu_alt_m);
        lcd_draw_string_xy(10U, 114U, line, CONSOLE_COLOR_MAGENTA, UI_PANEL_BG);

        rt_snprintf(line, sizeof(line), "touch    06 READY %6lu", (unsigned long)g_touch_cnt);
        lcd_draw_string_xy(8U, 150U, line, CONSOLE_COLOR_WHITE, UI_PANEL_BG);

        rt_snprintf(line, sizeof(line), "sensor   12 TIMR  %6lu", (unsigned long)g_sensor_cnt);
        lcd_draw_string_xy(8U, 166U, line, CONSOLE_COLOR_CYAN, UI_PANEL_BG);

        rt_snprintf(line, sizeof(line), "worker   13 MBOX  %6lu", (unsigned long)g_worker_cnt);
        lcd_draw_string_xy(8U, 182U, line, CONSOLE_COLOR_GREEN, UI_PANEL_BG);

        rt_snprintf(line, sizeof(line), "ui_dash  15 RUN   %6lu", (unsigned long)g_ui_cnt);
        lcd_draw_string_xy(8U, 198U, line, CONSOLE_COLOR_YELLOW, UI_PANEL_BG);

        rt_snprintf(line, sizeof(line), "led_hb   20 SLEEP %6lu", (unsigned long)g_led_cnt);
        lcd_draw_string_xy(8U, 214U, line, CONSOLE_COLOR_LIGHTGRAY, UI_PANEL_BG);

        if (g_last_touch_pts > 0U) {
            rt_snprintf(line, sizeof(line), "TOUCH X:%3u Y:%3u Pt:%1u ",
                        (unsigned)g_last_touch_x,
                        (unsigned)g_last_touch_y,
                        (unsigned)g_last_touch_pts);
            lcd_draw_string_xy(242U, 30U, line, CONSOLE_COLOR_GREEN, UI_PANEL_HEADER);
        } else if (g_gt911_ok) {
            rt_snprintf(line, sizeof(line), "GT911 OK (X:%3u Y:%3u) ",
                        (unsigned)g_last_touch_x,
                        (unsigned)g_last_touch_y);
            lcd_draw_string_xy(242U, 30U, line, CONSOLE_COLOR_CYAN, UI_PANEL_HEADER);
        } else {
            lcd_draw_string_xy(242U, 30U, "GT911 INIT / POLLING... ", CONSOLE_COLOR_YELLOW, UI_PANEL_HEADER);
        }

        gd32h759i_lcd_flush();
        rt_mutex_release(g_lcd_mutex);

        rt_thread_mdelay(100);
    }
}

static void led_thread_entry(void *parameter)
{
    (void)parameter;

    while (1) {
        g_led_cnt++;
        gd_eval_led_toggle(LED2);
        rt_thread_mdelay(k_led_periods[g_led_speed_idx]);
    }
}

void os_debug_start(void)
{
    gd32h759i_lcd_init();

    g_lcd_mutex = rt_mutex_create("lcd_mtx", RT_IPC_FLAG_PRIO);
    g_telem_mb  = rt_mb_create("telem_mb", 16, RT_IPC_FLAG_FIFO);
    g_burst_sem = rt_sem_create("burst_s", 0, RT_IPC_FLAG_FIFO);

    RT_ASSERT(g_lcd_mutex != RT_NULL);
    RT_ASSERT(g_telem_mb  != RT_NULL);
    RT_ASSERT(g_burst_sem != RT_NULL);

    rt_thread_idle_sethook(os_idle_hook);
    g_uptime_tmr = rt_timer_create("uptime",
                                   uptime_timer_cb,
                                   RT_NULL,
                                   RT_TICK_PER_SECOND,
                                   RT_TIMER_FLAG_PERIODIC);
    if (g_uptime_tmr != RT_NULL) {
        rt_timer_start(g_uptime_tmr);
    }

    ui_init_static_frame();

    rt_kprintf("\r\n=======================================================\r\n");
    rt_kprintf("  AeroDENS OS-DEBUG Mode (0x08010000)\r\n");
    rt_kprintf("  RT-Thread 3.1.5 + GT911 Capacitive Touch\r\n");
    rt_kprintf("=======================================================\r\n\r\n");

    rt_thread_t t_touch = rt_thread_create("touch",  touch_thread_entry,  RT_NULL, 2048,  6, 10);
    rt_thread_t t_burst = rt_thread_create("burst",  burst_thread_entry,  RT_NULL, 1536,  8, 10);
    rt_thread_t t_sens  = rt_thread_create("sensor", sensor_thread_entry, RT_NULL, 2048, 12, 10);
    rt_thread_t t_work  = rt_thread_create("worker", worker_thread_entry, RT_NULL, 1536, 13, 10);
    rt_thread_t t_ui    = rt_thread_create("ui",     ui_thread_entry,     RT_NULL, 3072, 15, 10);
    rt_thread_t t_led   = rt_thread_create("led",    led_thread_entry,    RT_NULL, 1024, 20, 10);

    if (t_touch) rt_thread_startup(t_touch);
    if (t_burst) rt_thread_startup(t_burst);
    if (t_sens)  rt_thread_startup(t_sens);
    if (t_work)  rt_thread_startup(t_work);
    if (t_ui)    rt_thread_startup(t_ui);
    if (t_led)   rt_thread_startup(t_led);

    while (1) {
        rt_thread_mdelay(5000);
        rt_kprintf("[OS-DEBUG] Up=%lus Tick=%lu CtxSw=%lu/s IPC=%lu TouchPts=%u\n",
                   (unsigned long)g_uptime_sec,
                   (unsigned long)rt_tick_get(),
                   (unsigned long)g_ctx_per_sec,
                   (unsigned long)g_ipc_msg_count,
                   (unsigned)g_last_touch_pts);
    }
}
