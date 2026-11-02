/*
 * RT-Thread Idle Thread & Defunct Reaper (src/os/rtthread/idle.c)
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rthw.h>
#include <rtthread.h>

static struct rt_thread idle;
RT_ALIGN(RT_ALIGN_SIZE)
static rt_uint8_t rt_thread_stack[IDLE_THREAD_STACK_SIZE];

extern rt_list_t rt_thread_defunct;

#ifdef RT_USING_IDLE_HOOK
static void (*idle_hook_list[RT_IDLE_HOOK_LIST_SIZE])(void);

rt_err_t rt_thread_idle_sethook(void (*hook)(void))
{
    rt_size_t i;
    rt_base_t level;
    rt_err_t ret = -RT_EFULL;

    level = rt_hw_interrupt_disable();
    for (i = 0; i < RT_IDLE_HOOK_LIST_SIZE; i++) {
        if (idle_hook_list[i] == RT_NULL) {
            idle_hook_list[i] = hook;
            ret = RT_EOK;
            break;
        }
    }
    rt_hw_interrupt_enable(level);
    return ret;
}

rt_err_t rt_thread_idle_delhook(void (*hook)(void))
{
    rt_size_t i;
    rt_base_t level;
    rt_err_t ret = -RT_ENOSYS;

    level = rt_hw_interrupt_disable();
    for (i = 0; i < RT_IDLE_HOOK_LIST_SIZE; i++) {
        if (idle_hook_list[i] == hook) {
            idle_hook_list[i] = RT_NULL;
            ret = RT_EOK;
            break;
        }
    }
    rt_hw_interrupt_enable(level);
    return ret;
}
#endif

void rt_thread_idle_excute(void)
{
    while (!rt_list_isempty(&rt_thread_defunct)) {
        rt_base_t lock;
        rt_thread_t thread;

        lock = rt_hw_interrupt_disable();
        if (!rt_list_isempty(&rt_thread_defunct)) {
            thread = rt_list_entry(rt_thread_defunct.next,
                                   struct rt_thread,
                                   tlist);
            rt_list_remove(&(thread->tlist));
            rt_hw_interrupt_enable(lock);

#ifdef RT_USING_HEAP
            rt_free(thread->stack_addr);
            rt_object_delete((rt_object_t)thread);
#endif
        } else {
            rt_hw_interrupt_enable(lock);
        }
    }
}

static void rt_thread_idle_entry(void *parameter)
{
    (void)parameter;
    while (1) {
#ifdef RT_USING_IDLE_HOOK
        rt_size_t i;
        for (i = 0; i < RT_IDLE_HOOK_LIST_SIZE; i++) {
            if (idle_hook_list[i] != RT_NULL) {
                idle_hook_list[i]();
            }
        }
#endif
        rt_thread_idle_excute();
        __asm volatile ("wfi");
    }
}

void rt_thread_idle_init(void)
{
    rt_thread_init(&idle,
                   "tidle",
                   rt_thread_idle_entry,
                   RT_NULL,
                   &rt_thread_stack[0],
                   sizeof(rt_thread_stack),
                   RT_THREAD_PRIORITY_MAX - 1,
                   32);

    rt_thread_startup(&idle);
}

rt_thread_t rt_thread_idle_gethandler(void)
{
    return (rt_thread_t)(&idle);
}

