/*
 * RT-Thread Kernel Public API Header (include/rtthread/rtthread.h)
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __RT_THREAD_H__
#define __RT_THREAD_H__

#include <rtconfig.h>
#include <rtdef.h>
#include <rtservice.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Kernel Object Interface
 */
struct rt_object_information *
rt_object_get_information(enum rt_object_class_type type);
void rt_object_init(struct rt_object         *object,
                    enum rt_object_class_type type,
                    const char               *name);
void rt_object_detach(rt_object_t object);
rt_object_t rt_object_allocate(enum rt_object_class_type type,
                               const char               *name);
void rt_object_delete(rt_object_t object);
rt_bool_t rt_object_is_systemobject(rt_object_t object);
rt_uint8_t rt_object_get_type(rt_object_t object);

/*
 * Clock & Timer Interface
 */
rt_tick_t rt_tick_get(void);
void rt_tick_set(rt_tick_t tick);
void rt_tick_increase(void);
rt_tick_t rt_tick_from_millisecond(rt_int32_t ms);

void rt_system_timer_init(void);
void rt_system_timer_thread_init(void);

void rt_timer_init(rt_timer_t  timer,
                   const char *name,
                   void (*timeout)(void *parameter),
                   void       *parameter,
                   rt_tick_t   time,
                   rt_uint8_t  flag);
rt_err_t rt_timer_detach(rt_timer_t timer);
rt_timer_t rt_timer_create(const char *name,
                           void (*timeout)(void *parameter),
                           void       *parameter,
                           rt_tick_t   time,
                           rt_uint8_t  flag);
rt_err_t rt_timer_delete(rt_timer_t timer);
rt_err_t rt_timer_start(rt_timer_t timer);
rt_err_t rt_timer_stop(rt_timer_t timer);
rt_err_t rt_timer_control(rt_timer_t timer, int cmd, void *arg);
rt_tick_t rt_timer_next_timeout_tick(void);
void rt_timer_check(void);

/*
 * Thread Interface
 */
rt_err_t rt_thread_init(struct rt_thread *thread,
                        const char       *name,
                        void (*entry)(void *parameter),
                        void             *parameter,
                        void             *stack_start,
                        rt_uint32_t       stack_size,
                        rt_uint8_t        priority,
                        rt_uint32_t       tick);
rt_err_t rt_thread_detach(rt_thread_t thread);
rt_thread_t rt_thread_create(const char *name,
                             void (*entry)(void *parameter),
                             void       *parameter,
                             rt_uint32_t stack_size,
                             rt_uint8_t  priority,
                             rt_uint32_t tick);
rt_thread_t rt_thread_self(void);
rt_thread_t rt_thread_find(char *name);
rt_err_t rt_thread_startup(rt_thread_t thread);
rt_err_t rt_thread_delete(rt_thread_t thread);

rt_err_t rt_thread_yield(void);
rt_err_t rt_thread_delay(rt_tick_t tick);
rt_err_t rt_thread_mdelay(rt_int32_t ms);
rt_err_t rt_thread_control(rt_thread_t thread, int cmd, void *arg);
rt_err_t rt_thread_suspend(rt_thread_t thread);
rt_err_t rt_thread_resume(rt_thread_t thread);
void rt_thread_timeout(void *parameter);

/*
 * Idle Thread Interface
 */
void rt_thread_idle_init(void);
void rt_thread_idle_excute(void);
rt_err_t rt_thread_idle_sethook(void (*hook)(void));
rt_err_t rt_thread_idle_delhook(void (*hook)(void));
rt_thread_t rt_thread_idle_gethandler(void);

/*
 * Scheduler Interface
 */
void rt_system_scheduler_init(void);
void rt_system_scheduler_start(void);
void rt_schedule(void);
void rt_schedule_insert_thread(struct rt_thread *thread);
void rt_schedule_remove_thread(struct rt_thread *thread);
void rt_enter_critical(void);
void rt_exit_critical(void);
rt_uint16_t rt_critical_level(void);
rt_uint32_t rt_scheduler_get_switch_count(void);

/*
 * Memory Heap Interface
 */
void rt_system_heap_init(void *begin_addr, void *end_addr);
void *rt_malloc(rt_size_t nbytes);
void rt_free(void *ptr);
void *rt_realloc(void *ptr, rt_size_t nbytes);
void *rt_calloc(rt_size_t count, rt_size_t size);
void rt_memory_info(rt_uint32_t *total,
                    rt_uint32_t *used,
                    rt_uint32_t *max_used);

/*
 * IPC: Semaphore, Mutex, Mailbox
 */
#ifdef RT_USING_SEMAPHORE
rt_err_t rt_sem_init(rt_sem_t    sem,
                     const char *name,
                     rt_uint32_t value,
                     rt_uint8_t  flag);
rt_err_t rt_sem_detach(rt_sem_t sem);
rt_sem_t rt_sem_create(const char *name, rt_uint32_t value, rt_uint8_t flag);
rt_err_t rt_sem_delete(rt_sem_t sem);
rt_err_t rt_sem_take(rt_sem_t sem, rt_int32_t time);
rt_err_t rt_sem_trytake(rt_sem_t sem);
rt_err_t rt_sem_release(rt_sem_t sem);
rt_err_t rt_sem_control(rt_sem_t sem, int cmd, void *arg);
#endif

#ifdef RT_USING_MUTEX
rt_err_t rt_mutex_init(rt_mutex_t mutex, const char *name, rt_uint8_t flag);
rt_err_t rt_mutex_detach(rt_mutex_t mutex);
rt_mutex_t rt_mutex_create(const char *name, rt_uint8_t flag);
rt_err_t rt_mutex_delete(rt_mutex_t mutex);
rt_err_t rt_mutex_take(rt_mutex_t mutex, rt_int32_t time);
rt_err_t rt_mutex_release(rt_mutex_t mutex);
rt_err_t rt_mutex_control(rt_mutex_t mutex, int cmd, void *arg);
#endif

#ifdef RT_USING_MAILBOX
rt_err_t rt_mb_init(rt_mb_t     mb,
                    const char *name,
                    void       *msgpool,
                    rt_size_t   size,
                    rt_uint8_t  flag);
rt_err_t rt_mb_detach(rt_mb_t mb);
rt_mb_t rt_mb_create(const char *name, rt_size_t size, rt_uint8_t flag);
rt_err_t rt_mb_delete(rt_mb_t mb);
rt_err_t rt_mb_send(rt_mb_t mb, rt_ubase_t value);
rt_err_t rt_mb_send_wait(rt_mb_t    mb,
                         rt_ubase_t value,
                         rt_int32_t timeout);
rt_err_t rt_mb_recv(rt_mb_t mb, rt_ubase_t *value, rt_int32_t timeout);
rt_err_t rt_mb_control(rt_mb_t mb, int cmd, void *arg);
#endif

/*
 * Interrupt Management
 */
void rt_interrupt_enter(void);
void rt_interrupt_leave(void);
rt_uint8_t rt_interrupt_get_nest(void);

/*
 * Kernel Services
 */
void rt_kprintf(const char *fmt, ...);
rt_int32_t rt_snprintf(char *buf, rt_size_t size, const char *fmt, ...);
rt_int32_t rt_vsnprintf(char *buf, rt_size_t size, const char *fmt, va_list args);
void rt_show_version(void);

void *rt_memset(void *src, int c, rt_ubase_t n);
void *rt_memcpy(void *dest, const void *src, rt_ubase_t n);
rt_int32_t rt_strncmp(const char *cs, const char *ct, rt_ubase_t count);
rt_int32_t rt_strcmp(const char *cs, const char *ct);
rt_size_t rt_strlen(const char *src);
char *rt_strncpy(char *dst, const char *src, rt_ubase_t n);
int __rt_ffs(int value);

void rt_components_board_init(void);
void rt_components_init(void);
int rtthread_startup(void);

#define RT_ASSERT(EX)                                                         \
if (!(EX))                                                                    \
{                                                                             \
    rt_kprintf("(%s) assertion failed at %s:%d\n", #EX, __FUNCTION__, __LINE__);\
    while (1);                                                                \
}

#ifdef __cplusplus
}
#endif

#endif /* __RT_THREAD_H__ */

