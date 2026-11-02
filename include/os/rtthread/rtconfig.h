/*
 * AeroDENS RT-Thread RTOS Configuration (include/rtthread/rtconfig.h)
 * Target: GigaDevice GD32H759I-EVAL (ARM Cortex-M7 @ 600 MHz, OS Partition @ 0x08010000)
 */

#ifndef RT_CONFIG_H__
#define RT_CONFIG_H__

/* RT-Thread Kernel Version (Nano 3.1.5 compatible) */
#define RT_NAME_MAX                     12
#define RT_ALIGN_SIZE                   8
#define RT_THREAD_PRIORITY_32
#define RT_THREAD_PRIORITY_MAX          32
#define RT_TICK_PER_SECOND              1000
#define RT_USING_OVERFLOW_CHECK
#define RT_USING_HOOK
#define RT_USING_IDLE_HOOK
#define RT_IDLE_HOOK_LIST_SIZE          4
#define IDLE_THREAD_STACK_SIZE          1024

/* Software Timer Thread */
#define RT_USING_TIMER_SOFT             0

/* Inter-Thread Communication (IPC) */
#define RT_USING_SEMAPHORE
#define RT_USING_MUTEX
#define RT_USING_EVENT
#define RT_USING_MAILBOX
#define RT_USING_MESSAGEQUEUE

/* Memory Management */
#define RT_USING_MEMPOOL
#define RT_USING_HEAP
#define RT_USING_SMALL_MEM
#define RT_HEAP_SIZE                    (48 * 1024)

/* Kernel Console */
#define RT_USING_CONSOLE
#define RT_CONSOLEBUF_SIZE              256

/* Automatic Component & User Main Thread Initialization */
#define RT_USING_COMPONENTS_INIT
#define RT_USING_USER_MAIN
#define RT_MAIN_THREAD_STACK_SIZE       4096
#define RT_MAIN_THREAD_PRIORITY         10

#endif /* RT_CONFIG_H__ */

