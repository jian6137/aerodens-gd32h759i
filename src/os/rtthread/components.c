/*
 * RT-Thread Component Initialization & User Main Thread Startup (src/os/rtthread/components.c)
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rthw.h>
#include <rtthread.h>

#ifdef RT_USING_COMPONENTS_INIT
static int rti_start(void)
{
    return 0;
}
INIT_EXPORT(rti_start, "0");

static int rti_board_start(void)
{
    return 0;
}
INIT_EXPORT(rti_board_start, "0.end");

static int rti_board_end(void)
{
    return 0;
}
INIT_EXPORT(rti_board_end, "1.end");

static int rti_end(void)
{
    return 0;
}
INIT_EXPORT(rti_end, "6.end");

void rt_components_board_init(void)
{
    volatile const init_fn_t *fn_ptr;
    for (fn_ptr = &__rt_init_rti_board_start; fn_ptr < &__rt_init_rti_board_end; fn_ptr++) {
        (*fn_ptr)();
    }
}

void rt_components_init(void)
{
    volatile const init_fn_t *fn_ptr;
    for (fn_ptr = &__rt_init_rti_board_end; fn_ptr < &__rt_init_rti_end; fn_ptr++) {
        (*fn_ptr)();
    }
}
#endif /* RT_USING_COMPONENTS_INIT */

#ifdef RT_USING_USER_MAIN

extern int main(void);
extern int __real_main(void);

RT_ALIGN(RT_ALIGN_SIZE)
static rt_uint8_t main_stack[RT_MAIN_THREAD_STACK_SIZE];
struct rt_thread main_thread;

/* GCC --wrap=main intercept: Reset_Handler calls main() -> __wrap_main() starts RT-Thread */
int __wrap_main(void)
{
    rtthread_startup();
    return 0;
}

static void main_thread_entry(void *parameter)
{
    (void)parameter;
#ifdef RT_USING_COMPONENTS_INIT
    rt_components_init();
#endif
    __real_main();
}

void rt_application_init(void)
{
    rt_thread_t tid = &main_thread;
    rt_err_t result;

    result = rt_thread_init(tid,
                            "main",
                            main_thread_entry,
                            RT_NULL,
                            main_stack,
                            sizeof(main_stack),
                            RT_MAIN_THREAD_PRIORITY,
                            20);
    RT_ASSERT(result == RT_EOK);

    rt_thread_startup(tid);
}

int rtthread_startup(void)
{
    rt_hw_interrupt_disable();

    /* Board level initialization (VTOR, Cache, Clock, Heap, Console) */
    rt_hw_board_init();

    /* Print RT-Thread version banner on USART0 */
    rt_show_version();

    /* Timer system initialization */
    rt_system_timer_init();

    /* Scheduler system initialization */
    rt_system_scheduler_init();

    /* Create and start main application thread */
    rt_application_init();

    /* Timer thread initialization */
    rt_system_timer_thread_init();

    /* Idle thread initialization */
    rt_thread_idle_init();

    /* Start RT-Thread scheduler */
    rt_system_scheduler_start();

    return 0;
}
#endif /* RT_USING_USER_MAIN */
