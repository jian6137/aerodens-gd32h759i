/*
 * RT-Thread Interrupt Nesting Management (src/os/rtthread/irq.c)
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rthw.h>
#include <rtthread.h>

volatile rt_uint8_t rt_interrupt_nest = 0;

void rt_interrupt_enter(void)
{
    rt_base_t level;
    level = rt_hw_interrupt_disable();
    rt_interrupt_nest++;
    rt_hw_interrupt_enable(level);
}

void rt_interrupt_leave(void)
{
    rt_base_t level;
    level = rt_hw_interrupt_disable();
    rt_interrupt_nest--;
    rt_hw_interrupt_enable(level);
}

rt_uint8_t rt_interrupt_get_nest(void)
{
    rt_uint8_t ret;
    rt_base_t level;

    level = rt_hw_interrupt_disable();
    ret = rt_interrupt_nest;
    rt_hw_interrupt_enable(level);
    return ret;
}

