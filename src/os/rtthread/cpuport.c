/*
 * RT-Thread ARM Cortex-M7 CPU Port (src/os/rtthread/cpuport.c)
 * Supports Cortex-M7 with VFPv5-D16 Hardware FPU (-mfpu=fpv5-d16 -mfloat-abi=hard)
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtthread.h>
#include <rthw.h>
#include "gd32h7xx.h"

#if defined(__VFP_FP__) && !defined(__SOFTFP__)
#define USE_FPU   1
#else
#define USE_FPU   0
#endif

/* Exception frame pushed/popped automatically by Cortex-M7 hardware */
struct exception_stack_frame
{
    rt_uint32_t r0;
    rt_uint32_t r1;
    rt_uint32_t r2;
    rt_uint32_t r3;
    rt_uint32_t r12;
    rt_uint32_t lr;
    rt_uint32_t pc;
    rt_uint32_t psr;
};

/* Software-saved context frame pushed/popped by PendSV_Handler */
struct stack_frame
{
#if USE_FPU
    rt_uint32_t flag;
#endif
    /* r4 - r11 register */
    rt_uint32_t r4;
    rt_uint32_t r5;
    rt_uint32_t r6;
    rt_uint32_t r7;
    rt_uint32_t r8;
    rt_uint32_t r9;
    rt_uint32_t r10;
    rt_uint32_t r11;

    struct exception_stack_frame exception_stack_frame;
};

/* Variables used by context_gcc.S */
volatile rt_ubase_t  rt_interrupt_from_thread = 0;
volatile rt_ubase_t  rt_interrupt_to_thread   = 0;
volatile rt_uint32_t rt_thread_switch_interrupt_flag = 0;

rt_uint8_t *rt_hw_stack_init(void       *tentry,
                             void       *parameter,
                             rt_uint8_t *stack_addr,
                             void       *texit)
{
    struct stack_frame *stack_frame;
    rt_uint8_t         *stk;
    unsigned long       i;

    stk  = stack_addr + sizeof(rt_uint32_t);
    stk  = (rt_uint8_t *)RT_ALIGN_DOWN((rt_uint32_t)stk, 8);
    stk -= sizeof(struct stack_frame);

    stack_frame = (struct stack_frame *)stk;

    for (i = 0; i < sizeof(struct stack_frame) / sizeof(rt_uint32_t); i++) {
        ((rt_uint32_t *)stack_frame)[i] = 0xdeadbeefUL;
    }

    stack_frame->exception_stack_frame.r0  = (unsigned long)parameter;
    stack_frame->exception_stack_frame.r1  = 0;
    stack_frame->exception_stack_frame.r2  = 0;
    stack_frame->exception_stack_frame.r3  = 0;
    stack_frame->exception_stack_frame.r12 = 0;
    stack_frame->exception_stack_frame.lr  = (unsigned long)texit;
    stack_frame->exception_stack_frame.pc  = (unsigned long)tentry;
    stack_frame->exception_stack_frame.psr = 0x01000000UL;

#if USE_FPU
    stack_frame->flag = 0;
#endif

    return stk;
}

void rt_hw_cpu_reset(void)
{
    NVIC_SystemReset();
}

