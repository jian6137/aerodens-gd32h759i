/*!
    \file    src/os/ui/port/lv_port_disp.c
    \brief   TLI and IPA display driver port for LVGL
*/

#include "lv_port_disp.h"
#include "gd32h7xx.h"
#include "gd32h759i_lcd.h"
#include <rtthread.h>
#include <rthw.h>
#include <string.h>

#define LV_BUF_LINES        30U
#define LV_BUF_SIZE         (LCD_WIDTH * LV_BUF_LINES)

static lv_disp_draw_buf_t draw_buf_dsc;
static lv_color_t buf_1[LV_BUF_SIZE] __attribute__((aligned(32)));
static lv_color_t buf_2[LV_BUF_SIZE] __attribute__((aligned(32)));

static lv_disp_drv_t disp_drv;
static rt_sem_t s_ipa_sem = RT_NULL;

void IPA_IRQHandler(void)
{
    rt_interrupt_enter();

    if (RESET != ipa_interrupt_flag_get(IPA_INT_FLAG_FTF)) {
        ipa_interrupt_flag_clear(IPA_INT_FLAG_FTF);
    }
    if (RESET != ipa_interrupt_flag_get(IPA_INT_FLAG_TAE)) {
        ipa_interrupt_flag_clear(IPA_INT_FLAG_TAE);
    }

    if (s_ipa_sem != RT_NULL) {
        rt_sem_release(s_ipa_sem);
    }

    rt_interrupt_leave();
}

static void disp_clean_dcache(lv_disp_drv_t *drv)
{
#if __CORTEX_M >= 0x07
    if (drv != NULL && drv->draw_buf != NULL && drv->draw_buf->buf_act != NULL) {
        SCB_CleanDCache_by_Addr((volatile void *)drv->draw_buf->buf_act,
                                (int32_t)(LV_BUF_SIZE * sizeof(lv_color_t)));
    }
#endif
}

static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t width  = (uint32_t)(area->x2 - area->x1 + 1);
    uint32_t height = (uint32_t)(area->y2 - area->y1 + 1);
    uint16_t offline = (uint16_t)(LCD_WIDTH - width);

    IPA_CTL = 0x0U;
    IPA_FMADDR = (uint32_t)color_p;
    IPA_DMADDR = (uint32_t)DISPLAY_FRAMEBUFFER + 2U * (LCD_WIDTH * (uint32_t)area->y1 + (uint32_t)area->x1);
    IPA_FLOFF = 0U;
    IPA_DLOFF = offline;
    IPA_FPCTL = FOREGROUND_PPF_RGB565;
    IPA_IMS = (uint32_t)(width << 16U) | (uint16_t)height;

    ipa_interrupt_flag_clear(IPA_INT_FLAG_FTF | IPA_INT_FLAG_TAE);
    ipa_interrupt_enable(IPA_INT_FTF | IPA_INT_TAE);

    /* Drain any residual semaphore count before initiating transfer */
    if (s_ipa_sem != RT_NULL) {
        while (rt_sem_trytake(s_ipa_sem) == RT_EOK) {
            /* flush pending token */
        }
    }

    /* Start IPA transfer */
    IPA_CTL |= IPA_CTL_TEN;

    /* Yield CPU to other threads while DMA transfers in background */
    if (s_ipa_sem != RT_NULL) {
        rt_sem_take(s_ipa_sem, rt_tick_from_millisecond(50));
    }

    lv_disp_flush_ready(drv);
}

void lv_port_disp_init(void)
{
    rcu_periph_clock_enable(RCU_IPA);

    if (s_ipa_sem == RT_NULL) {
        s_ipa_sem = rt_sem_create("ipa_sem", 0, RT_IPC_FLAG_FIFO);
    }
    nvic_irq_enable(IPA_IRQn, 3U, 0U);

    gd32h759i_lcd_init();
    memset((void *)DISPLAY_FRAMEBUFFER, 0, LCD_FRAME_BYTES);
    SCB_CleanDCache_by_Addr((volatile void *)DISPLAY_FRAMEBUFFER, (int32_t)LCD_FRAME_BYTES);

    lv_disp_draw_buf_init(&draw_buf_dsc, buf_1, buf_2, LV_BUF_SIZE);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_WIDTH;
    disp_drv.ver_res = LCD_HEIGHT;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf_dsc;
    disp_drv.clean_dcache_cb = disp_clean_dcache;

    lv_disp_drv_register(&disp_drv);
}
