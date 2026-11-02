/*!
    \file    src/os/app_main.c
    \brief   AeroDENS Material 3 LVGL Electronic Flight Bag (EFB) Main Application
             Target: GD32H759I-EVAL (600MHz ARM Cortex-M7 | 480x272 RGB565)
*/

#include "app_main.h"
#include "gd32h7xx.h"
#include "gd32h759i_eval.h"
#include "gd32h759i_lcd.h"
#include "systick.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "ui_manager.h"
#include <rtthread.h>
#include <rthw.h>
#include <stdio.h>

rt_mutex_t g_lvgl_mutex = RT_NULL;
static volatile uint32_t s_app_ticks = 0U;

void app_lvgl_lock(void)
{
    if (g_lvgl_mutex != RT_NULL) {
        rt_mutex_take(g_lvgl_mutex, RT_WAITING_FOREVER);
    }
}

void app_lvgl_unlock(void)
{
    if (g_lvgl_mutex != RT_NULL) {
        rt_mutex_release(g_lvgl_mutex);
    }
}

static void app_lvgl_thread(void *parameter)
{
    (void)parameter;

    rt_kprintf("\r\n=======================================================\r\n");
    rt_kprintf("  AeroDENS Application (0x08010000)\r\n");
    rt_kprintf("  SystemCoreClock : %lu Hz\r\n", (unsigned long)SystemCoreClock);
    rt_kprintf("=======================================================\r\n\r\n");

    g_lvgl_mutex = rt_mutex_create("lvgl_mtx", RT_IPC_FLAG_PRIO);

    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();
    ui_manager_init();

    while (1) {
        s_app_ticks++;

        app_lvgl_lock();
        uint32_t wait_ms = lv_timer_handler();
        ui_manager_update();
        app_lvgl_unlock();

        if (wait_ms < 5U)  wait_ms = 5U;
        if (wait_ms > 25U) wait_ms = 25U;

        rt_thread_mdelay(wait_ms);
    }
}

static void app_led_thread(void *parameter)
{
    (void)parameter;

    while (1) {
        gd_eval_led_toggle(LED2);
        rt_thread_mdelay(500);
    }
}

void app_main_start(void)
{
    gd32h759i_lcd_init();
    gd32h759i_lcd_fill(0x0000U);

    rt_thread_t t_gui = rt_thread_create("lvgl", app_lvgl_thread, RT_NULL, 16384, 10, 10);
    rt_thread_t t_led = rt_thread_create("led_hb", app_led_thread, RT_NULL, 1024, 20, 20);

    if (t_gui) rt_thread_startup(t_gui);
    if (t_led) rt_thread_startup(t_led);

    while (1) {
        rt_thread_mdelay(5000);
        lv_mem_monitor_t mon;

        app_lvgl_lock();
        lv_mem_monitor(&mon);
        app_lvgl_unlock();

        rt_kprintf("[APP] Up=%lus LVGL Ticks=%lu RT-Tick=%lu | LVGL Mem: used=%lu/%lu (%u%%), max=%lu, frag=%u%%\r\n",
                   (unsigned long)(s_app_ticks * 5 / 1000),
                   (unsigned long)s_app_ticks,
                   (unsigned long)rt_tick_get(),
                   (unsigned long)(mon.total_size - mon.free_size),
                   (unsigned long)mon.total_size,
                   mon.used_pct,
                   (unsigned long)mon.max_used,
                   mon.frag_pct);
    }
}
