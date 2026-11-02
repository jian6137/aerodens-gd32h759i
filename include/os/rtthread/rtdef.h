/*
 * RT-Thread Kernel Definitions (include/rtthread/rtdef.h)
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __RT_DEF_H__
#define __RT_DEF_H__

#include <rtconfig.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RT-Thread version information */
#define RT_VERSION                      3L
#define RT_SUBVERSION                   1L
#define RT_REVISION                     5L
#define RTTHREAD_VERSION                ((RT_VERSION * 10000) + (RT_SUBVERSION * 100) + RT_REVISION)

/* Basic data types */
typedef int                             rt_bool_t;
typedef signed long                     rt_base_t;
typedef unsigned long                   rt_ubase_t;
typedef int8_t                          rt_int8_t;
typedef int16_t                         rt_int16_t;
typedef int32_t                         rt_int32_t;
typedef uint8_t                         rt_uint8_t;
typedef uint16_t                        rt_uint16_t;
typedef uint32_t                        rt_uint32_t;
typedef int64_t                         rt_int64_t;
typedef uint64_t                        rt_uint64_t;
typedef size_t                          rt_size_t;
typedef rt_base_t                       rt_err_t;
typedef rt_uint32_t                     rt_time_t;
typedef rt_uint32_t                     rt_tick_t;
typedef rt_base_t                       rt_flag_t;
typedef rt_ubase_t                      rt_dev_t;
typedef rt_base_t                       rt_off_t;

#define RT_TRUE                         1
#define RT_FALSE                        0
#define RT_NULL                         (0)

#define RT_UINT8_MAX                    0xff
#define RT_UINT16_MAX                   0xffff
#define RT_UINT32_MAX                   0xffffffff
#define RT_TICK_MAX                     RT_UINT32_MAX

/* Compiler attributes */
#define rt_inline                       static __inline
#define RT_USED                         __attribute__((used))
#define RT_SECTION(x)                   __attribute__((section(x)))
#define RT_WEAK                         __attribute__((weak))
#define RT_ALIGN(size)                  __attribute__((aligned(size)))

#define RT_ALIGN_UP(size, align)        (((size) + (align) - 1) & ~((align) - 1))
#define RT_ALIGN_DOWN(size, align)      ((size) & ~((align) - 1))

/* RT-Thread error codes */
#define RT_EOK                          0               /**< No error */
#define RT_ERROR                        1               /**< Generic error */
#define RT_ETIMEOUT                     2               /**< Timed out */
#define RT_EFULL                        3               /**< Resource is full */
#define RT_EEMPTY                       4               /**< Resource is empty */
#define RT_ENOMEM                       5               /**< No memory */
#define RT_ENOSYS                       6               /**< No system */
#define RT_EBUSY                        7               /**< Busy */
#define RT_EIO                          8               /**< IO error */
#define RT_EINTR                        9               /**< Interrupted system call */
#define RT_EINVAL                       10              /**< Invalid argument */

/* Automatic component initialization macros */
typedef int (*init_fn_t)(void);
#define INIT_EXPORT(fn, level) \
    RT_USED const init_fn_t __rt_init_##fn RT_SECTION(".rti_fn." level) = fn

#define INIT_BOARD_EXPORT(fn)           INIT_EXPORT(fn, "1")
#define INIT_PREV_EXPORT(fn)            INIT_EXPORT(fn, "2")
#define INIT_DEVICE_EXPORT(fn)          INIT_EXPORT(fn, "3")
#define INIT_COMPONENT_EXPORT(fn)       INIT_EXPORT(fn, "4")
#define INIT_ENV_EXPORT(fn)             INIT_EXPORT(fn, "5")
#define INIT_APP_EXPORT(fn)             INIT_EXPORT(fn, "6")

/* Doubly-linked list node */
struct rt_list_node
{
    struct rt_list_node *next;
    struct rt_list_node *prev;
};
typedef struct rt_list_node rt_list_t;

/* Kernel Object definitions */
enum rt_object_class_type
{
    RT_Object_Class_Null          = 0x00,
    RT_Object_Class_Thread        = 0x01,
    RT_Object_Class_Semaphore     = 0x02,
    RT_Object_Class_Mutex         = 0x03,
    RT_Object_Class_Event         = 0x04,
    RT_Object_Class_MailBox       = 0x05,
    RT_Object_Class_MessageQueue  = 0x06,
    RT_Object_Class_MemHeap       = 0x07,
    RT_Object_Class_MemPool       = 0x08,
    RT_Object_Class_Device        = 0x09,
    RT_Object_Class_Timer         = 0x0a,
    RT_Object_Class_Unknown       = 0x0c,
    RT_Object_Class_Static        = 0x80
};

struct rt_object
{
    char       name[RT_NAME_MAX];
    rt_uint8_t type;
    rt_uint8_t flag;
    rt_list_t  list;
};
typedef struct rt_object *rt_object_t;

struct rt_object_information
{
    enum rt_object_class_type type;
    rt_list_t                 object_list;
    rt_size_t                 object_size;
};

/* Timer definitions */
#define RT_TIMER_FLAG_DEACTIVATED       0x0
#define RT_TIMER_FLAG_ACTIVATED         0x1
#define RT_TIMER_FLAG_ONE_SHOT          0x0
#define RT_TIMER_FLAG_PERIODIC          0x2
#define RT_TIMER_FLAG_HARD_TIMER        0x0
#define RT_TIMER_FLAG_SOFT_TIMER        0x4

#define RT_TIMER_CTRL_SET_TIME          0x0
#define RT_TIMER_CTRL_GET_TIME          0x1
#define RT_TIMER_CTRL_SET_ONESHOT       0x2
#define RT_TIMER_CTRL_SET_PERIODIC      0x3
#define RT_TIMER_CTRL_GET_STATE         0x4

#define RT_TIMER_SKIP_LIST_LEVEL        1

struct rt_timer
{
    struct rt_object parent;
    rt_list_t        row[RT_TIMER_SKIP_LIST_LEVEL];
    void (*timeout_func)(void *parameter);
    void            *parameter;
    rt_tick_t        init_tick;
    rt_tick_t        timeout_tick;
};
typedef struct rt_timer *rt_timer_t;

/* Thread state definitions */
#define RT_THREAD_INIT                  0x00
#define RT_THREAD_READY                 0x01
#define RT_THREAD_SUSPEND               0x02
#define RT_THREAD_RUNNING               0x03
#define RT_THREAD_BLOCK                 RT_THREAD_SUSPEND
#define RT_THREAD_CLOSE                 0x04
#define RT_THREAD_STAT_MASK             0x07

#define RT_THREAD_STAT_YIELD            0x08
#define RT_THREAD_STAT_YIELD_MASK       RT_THREAD_STAT_YIELD

#define RT_THREAD_CTRL_STARTUP          0x00
#define RT_THREAD_CTRL_CLOSE            0x01
#define RT_THREAD_CTRL_CHANGE_PRIORITY  0x02
#define RT_THREAD_CTRL_INFO             0x03

struct rt_thread
{
    /* rt object */
    char        name[RT_NAME_MAX];
    rt_uint8_t  type;
    rt_uint8_t  flags;
    rt_list_t   list;

    rt_list_t   tlist;

    /* stack point and entry */
    void       *sp;
    void       *entry;
    void       *parameter;
    void       *stack_addr;
    rt_uint32_t stack_size;

    /* error code */
    rt_err_t    error;
    rt_uint8_t  stat;

    /* priority */
    rt_uint8_t  current_priority;
    rt_uint8_t  init_priority;
    rt_uint32_t number_mask;

    /* event */
    rt_uint32_t event_set;
    rt_uint8_t  event_info;

    /* tick slice */
    rt_ubase_t  init_tick;
    rt_ubase_t  remaining_tick;

    /* built-in thread timer */
    struct rt_timer thread_timer;

    void (*cleanup)(struct rt_thread *tid);
    rt_uint32_t user_data;
};
typedef struct rt_thread *rt_thread_t;

/* IPC flags and structures */
#define RT_IPC_FLAG_FIFO                0x00
#define RT_IPC_FLAG_PRIO                0x01

#define RT_IPC_CMD_UNKNOWN              0x00
#define RT_IPC_CMD_RESET                0x01

#define RT_WAITING_FOREVER              -1
#define RT_WAITING_NO                   0

struct rt_ipc_object
{
    struct rt_object parent;
    rt_list_t        suspend_thread;
};

#ifdef RT_USING_SEMAPHORE
struct rt_semaphore
{
    struct rt_ipc_object parent;
    rt_uint16_t          value;
    rt_uint16_t          reserved;
};
typedef struct rt_semaphore *rt_sem_t;
#endif

#ifdef RT_USING_MUTEX
struct rt_mutex
{
    struct rt_ipc_object parent;
    rt_uint16_t          value;
    rt_uint8_t           original_priority;
    rt_uint8_t           hold;
    struct rt_thread    *owner;
};
typedef struct rt_mutex *rt_mutex_t;
#endif

#ifdef RT_USING_MAILBOX
struct rt_mailbox
{
    struct rt_ipc_object parent;
    rt_ubase_t          *msg_pool;
    rt_uint16_t          size;
    rt_uint16_t          entry;
    rt_uint16_t          in_offset;
    rt_uint16_t          out_offset;
    rt_list_t            suspend_sender_thread;
};
typedef struct rt_mailbox *rt_mb_t;
#endif

#ifdef __cplusplus
}
#endif

#endif /* __RT_DEF_H__ */

