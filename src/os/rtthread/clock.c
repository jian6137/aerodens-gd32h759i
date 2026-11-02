/*
 * RT-Thread System Tick & Clock Management (src/os/rtthread/clock.c)
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rthw.h>
#include <rtthread.h>

static volatile rt_tick_t rt_tick = 0;

rt_tick_t rt_tick_get(void)
{
    return rt_tick;
}

void rt_tick_set(rt_tick_t tick)
{
    rt_base_t level;
    level = rt_hw_interrupt_disable();
    rt_tick = tick;
    rt_hw_interrupt_enable(level);
}

void rt_tick_increase(void)
{
    struct rt_thread *thread;

    ++rt_tick;

    thread = rt_thread_self();
    if (thread != RT_NULL) {
        --thread->remaining_tick;
        if (thread->remaining_tick == 0) {
            thread->remaining_tick = thread->init_tick;
            thread->stat |= RT_THREAD_STAT_YIELD;
            rt_schedule();
        }
    }

    rt_timer_check();
}

rt_tick_t rt_tick_from_millisecond(rt_int32_t ms)
{
    rt_tick_t tick;

    if (ms < 0) {
        tick = (rt_tick_t)RT_WAITING_FOREVER;
    } else {
        tick = RT_TICK_PER_SECOND * (ms / 1000);
        tick += (RT_TICK_PER_SECOND * (ms % 1000) + 999) / 1000;
    }

    return tick;
}

