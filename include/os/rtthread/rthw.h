/*
 * RT-Thread Hardware Abstraction Layer Header (include/rtthread/rthw.h)
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __RT_HW_H__
#define __RT_HW_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

rt_uint8_t *rt_hw_stack_init(void       *entry,
                             void       *parameter,
                             rt_uint8_t *stack_addr,
                             void       *exit);

rt_base_t rt_hw_interrupt_disable(void);
void rt_hw_interrupt_enable(rt_base_t level);

void rt_hw_context_switch(rt_ubase_t from, rt_ubase_t to);
void rt_hw_context_switch_to(rt_ubase_t to);
void rt_hw_context_switch_interrupt(rt_ubase_t from, rt_ubase_t to);

void rt_hw_console_output(const char *str);
void rt_hw_board_init(void);
void rt_hw_us_delay(rt_uint32_t us);
void rt_hw_cpu_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* __RT_HW_H__ */

