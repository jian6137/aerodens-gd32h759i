/*
 * RT-Thread 32-Priority Preemptive & Round-Robin Scheduler (src/os/rtthread/scheduler.c)
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtthread.h>
#include <rthw.h>

rt_list_t rt_thread_priority_table[RT_THREAD_PRIORITY_MAX];
struct rt_thread *rt_current_thread = RT_NULL;
rt_uint8_t rt_current_priority;
rt_uint32_t rt_thread_ready_priority_group;
rt_list_t rt_thread_defunct;

static rt_int16_t rt_scheduler_lock_nest = 0;
static volatile rt_uint32_t rt_context_switch_count = 0;

rt_uint32_t rt_scheduler_get_switch_count(void)
{
    return rt_context_switch_count;
}

void rt_system_scheduler_init(void)
{
    register rt_base_t offset;

    rt_scheduler_lock_nest = 0;

    for (offset = 0; offset < RT_THREAD_PRIORITY_MAX; offset++) {
        rt_list_init(&rt_thread_priority_table[offset]);
    }

    rt_current_priority = RT_THREAD_PRIORITY_MAX - 1;
    rt_current_thread = RT_NULL;
    rt_thread_ready_priority_group = 0;

    rt_list_init(&rt_thread_defunct);
}

void rt_system_scheduler_start(void)
{
    register struct rt_thread *to_thread;
    register rt_ubase_t highest_ready_priority;

    highest_ready_priority = __rt_ffs(rt_thread_ready_priority_group) - 1;

    to_thread = rt_list_entry(rt_thread_priority_table[highest_ready_priority].next,
                              struct rt_thread,
                              tlist);

    rt_current_thread = to_thread;
    rt_context_switch_count++;

    rt_hw_context_switch_to((rt_ubase_t)&to_thread->sp);
}

void rt_schedule(void)
{
    rt_base_t level;
    struct rt_thread *to_thread;
    struct rt_thread *from_thread;

    level = rt_hw_interrupt_disable();

    if (rt_scheduler_lock_nest == 0) {
        rt_ubase_t highest_ready_priority;

        if (rt_thread_ready_priority_group != 0) {
            highest_ready_priority = __rt_ffs(rt_thread_ready_priority_group) - 1;
            to_thread = rt_list_entry(rt_thread_priority_table[highest_ready_priority].next,
                                      struct rt_thread,
                                      tlist);

            if ((rt_current_thread->stat & RT_THREAD_STAT_MASK) == RT_THREAD_RUNNING) {
                if (rt_current_thread->current_priority < highest_ready_priority) {
                    to_thread = rt_current_thread;
                } else if (rt_current_thread->current_priority == highest_ready_priority &&
                           (rt_current_thread->stat & RT_THREAD_STAT_YIELD_MASK) == 0) {
                    to_thread = rt_current_thread;
                }
            }

            if (to_thread != rt_current_thread) {
                rt_current_priority = (rt_uint8_t)highest_ready_priority;
                from_thread         = rt_current_thread;
                rt_current_thread   = to_thread;

                to_thread->stat = RT_THREAD_RUNNING | (to_thread->stat & ~RT_THREAD_STAT_MASK);
                rt_context_switch_count++;

                if (rt_interrupt_get_nest() == 0) {
                    rt_hw_context_switch((rt_ubase_t)&from_thread->sp,
                                         (rt_ubase_t)&to_thread->sp);
                    rt_hw_interrupt_enable(level);
                    return;
                } else {
                    rt_hw_context_switch_interrupt((rt_ubase_t)&from_thread->sp,
                                                   (rt_ubase_t)&to_thread->sp);
                }
            } else {
                rt_current_thread->stat &= ~RT_THREAD_STAT_YIELD_MASK;
            }
        }
    }

    rt_hw_interrupt_enable(level);
}

void rt_schedule_insert_thread(struct rt_thread *thread)
{
    register rt_base_t temp;

    RT_ASSERT(thread != RT_NULL);

    temp = rt_hw_interrupt_disable();

    if (thread == rt_current_thread) {
        thread->stat = RT_THREAD_RUNNING | (thread->stat & ~RT_THREAD_STAT_MASK);
        goto __exit;
    }

    thread->stat = RT_THREAD_READY | (thread->stat & ~RT_THREAD_STAT_MASK);
    rt_list_insert_before(&(rt_thread_priority_table[thread->current_priority]),
                          &(thread->tlist));
    rt_thread_ready_priority_group |= thread->number_mask;

__exit:
    rt_hw_interrupt_enable(temp);
}

void rt_schedule_remove_thread(struct rt_thread *thread)
{
    register rt_base_t level;

    RT_ASSERT(thread != RT_NULL);

    level = rt_hw_interrupt_disable();

    rt_list_remove(&(thread->tlist));
    if (rt_list_isempty(&(rt_thread_priority_table[thread->current_priority]))) {
        rt_thread_ready_priority_group &= ~thread->number_mask;
    }

    rt_hw_interrupt_enable(level);
}

void rt_enter_critical(void)
{
    register rt_base_t level;
    level = rt_hw_interrupt_disable();
    rt_scheduler_lock_nest++;
    rt_hw_interrupt_enable(level);
}

void rt_exit_critical(void)
{
    register rt_base_t level;
    level = rt_hw_interrupt_disable();
    rt_scheduler_lock_nest--;
    if (rt_scheduler_lock_nest <= 0) {
        rt_scheduler_lock_nest = 0;
        rt_hw_interrupt_enable(level);
        if (rt_current_thread != RT_NULL) {
            rt_schedule();
        }
    } else {
        rt_hw_interrupt_enable(level);
    }
}

rt_uint16_t rt_critical_level(void)
{
    return (rt_uint16_t)rt_scheduler_lock_nest;
}

