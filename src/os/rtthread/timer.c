/*
 * RT-Thread Timer Subsystem (src/os/rtthread/timer.c)
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtthread.h>
#include <rthw.h>

static rt_list_t rt_timer_list[RT_TIMER_SKIP_LIST_LEVEL];

static void _rt_timer_init(rt_timer_t timer,
                           void (*timeout)(void *parameter),
                           void      *parameter,
                           rt_tick_t  time,
                           rt_uint8_t flag)
{
    int i;

    timer->parent.flag  = flag & ~RT_TIMER_FLAG_ACTIVATED;
    timer->timeout_func = timeout;
    timer->parameter    = parameter;
    timer->timeout_tick = 0;
    timer->init_tick    = time;

    for (i = 0; i < RT_TIMER_SKIP_LIST_LEVEL; i++) {
        rt_list_init(&(timer->row[i]));
    }
}

rt_inline void _rt_timer_remove(rt_timer_t timer)
{
    int i;
    for (i = 0; i < RT_TIMER_SKIP_LIST_LEVEL; i++) {
        rt_list_remove(&timer->row[i]);
    }
}

void rt_timer_init(rt_timer_t  timer,
                   const char *name,
                   void (*timeout)(void *parameter),
                   void       *parameter,
                   rt_tick_t   time,
                   rt_uint8_t  flag)
{
    RT_ASSERT(timer != RT_NULL);

    rt_object_init((rt_object_t)timer, RT_Object_Class_Timer, name);
    _rt_timer_init(timer, timeout, parameter, time, flag);
}

rt_err_t rt_timer_detach(rt_timer_t timer)
{
    register rt_base_t level;

    RT_ASSERT(timer != RT_NULL);

    level = rt_hw_interrupt_disable();
    _rt_timer_remove(timer);
    timer->parent.flag &= ~RT_TIMER_FLAG_ACTIVATED;
    rt_hw_interrupt_enable(level);

    rt_object_detach((rt_object_t)timer);
    return RT_EOK;
}

#ifdef RT_USING_HEAP
rt_timer_t rt_timer_create(const char *name,
                           void (*timeout)(void *parameter),
                           void       *parameter,
                           rt_tick_t   time,
                           rt_uint8_t  flag)
{
    struct rt_timer *timer;

    timer = (struct rt_timer *)rt_object_allocate(RT_Object_Class_Timer, name);
    if (timer == RT_NULL) {
        return RT_NULL;
    }

    _rt_timer_init(timer, timeout, parameter, time, flag);
    return timer;
}

rt_err_t rt_timer_delete(rt_timer_t timer)
{
    register rt_base_t level;

    RT_ASSERT(timer != RT_NULL);

    level = rt_hw_interrupt_disable();
    _rt_timer_remove(timer);
    timer->parent.flag &= ~RT_TIMER_FLAG_ACTIVATED;
    rt_hw_interrupt_enable(level);

    rt_object_delete((rt_object_t)timer);
    return RT_EOK;
}
#endif

rt_err_t rt_timer_start(rt_timer_t timer)
{
    unsigned int row_lvl;
    rt_list_t *timer_list;
    register rt_base_t level;
    rt_list_t *row_head[RT_TIMER_SKIP_LIST_LEVEL];

    RT_ASSERT(timer != RT_NULL);

    level = rt_hw_interrupt_disable();
    _rt_timer_remove(timer);
    timer->parent.flag &= ~RT_TIMER_FLAG_ACTIVATED;
    rt_hw_interrupt_enable(level);

    timer->timeout_tick = rt_tick_get() + timer->init_tick;

    level = rt_hw_interrupt_disable();
    timer_list = rt_timer_list;
    row_head[0] = &timer_list[0];

    for (row_lvl = 0; row_lvl < RT_TIMER_SKIP_LIST_LEVEL; row_lvl++) {
        for (; row_head[row_lvl] != timer_list[row_lvl].prev;
             row_head[row_lvl] = row_head[row_lvl]->next) {
            struct rt_timer *t;
            rt_list_t *p = row_head[row_lvl]->next;

            t = rt_list_entry(p, struct rt_timer, row[row_lvl]);

            if ((t->timeout_tick - timer->timeout_tick) == 0) {
                continue;
            } else if ((t->timeout_tick - timer->timeout_tick) < RT_TICK_MAX / 2) {
                break;
            }
        }
        if (row_lvl != RT_TIMER_SKIP_LIST_LEVEL - 1) {
            row_head[row_lvl + 1] = row_head[row_lvl] + 1;
        }
    }

    timer->parent.flag |= RT_TIMER_FLAG_ACTIVATED;
    rt_list_insert_after(row_head[RT_TIMER_SKIP_LIST_LEVEL - 1],
                         &(timer->row[RT_TIMER_SKIP_LIST_LEVEL - 1]));

    rt_hw_interrupt_enable(level);
    return RT_EOK;
}

rt_err_t rt_timer_stop(rt_timer_t timer)
{
    register rt_base_t level;

    RT_ASSERT(timer != RT_NULL);
    if (!(timer->parent.flag & RT_TIMER_FLAG_ACTIVATED)) {
        return -RT_ERROR;
    }

    level = rt_hw_interrupt_disable();
    _rt_timer_remove(timer);
    timer->parent.flag &= ~RT_TIMER_FLAG_ACTIVATED;
    rt_hw_interrupt_enable(level);

    return RT_EOK;
}

rt_err_t rt_timer_control(rt_timer_t timer, int cmd, void *arg)
{
    register rt_base_t level;

    RT_ASSERT(timer != RT_NULL);

    level = rt_hw_interrupt_disable();
    switch (cmd) {
    case RT_TIMER_CTRL_GET_TIME:
        *(rt_tick_t *)arg = timer->init_tick;
        break;
    case RT_TIMER_CTRL_SET_TIME:
        timer->init_tick = *(rt_tick_t *)arg;
        break;
    case RT_TIMER_CTRL_SET_ONESHOT:
        timer->parent.flag &= ~RT_TIMER_FLAG_PERIODIC;
        break;
    case RT_TIMER_CTRL_SET_PERIODIC:
        timer->parent.flag |= RT_TIMER_FLAG_PERIODIC;
        break;
    case RT_TIMER_CTRL_GET_STATE:
        if (timer->parent.flag & RT_TIMER_FLAG_ACTIVATED) {
            *(rt_tick_t *)arg = RT_TIMER_FLAG_ACTIVATED;
        } else {
            *(rt_tick_t *)arg = RT_TIMER_FLAG_DEACTIVATED;
        }
        break;
    default:
        break;
    }
    rt_hw_interrupt_enable(level);
    return RT_EOK;
}

void rt_timer_check(void)
{
    struct rt_timer *t;
    rt_tick_t current_tick;
    register rt_base_t level;
    rt_list_t list;

    rt_list_init(&list);
    current_tick = rt_tick_get();

    level = rt_hw_interrupt_disable();

    while (!rt_list_isempty(&rt_timer_list[RT_TIMER_SKIP_LIST_LEVEL - 1])) {
        t = rt_list_entry(rt_timer_list[RT_TIMER_SKIP_LIST_LEVEL - 1].next,
                          struct rt_timer,
                          row[RT_TIMER_SKIP_LIST_LEVEL - 1]);

        if ((current_tick - t->timeout_tick) < RT_TICK_MAX / 2) {
            _rt_timer_remove(t);
            if (!(t->parent.flag & RT_TIMER_FLAG_PERIODIC)) {
                t->parent.flag &= ~RT_TIMER_FLAG_ACTIVATED;
            }
            rt_list_insert_after(&list, &(t->row[RT_TIMER_SKIP_LIST_LEVEL - 1]));
            t->timeout_func(t->parameter);

            current_tick = rt_tick_get();

            if (rt_list_isempty(&list)) {
                continue;
            }
            rt_list_remove(&(t->row[RT_TIMER_SKIP_LIST_LEVEL - 1]));
            if ((t->parent.flag & RT_TIMER_FLAG_PERIODIC) &&
                (t->parent.flag & RT_TIMER_FLAG_ACTIVATED)) {
                t->parent.flag &= ~RT_TIMER_FLAG_ACTIVATED;
                rt_timer_start(t);
            }
        } else {
            break;
        }
    }

    rt_hw_interrupt_enable(level);
}

void rt_system_timer_init(void)
{
    rt_size_t i;
    for (i = 0; i < sizeof(rt_timer_list) / sizeof(rt_timer_list[0]); i++) {
        rt_list_init(rt_timer_list + i);
    }
}

void rt_system_timer_thread_init(void)
{
}
