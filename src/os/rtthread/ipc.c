/*
 * RT-Thread Inter-Thread Communication (Semaphore, Mutex, Mailbox) (src/os/rtthread/ipc.c)
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtthread.h>
#include <rthw.h>

rt_inline rt_err_t rt_ipc_object_init(struct rt_ipc_object *ipc)
{
    rt_list_init(&(ipc->suspend_thread));
    return RT_EOK;
}

rt_inline rt_err_t rt_ipc_list_suspend(rt_list_t        *list,
                                       struct rt_thread *thread,
                                       rt_uint8_t        flag)
{
    rt_thread_suspend(thread);

    switch (flag) {
    case RT_IPC_FLAG_FIFO:
        rt_list_insert_before(list, &(thread->tlist));
        break;

    case RT_IPC_FLAG_PRIO:
    {
        struct rt_list_node *n;
        struct rt_thread *sthread;

        for (n = list->next; n != list; n = n->next) {
            sthread = rt_list_entry(n, struct rt_thread, tlist);
            if (thread->current_priority < sthread->current_priority) {
                rt_list_insert_before(&(sthread->tlist), &(thread->tlist));
                break;
            }
        }
        if (n == list) {
            rt_list_insert_before(list, &(thread->tlist));
        }
        break;
    }

    default:
        break;
    }

    return RT_EOK;
}

rt_inline rt_err_t rt_ipc_list_resume(rt_list_t *list)
{
    struct rt_thread *thread;

    thread = rt_list_entry(list->next, struct rt_thread, tlist);
    rt_thread_resume(thread);

    return RT_EOK;
}

rt_inline rt_err_t rt_ipc_list_resume_all(rt_list_t *list)
{
    struct rt_thread *thread;
    register rt_ubase_t temp;

    while (!rt_list_isempty(list)) {
        temp = rt_hw_interrupt_disable();
        thread = rt_list_entry(list->next, struct rt_thread, tlist);
        thread->error = -RT_ERROR;
        rt_thread_resume(thread);
        rt_hw_interrupt_enable(temp);
    }

    return RT_EOK;
}

/*
 * Semaphore Implementation
 */
#ifdef RT_USING_SEMAPHORE
rt_err_t rt_sem_init(rt_sem_t    sem,
                     const char *name,
                     rt_uint32_t value,
                     rt_uint8_t  flag)
{
    RT_ASSERT(sem != RT_NULL);

    rt_object_init(&(sem->parent.parent), RT_Object_Class_Semaphore, name);
    rt_ipc_object_init(&(sem->parent));

    sem->value = (rt_uint16_t)value;
    sem->parent.parent.flag = flag;

    return RT_EOK;
}

rt_err_t rt_sem_detach(rt_sem_t sem)
{
    RT_ASSERT(sem != RT_NULL);

    rt_ipc_list_resume_all(&(sem->parent.suspend_thread));
    rt_object_detach(&(sem->parent.parent));

    return RT_EOK;
}

#ifdef RT_USING_HEAP
rt_sem_t rt_sem_create(const char *name, rt_uint32_t value, rt_uint8_t flag)
{
    rt_sem_t sem;

    sem = (rt_sem_t)rt_object_allocate(RT_Object_Class_Semaphore, name);
    if (sem == RT_NULL) {
        return RT_NULL;
    }

    rt_ipc_object_init(&(sem->parent));
    sem->value = (rt_uint16_t)value;
    sem->parent.parent.flag = flag;

    return sem;
}

rt_err_t rt_sem_delete(rt_sem_t sem)
{
    RT_ASSERT(sem != RT_NULL);

    rt_ipc_list_resume_all(&(sem->parent.suspend_thread));
    rt_object_delete(&(sem->parent.parent));

    return RT_EOK;
}
#endif

rt_err_t rt_sem_take(rt_sem_t sem, rt_int32_t time)
{
    register rt_base_t temp;
    struct rt_thread *thread;

    RT_ASSERT(sem != RT_NULL);

    temp = rt_hw_interrupt_disable();

    if (sem->value > 0) {
        sem->value--;
        rt_hw_interrupt_enable(temp);
    } else {
        if (time == 0) {
            rt_hw_interrupt_enable(temp);
            return -RT_ETIMEOUT;
        } else {
            thread = rt_thread_self();
            thread->error = RT_EOK;

            rt_ipc_list_suspend(&(sem->parent.suspend_thread),
                                thread,
                                sem->parent.parent.flag);

            if (time > 0) {
                rt_timer_control(&(thread->thread_timer),
                                 RT_TIMER_CTRL_SET_TIME,
                                 &time);
                rt_timer_start(&(thread->thread_timer));
            }

            rt_hw_interrupt_enable(temp);
            rt_schedule();

            if (thread->error != RT_EOK) {
                return thread->error;
            }
        }
    }

    return RT_EOK;
}

rt_err_t rt_sem_trytake(rt_sem_t sem)
{
    return rt_sem_take(sem, 0);
}

rt_err_t rt_sem_release(rt_sem_t sem)
{
    register rt_base_t temp;
    register rt_bool_t need_schedule;

    RT_ASSERT(sem != RT_NULL);

    need_schedule = RT_FALSE;
    temp = rt_hw_interrupt_disable();

    if (!rt_list_isempty(&sem->parent.suspend_thread)) {
        rt_ipc_list_resume(&(sem->parent.suspend_thread));
        need_schedule = RT_TRUE;
    } else {
        sem->value++;
    }

    rt_hw_interrupt_enable(temp);

    if (need_schedule == RT_TRUE) {
        rt_schedule();
    }

    return RT_EOK;
}

rt_err_t rt_sem_control(rt_sem_t sem, int cmd, void *arg)
{
    rt_ubase_t level;

    RT_ASSERT(sem != RT_NULL);

    if (cmd == RT_IPC_CMD_RESET) {
        rt_ubase_t value = (rt_ubase_t)arg;
        level = rt_hw_interrupt_disable();
        rt_ipc_list_resume_all(&sem->parent.suspend_thread);
        sem->value = (rt_uint16_t)value;
        rt_hw_interrupt_enable(level);
        rt_schedule();
        return RT_EOK;
    }

    return -RT_ERROR;
}
#endif /* RT_USING_SEMAPHORE */

/*
 * Mutex Implementation (with Priority Inheritance)
 */
#ifdef RT_USING_MUTEX
rt_err_t rt_mutex_init(rt_mutex_t mutex, const char *name, rt_uint8_t flag)
{
    RT_ASSERT(mutex != RT_NULL);

    rt_object_init(&(mutex->parent.parent), RT_Object_Class_Mutex, name);
    rt_ipc_object_init(&(mutex->parent));

    mutex->value              = 1;
    mutex->owner              = RT_NULL;
    mutex->original_priority  = 0xFF;
    mutex->hold               = 0;
    mutex->parent.parent.flag = flag;

    return RT_EOK;
}

rt_err_t rt_mutex_detach(rt_mutex_t mutex)
{
    RT_ASSERT(mutex != RT_NULL);

    rt_ipc_list_resume_all(&(mutex->parent.suspend_thread));
    rt_object_detach(&(mutex->parent.parent));

    return RT_EOK;
}

#ifdef RT_USING_HEAP
rt_mutex_t rt_mutex_create(const char *name, rt_uint8_t flag)
{
    struct rt_mutex *mutex;

    mutex = (rt_mutex_t)rt_object_allocate(RT_Object_Class_Mutex, name);
    if (mutex == RT_NULL) {
        return RT_NULL;
    }

    rt_ipc_object_init(&(mutex->parent));
    mutex->value              = 1;
    mutex->owner              = RT_NULL;
    mutex->original_priority  = 0xFF;
    mutex->hold               = 0;
    mutex->parent.parent.flag = flag;

    return mutex;
}

rt_err_t rt_mutex_delete(rt_mutex_t mutex)
{
    RT_ASSERT(mutex != RT_NULL);

    rt_ipc_list_resume_all(&(mutex->parent.suspend_thread));
    rt_object_delete(&(mutex->parent.parent));

    return RT_EOK;
}
#endif

rt_err_t rt_mutex_take(rt_mutex_t mutex, rt_int32_t time)
{
    register rt_base_t temp;
    struct rt_thread *thread;

    RT_ASSERT(mutex != RT_NULL);

    thread = rt_thread_self();
    RT_ASSERT(thread != RT_NULL);

    temp = rt_hw_interrupt_disable();
    thread->error = RT_EOK;

    if (mutex->owner == thread) {
        mutex->hold++;
    } else {
__again:
        if (mutex->value > 0) {
            mutex->value--;
            mutex->owner             = thread;
            mutex->original_priority = thread->current_priority;
            mutex->hold++;
        } else {
            if (time == 0) {
                thread->error = -RT_ETIMEOUT;
                rt_hw_interrupt_enable(temp);
                return -RT_ETIMEOUT;
            } else {
                if (thread->current_priority < mutex->owner->current_priority) {
                    rt_thread_control(mutex->owner,
                                      RT_THREAD_CTRL_CHANGE_PRIORITY,
                                      &thread->current_priority);
                }

                rt_ipc_list_suspend(&(mutex->parent.suspend_thread),
                                    thread,
                                    mutex->parent.parent.flag);

                if (time > 0) {
                    rt_timer_control(&(thread->thread_timer),
                                     RT_TIMER_CTRL_SET_TIME,
                                     &time);
                    rt_timer_start(&(thread->thread_timer));
                }

                rt_hw_interrupt_enable(temp);
                rt_schedule();

                if (thread->error != RT_EOK) {
                    return thread->error;
                } else {
                    temp = rt_hw_interrupt_disable();
                    goto __again;
                }
            }
        }
    }

    rt_hw_interrupt_enable(temp);
    return RT_EOK;
}

rt_err_t rt_mutex_release(rt_mutex_t mutex)
{
    register rt_base_t temp;
    struct rt_thread *thread;
    rt_bool_t need_schedule;

    RT_ASSERT(mutex != RT_NULL);

    need_schedule = RT_FALSE;
    thread = rt_thread_self();

    temp = rt_hw_interrupt_disable();

    RT_ASSERT(thread == mutex->owner);

    mutex->hold--;
    if (mutex->hold == 0) {
        if (mutex->original_priority != mutex->owner->current_priority) {
            rt_thread_control(mutex->owner,
                              RT_THREAD_CTRL_CHANGE_PRIORITY,
                              &(mutex->original_priority));
        }

        if (!rt_list_isempty(&mutex->parent.suspend_thread)) {
            thread = rt_list_entry(mutex->parent.suspend_thread.next,
                                   struct rt_thread,
                                   tlist);

            mutex->owner             = thread;
            mutex->original_priority = thread->current_priority;
            mutex->hold++;

            rt_ipc_list_resume(&(mutex->parent.suspend_thread));
            need_schedule = RT_TRUE;
        } else {
            mutex->value++;
            mutex->owner             = RT_NULL;
            mutex->original_priority = 0xFF;
        }
    }

    rt_hw_interrupt_enable(temp);

    if (need_schedule == RT_TRUE) {
        rt_schedule();
    }

    return RT_EOK;
}

rt_err_t rt_mutex_control(rt_mutex_t mutex, int cmd, void *arg)
{
    (void)mutex;
    (void)cmd;
    (void)arg;
    return -RT_ERROR;
}
#endif /* RT_USING_MUTEX */

/*
 * Mailbox Implementation
 */
#ifdef RT_USING_MAILBOX
rt_err_t rt_mb_init(rt_mb_t     mb,
                    const char *name,
                    void       *msgpool,
                    rt_size_t   size,
                    rt_uint8_t  flag)
{
    RT_ASSERT(mb != RT_NULL);

    rt_object_init(&(mb->parent.parent), RT_Object_Class_MailBox, name);
    rt_ipc_object_init(&(mb->parent));

    mb->parent.parent.flag = flag;
    mb->msg_pool   = (rt_ubase_t *)msgpool;
    mb->size       = (rt_uint16_t)size;
    mb->entry      = 0;
    mb->in_offset  = 0;
    mb->out_offset = 0;

    rt_list_init(&(mb->suspend_sender_thread));

    return RT_EOK;
}

rt_err_t rt_mb_detach(rt_mb_t mb)
{
    RT_ASSERT(mb != RT_NULL);

    rt_ipc_list_resume_all(&(mb->parent.suspend_thread));
    rt_ipc_list_resume_all(&(mb->suspend_sender_thread));
    rt_object_detach(&(mb->parent.parent));

    return RT_EOK;
}

#ifdef RT_USING_HEAP
rt_mb_t rt_mb_create(const char *name, rt_size_t size, rt_uint8_t flag)
{
    rt_mb_t mb;

    mb = (rt_mb_t)rt_object_allocate(RT_Object_Class_MailBox, name);
    if (mb == RT_NULL) {
        return RT_NULL;
    }

    mb->parent.parent.flag = flag;
    rt_ipc_object_init(&(mb->parent));

    mb->size     = (rt_uint16_t)size;
    mb->msg_pool = (rt_ubase_t *)rt_malloc(mb->size * sizeof(rt_ubase_t));
    if (mb->msg_pool == RT_NULL) {
        rt_object_delete(&(mb->parent.parent));
        return RT_NULL;
    }

    mb->entry      = 0;
    mb->in_offset  = 0;
    mb->out_offset = 0;
    rt_list_init(&(mb->suspend_sender_thread));

    return mb;
}

rt_err_t rt_mb_delete(rt_mb_t mb)
{
    RT_ASSERT(mb != RT_NULL);

    rt_ipc_list_resume_all(&(mb->parent.suspend_thread));
    rt_ipc_list_resume_all(&(mb->suspend_sender_thread));
    rt_free(mb->msg_pool);
    rt_object_delete(&(mb->parent.parent));

    return RT_EOK;
}
#endif

rt_err_t rt_mb_send_wait(rt_mb_t    mb,
                         rt_ubase_t value,
                         rt_int32_t timeout)
{
    struct rt_thread *thread;
    register rt_ubase_t temp;
    rt_uint32_t tick_delta;

    RT_ASSERT(mb != RT_NULL);

    temp = rt_hw_interrupt_disable();

    if (mb->entry == mb->size && timeout == 0) {
        rt_hw_interrupt_enable(temp);
        return -RT_EFULL;
    }

    thread = rt_thread_self();
    tick_delta = 0;

    while (mb->entry == mb->size) {
        thread->error = RT_EOK;
        if (timeout == 0) {
            rt_hw_interrupt_enable(temp);
            return -RT_EFULL;
        }

        rt_ipc_list_suspend(&(mb->suspend_sender_thread),
                            thread,
                            mb->parent.parent.flag);

        if (timeout > 0) {
            tick_delta = rt_tick_get();
            rt_timer_control(&(thread->thread_timer),
                             RT_TIMER_CTRL_SET_TIME,
                             &timeout);
            rt_timer_start(&(thread->thread_timer));
        }

        rt_hw_interrupt_enable(temp);
        rt_schedule();

        if (thread->error != RT_EOK) {
            return thread->error;
        }

        temp = rt_hw_interrupt_disable();
        if (timeout > 0) {
            tick_delta = rt_tick_get() - tick_delta;
            timeout -= (rt_int32_t)tick_delta;
            if (timeout < 0) {
                timeout = 0;
            }
        }
    }

    mb->msg_pool[mb->in_offset] = value;
    ++mb->in_offset;
    if (mb->in_offset >= mb->size) {
        mb->in_offset = 0;
    }
    mb->entry++;

    if (!rt_list_isempty(&mb->parent.suspend_thread)) {
        rt_ipc_list_resume(&(mb->parent.suspend_thread));
        rt_hw_interrupt_enable(temp);
        rt_schedule();
        return RT_EOK;
    }

    rt_hw_interrupt_enable(temp);
    return RT_EOK;
}

rt_err_t rt_mb_send(rt_mb_t mb, rt_ubase_t value)
{
    return rt_mb_send_wait(mb, value, 0);
}

rt_err_t rt_mb_recv(rt_mb_t mb, rt_ubase_t *value, rt_int32_t timeout)
{
    struct rt_thread *thread;
    register rt_ubase_t temp;
    rt_uint32_t tick_delta;

    RT_ASSERT(mb != RT_NULL);

    temp = rt_hw_interrupt_disable();

    if (mb->entry == 0 && timeout == 0) {
        rt_hw_interrupt_enable(temp);
        return -RT_ETIMEOUT;
    }

    thread = rt_thread_self();
    tick_delta = 0;

    while (mb->entry == 0) {
        thread->error = RT_EOK;
        if (timeout == 0) {
            rt_hw_interrupt_enable(temp);
            return -RT_ETIMEOUT;
        }

        rt_ipc_list_suspend(&(mb->parent.suspend_thread),
                            thread,
                            mb->parent.parent.flag);

        if (timeout > 0) {
            tick_delta = rt_tick_get();
            rt_timer_control(&(thread->thread_timer),
                             RT_TIMER_CTRL_SET_TIME,
                             &timeout);
            rt_timer_start(&(thread->thread_timer));
        }

        rt_hw_interrupt_enable(temp);
        rt_schedule();

        if (thread->error != RT_EOK) {
            return thread->error;
        }

        temp = rt_hw_interrupt_disable();
        if (timeout > 0) {
            tick_delta = rt_tick_get() - tick_delta;
            timeout -= (rt_int32_t)tick_delta;
            if (timeout < 0) {
                timeout = 0;
            }
        }
    }

    *value = mb->msg_pool[mb->out_offset];
    ++mb->out_offset;
    if (mb->out_offset >= mb->size) {
        mb->out_offset = 0;
    }
    mb->entry--;

    if (!rt_list_isempty(&(mb->suspend_sender_thread))) {
        rt_ipc_list_resume(&(mb->suspend_sender_thread));
        rt_hw_interrupt_enable(temp);
        rt_schedule();
        return RT_EOK;
    }

    rt_hw_interrupt_enable(temp);
    return RT_EOK;
}

rt_err_t rt_mb_control(rt_mb_t mb, int cmd, void *arg)
{
    rt_ubase_t level;
    (void)arg;

    RT_ASSERT(mb != RT_NULL);

    if (cmd == RT_IPC_CMD_RESET) {
        level = rt_hw_interrupt_disable();
        rt_ipc_list_resume_all(&(mb->parent.suspend_thread));
        rt_ipc_list_resume_all(&(mb->suspend_sender_thread));
        mb->entry      = 0;
        mb->in_offset  = 0;
        mb->out_offset = 0;
        rt_hw_interrupt_enable(level);
        rt_schedule();
        return RT_EOK;
    }

    return -RT_ERROR;
}
#endif /* RT_USING_MAILBOX */

