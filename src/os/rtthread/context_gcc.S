/*
 * RT-Thread ARM Cortex-M7 Context Switch Assembly (src/os/rtthread/context_gcc.S)
 * Supports Cortex-M7 with VFPv5-D16 Hardware FPU (-mfpu=fpv5-d16 -mfloat-abi=hard)
 * SPDX-License-Identifier: Apache-2.0
 */

.cpu cortex-m7
.syntax unified
.thumb
.text

.equ SCB_VTOR,           0xE000ED08              /* Vector Table Offset Register */
.equ NVIC_INT_CTRL,      0xE000ED04              /* Interrupt Control and State Register */
.equ NVIC_SYSPRI2,       0xE000ED20              /* System Handler Priority Register 2 (PendSV/SysTick) */
.equ NVIC_PENDSV_PRI,    0xFFFF0000              /* PendSV and SysTick priority value (lowest) */
.equ NVIC_PENDSVSET,     0x10000000              /* Value to trigger PendSV exception */

/*
 * rt_base_t rt_hw_interrupt_disable(void);
 */
.global rt_hw_interrupt_disable
.type rt_hw_interrupt_disable, %function
rt_hw_interrupt_disable:
    mrs     r0, PRIMASK
    cpsid   i
    bx      lr

/*
 * void rt_hw_interrupt_enable(rt_base_t level);
 */
.global rt_hw_interrupt_enable
.type rt_hw_interrupt_enable, %function
rt_hw_interrupt_enable:
    msr     PRIMASK, r0
    bx      lr

/*
 * void rt_hw_context_switch(rt_ubase_t from, rt_ubase_t to);
 * r0 --> from
 * r1 --> to
 */
.global rt_hw_context_switch_interrupt
.type rt_hw_context_switch_interrupt, %function
.global rt_hw_context_switch
.type rt_hw_context_switch, %function

rt_hw_context_switch_interrupt:
rt_hw_context_switch:
    /* check rt_thread_switch_interrupt_flag */
    ldr     r2, =rt_thread_switch_interrupt_flag
    ldr     r3, [r2]
    cmp     r3, #1
    beq     _reswitch
    mov     r3, #1
    str     r3, [r2]

    ldr     r2, =rt_interrupt_from_thread
    str     r0, [r2]

_reswitch:
    ldr     r2, =rt_interrupt_to_thread
    str     r1, [r2]

    ldr     r0, =NVIC_INT_CTRL
    ldr     r1, =NVIC_PENDSVSET
    str     r1, [r0]
    bx      lr

/*
 * void PendSV_Handler(void);
 * r0 --> switch from thread stack
 * r1 --> switch to thread stack
 * psr, pc, lr, r12, r3, r2, r1, r0 are pushed into [from] stack
 */
.global PendSV_Handler
.type PendSV_Handler, %function
PendSV_Handler:
    /* disable interrupt to protect context switch */
    mrs     r2, PRIMASK
    cpsid   i

    /* get rt_thread_switch_interrupt_flag */
    ldr     r0, =rt_thread_switch_interrupt_flag
    ldr     r1, [r0]
    cbz     r1, pendsv_exit         /* pendsv already handled */

    /* clear rt_thread_switch_interrupt_flag to 0 */
    mov     r1, #0x00
    str     r1, [r0]

    ldr     r0, =rt_interrupt_from_thread
    ldr     r1, [r0]
    cbz     r1, switch_to_thread    /* skip register save at the first time */

    mrs     r1, psp                 /* get from thread stack pointer */

#if defined (__VFP_FP__) && !defined(__SOFTFP__)
    tst     lr, #0x10               /* if(!EXC_RETURN[4]) */
    it      eq
    vstmdbeq r1!, {d8 - d15}        /* push FPU register s16~s31 */
#endif

    stmfd   r1!, {r4 - r11}         /* push r4 - r11 register */

#if defined (__VFP_FP__) && !defined(__SOFTFP__)
    mov     r4, #0x00               /* flag = 0 */
    tst     lr, #0x10               /* if(!EXC_RETURN[4]) */
    it      eq
    movweq  r4, #0x01               /* flag = 1 */
    stmfd   r1!, {r4}               /* push flag */
#endif

    ldr     r0, [r0]
    str     r1, [r0]                /* update from thread stack pointer */

switch_to_thread:
    ldr     r1, =rt_interrupt_to_thread
    ldr     r1, [r1]
    ldr     r1, [r1]                /* load thread stack pointer */

#if defined (__VFP_FP__) && !defined(__SOFTFP__)
    ldmfd   r1!, {r3}               /* pop flag */
#endif

    ldmfd   r1!, {r4 - r11}         /* pop r4 - r11 register */

#if defined (__VFP_FP__) && !defined(__SOFTFP__)
    cmp     r3, #0                  /* if(flag_r3 != 0) */
    it      ne
    vldmiane r1!, {d8 - d15}        /* pop FPU register s16~s31 */
#endif

    msr     psp, r1                 /* update stack pointer */

#if defined (__VFP_FP__) && !defined(__SOFTFP__)
    orr     lr, lr, #0x10           /* default EXC_RETURN[4] = 1 (no FPU frame) */
    cmp     r3, #0                  /* if(flag_r3 != 0) */
    it      ne
    bicne   lr, lr, #0x10           /* EXC_RETURN[4] = 0 (extended FPU frame) */
#endif

pendsv_exit:
    /* restore interrupt */
    msr     PRIMASK, r2

    orr     lr, lr, #0x04           /* ensure exception return uses PSP in Thread mode */
    dsb
    isb
    bx      lr

/*
 * void rt_hw_context_switch_to(rt_ubase_t to);
 * r0 --> to
 */
.global rt_hw_context_switch_to
.type rt_hw_context_switch_to, %function
rt_hw_context_switch_to:
    ldr     r1, =rt_interrupt_to_thread
    str     r0, [r1]

#if defined (__VFP_FP__) && !defined(__SOFTFP__)
    /* Clear CONTROL.FPCA so first context switch starts clean */
    mrs     r2, control
    bic     r2, r2, #0x04
    msr     control, r2
#endif

    /* set from thread to 0 */
    ldr     r1, =rt_interrupt_from_thread
    mov     r0, #0x0
    str     r0, [r1]

    /* set interrupt flag to 1 */
    ldr     r1, =rt_thread_switch_interrupt_flag
    mov     r0, #1
    str     r0, [r1]

    /* set the PendSV and SysTick exception priority */
    ldr     r0, =NVIC_SYSPRI2
    ldr     r1, =NVIC_PENDSV_PRI
    ldr.w   r2, [r0,#0x00]          /* read */
    orr     r1, r1, r2              /* modify */
    str     r1, [r0]                /* write-back */

    /* trigger the PendSV exception (causes context switch) */
    ldr     r0, =NVIC_INT_CTRL
    ldr     r1, =NVIC_PENDSVSET
    str     r1, [r0]

    /* restore MSP from SCB->VTOR[0] */
    ldr     r0, =SCB_VTOR
    ldr     r0, [r0]
    ldr     r0, [r0]
    msr     msp, r0

    /* enable interrupts at processor level */
    cpsie   f
    cpsie   i
    dsb
    isb

    /* never reach here! */
_spin:
    b       _spin

