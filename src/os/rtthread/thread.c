/*
 * RT-Thread Thread Management (src/os/rtthread/thread.c)
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtthread.h>
#include <rthw.h>

extern struct rt_thread *rt_current_thread;
extern rt_list_t rt_thread_defunct;

static void rt_thread_exit(void)
{
    struct rt_thread *thread;
    register rt_base_t level;

    thread = rt_current_thread;

    level = rt_hw_interrupt_disable();

    rt_schedule_remove_thread(thread);
    thread->stat = RT_THREAD_CLOSE;
    rt_timer_detach(&thread->thread_timer);

    if (rt_object_is_systemobject((rt_object_t)thread) == RT_TRUE) {
        rt_object_detach((rt_object_t)thread);
    } else {
        rt_list_insert_after(&rt_thread_defunct, &(thread->tlist));
    }

    rt_hw_interrupt_enable(level);
    rt_schedule();
}

static rt_err_t _rt_thread_init(struct rt_thread *thread,
                                const char       *name,
                                void (*entry)(void *parameter),
                                void             *parameter,
                                void             *stack_start,
                                rt_uint32_t       stack_size,
                                rt_uint8_t        priority,
                                rt_uint32_t       tick)
{
    rt_list_init(&(thread->tlist));

    thread->entry = (void *)entry;
    thread->parameter = parameter;

    thread->stack_addr = stack_start;
    thread->stack_size = stack_size;

    rt_memset(thread->stack_addr, '#', thread->stack_size);
    thread->sp = (void *)rt_hw_stack_init(thread->entry,
                                          thread->parameter,
                                          (rt_uint8_t *)((char *)thread->stack_addr +
                                                         thread->stack_size -
                                                         sizeof(rt_ubase_t)),
                                          (void *)rt_thread_exit);

    RT_ASSERT(priority < RT_THREAD_PRIORITY_MAX);
    thread->init_priority    = priority;
    thread->current_priority = priority;
    thread->number_mask      = 0;

    thread->init_tick      = tick;
    thread->remaining_tick = tick;

    thread->error   = RT_EOK;
    thread->stat    = RT_THREAD_INIT;
    thread->cleanup = 0;
    thread->user_data = 0;

    rt_timer_init(&(thread->thread_timer),
                  thread->name,
                  rt_thread_timeout,
                  thread,
                  0,
                  RT_TIMER_FLAG_ONE_SHOT);

    return RT_EOK;
}

rt_err_t rt_thread_init(struct rt_thread *thread,
                        const char       *name,
                        void (*entry)(void *parameter),
                        void             *parameter,
                        void             *stack_start,
                        rt_uint32_t       stack_size,
                        rt_uint8_t        priority,
                        rt_uint32_t       tick)
{
    RT_ASSERT(thread != RT_NULL);
    RT_ASSERT(stack_start != RT_NULL);

    rt_object_init((rt_object_t)thread, RT_Object_Class_Thread, name);

    return _rt_thread_init(thread,
                           name,
                           entry,
                           parameter,
                           stack_start,
                           stack_size,
                           priority,
                           tick);
}

rt_thread_t rt_thread_self(void)
{
    return rt_current_thread;
}

rt_err_t rt_thread_startup(rt_thread_t thread)
{
    RT_ASSERT(thread != RT_NULL);
    RT_ASSERT((thread->stat & RT_THREAD_STAT_MASK) == RT_THREAD_INIT);

    thread->current_priority = thread->init_priority;
    thread->number_mask      = 1UL << thread->current_priority;
    thread->stat             = RT_THREAD_SUSPEND;

    rt_thread_resume(thread);
    if (rt_thread_self() != RT_NULL) {
        rt_schedule();
    }

    return RT_EOK;
}

rt_err_t rt_thread_detach(rt_thread_t thread)
{
    rt_base_t lock;

    RT_ASSERT(thread != RT_NULL);
    RT_ASSERT(rt_object_is_systemobject((rt_object_t)thread));

    if ((thread->stat & RT_THREAD_STAT_MASK) == RT_THREAD_CLOSE) {
        return RT_EOK;
    }

    if ((thread->stat & RT_THREAD_STAT_MASK) != RT_THREAD_INIT) {
        rt_schedule_remove_thread(thread);
    }

    rt_timer_detach(&(thread->thread_timer));

    lock = rt_hw_interrupt_disable();
    thread->stat = RT_THREAD_CLOSE;
    if (rt_object_is_systemobject((rt_object_t)thread) == RT_TRUE) {
        rt_object_detach((rt_object_t)thread);
    } else {
        rt_list_insert_after(&rt_thread_defunct, &(thread->tlist));
    }
    rt_hw_interrupt_enable(lock);

    return RT_EOK;
}

#ifdef RT_USING_HEAP
rt_thread_t rt_thread_create(const char *name,
                             void (*entry)(void *parameter),
                             void       *parameter,
                             rt_uint32_t stack_size,
                             rt_uint8_t  priority,
                             rt_uint32_t tick)
{
    struct rt_thread *thread;
    void *stack_start;

    thread = (struct rt_thread *)rt_object_allocate(RT_Object_Class_Thread, name);
    if (thread == RT_NULL) {
        return RT_NULL;
    }

    stack_size = RT_ALIGN_UP(stack_size, RT_ALIGN_SIZE);
    stack_start = (void *)rt_malloc(stack_size);
    if (stack_start == RT_NULL) {
        rt_object_delete((rt_object_t)thread);
        return RT_NULL;
    }

    _rt_thread_init(thread,
                    name,
                    entry,
                    parameter,
                    stack_start,
                    stack_size,
                    priority,
                    tick);

    return thread;
}

rt_err_t rt_thread_delete(rt_thread_t thread)
{
    rt_base_t lock;

    RT_ASSERT(thread != RT_NULL);
    RT_ASSERT(!rt_object_is_systemobject((rt_object_t)thread));

    if ((thread->stat & RT_THREAD_STAT_MASK) == RT_THREAD_CLOSE) {
        return RT_EOK;
    }

    if ((thread->stat & RT_THREAD_STAT_MASK) != RT_THREAD_INIT) {
        rt_schedule_remove_thread(thread);
    }

    rt_timer_detach(&(thread->thread_timer));

    lock = rt_hw_interrupt_disable();
    thread->stat = RT_THREAD_CLOSE;
    rt_list_insert_after(&rt_thread_defunct, &(thread->tlist));
    rt_hw_interrupt_enable(lock);

    return RT_EOK;
}
#endif

rt_err_t rt_thread_yield(void)
{
    register rt_base_t level;
    struct rt_thread *thread;

    level = rt_hw_interrupt_disable();
    thread = rt_current_thread;

    if ((thread->stat & RT_THREAD_STAT_MASK) == RT_THREAD_RUNNING) {
        thread->stat |= RT_THREAD_STAT_YIELD;
        rt_schedule();
    }

    rt_hw_interrupt_enable(level);
    return RT_EOK;
}

rt_err_t rt_thread_sleep(rt_tick_t tick)
{
    register rt_base_t temp;
    struct rt_thread *thread;

    temp = rt_hw_interrupt_disable();
    thread = rt_current_thread;
    RT_ASSERT(thread != RT_NULL);

    rt_thread_suspend(thread);

    rt_timer_control(&(thread->thread_timer), RT_TIMER_CTRL_SET_TIME, &tick);
    rt_timer_start(&(thread->thread_timer));

    rt_hw_interrupt_enable(temp);

    rt_schedule();

    if (thread->error == -RT_ETIMEOUT) {
        thread->error = RT_EOK;
    }

    return RT_EOK;
}

rt_err_t rt_thread_delay(rt_tick_t tick)
{
    return rt_thread_sleep(tick);
}

rt_err_t rt_thread_mdelay(rt_int32_t ms)
{
    rt_tick_t tick;
    tick = rt_tick_from_millisecond(ms);
    return rt_thread_sleep(tick);
}

rt_err_t rt_thread_control(rt_thread_t thread, int cmd, void *arg)
{
    register rt_base_t temp;

    RT_ASSERT(thread != RT_NULL);

    switch (cmd) {
    case RT_THREAD_CTRL_CHANGE_PRIORITY:
        temp = rt_hw_interrupt_disable();
        if ((thread->stat & RT_THREAD_STAT_MASK) == RT_THREAD_READY) {
            rt_schedule_remove_thread(thread);
            thread->current_priority = *(rt_uint8_t *)arg;
            thread->number_mask = 1UL << thread->current_priority;
            rt_schedule_insert_thread(thread);
        } else {
            thread->current_priority = *(rt_uint8_t *)arg;
            thread->number_mask = 1UL << thread->current_priority;
        }
        rt_hw_interrupt_enable(temp);
        break;

    case RT_THREAD_CTRL_STARTUP:
        return rt_thread_startup(thread);

    case RT_THREAD_CTRL_CLOSE:
        if (rt_object_is_systemobject((rt_object_t)thread) == RT_TRUE) {
            return rt_thread_detach(thread);
        }
#ifdef RT_USING_HEAP
        else {
            return rt_thread_delete(thread);
        }
#endif
        break;

    default:
        break;
    }

    return RT_EOK;
}

rt_err_t rt_thread_suspend(rt_thread_t thread)
{
    register rt_base_t temp;

    RT_ASSERT(thread != RT_NULL);

    if ((thread->stat & RT_THREAD_STAT_MASK) != RT_THREAD_READY &&
        (thread->stat & RT_THREAD_STAT_MASK) != RT_THREAD_RUNNING) {
        return -RT_ERROR;
    }

    temp = rt_hw_interrupt_disable();
    thread->stat = RT_THREAD_SUSPEND | (thread->stat & ~RT_THREAD_STAT_MASK);
    rt_schedule_remove_thread(thread);
    rt_timer_stop(&(thread->thread_timer));
    rt_hw_interrupt_enable(temp);

    return RT_EOK;
}

rt_err_t rt_thread_resume(rt_thread_t thread)
{
    register rt_base_t temp;

    RT_ASSERT(thread != RT_NULL);

    if ((thread->stat & RT_THREAD_STAT_MASK) != RT_THREAD_SUSPEND) {
        return -RT_ERROR;
    }

    temp = rt_hw_interrupt_disable();
    rt_list_remove(&(thread->tlist));
    rt_timer_stop(&thread->thread_timer);
    rt_hw_interrupt_enable(temp);

    rt_schedule_insert_thread(thread);
    return RT_EOK;
}

void rt_thread_timeout(void *parameter)
{
    struct rt_thread *thread;

    thread = (struct rt_thread *)parameter;
    RT_ASSERT(thread != RT_NULL);

    thread->error = -RT_ETIMEOUT;
    rt_list_remove(&(thread->tlist));
    rt_schedule_insert_thread(thread);
    rt_schedule();
}

rt_thread_t rt_thread_find(char *name)
{
    struct rt_object_information *information;
    struct rt_object *object;
    struct rt_list_node *node;

    if (rt_thread_self() != RT_NULL) {
        rt_enter_critical();
    }

    information = rt_object_get_information(RT_Object_Class_Thread);
    RT_ASSERT(information != RT_NULL);
    for (node = information->object_list.next;
         node != &(information->object_list);
         node = node->next) {
        object = rt_list_entry(node, struct rt_object, list);
        if (rt_strncmp(object->name, name, RT_NAME_MAX) == 0) {
            if (rt_thread_self() != RT_NULL) {
                rt_exit_critical();
            }
            return (rt_thread_t)object;
        }
    }

    if (rt_thread_self() != RT_NULL) {
        rt_exit_critical();
    }

    return RT_NULL;
}

