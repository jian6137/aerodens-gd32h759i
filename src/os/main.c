/*!
    \file    src/os/main.c
    \brief   AeroDENS OS Partition Main Entrypoint
*/

#include "app_main.h"
#include "os_debug.h"
#include "boot_param.h"
#include "gd32h7xx.h"
#include "gd32h759i_eval.h"
#include <rtthread.h>
#include <stdio.h>

void prvGetRegistersFromStack(uint32_t *pulFaultStackAddress)
{
    uint32_t stack_val = (uint32_t)pulFaultStackAddress;
    uint8_t valid_stack = (stack_val >= 0x20000000UL && stack_val <= 0x24080000UL);

    rt_kprintf("\r\n================ [HARD FAULT DETECTED] ================\r\n");
    if (valid_stack) {
        volatile uint32_t r0  = pulFaultStackAddress[0];
        volatile uint32_t r1  = pulFaultStackAddress[1];
        volatile uint32_t r2  = pulFaultStackAddress[2];
        volatile uint32_t r3  = pulFaultStackAddress[3];
        volatile uint32_t r12 = pulFaultStackAddress[4];
        volatile uint32_t lr  = pulFaultStackAddress[5];
        volatile uint32_t pc  = pulFaultStackAddress[6];
        volatile uint32_t psr = pulFaultStackAddress[7];

        rt_kprintf("  R0  = 0x%08lX   R1  = 0x%08lX\r\n", (unsigned long)r0, (unsigned long)r1);
        rt_kprintf("  R2  = 0x%08lX   R3  = 0x%08lX\r\n", (unsigned long)r2, (unsigned long)r3);
        rt_kprintf("  R12 = 0x%08lX   LR  = 0x%08lX\r\n", (unsigned long)r12, (unsigned long)lr);
        rt_kprintf("  PC  = 0x%08lX   PSR = 0x%08lX\r\n", (unsigned long)pc, (unsigned long)psr);
    } else {
        rt_kprintf("  Invalid Stack Pointer: 0x%08lX\r\n", (unsigned long)stack_val);
    }
    rt_kprintf("  CFSR= 0x%08lX   HFSR= 0x%08lX   MMFAR=0x%08lX\r\n",
               (unsigned long)SCB->CFSR, (unsigned long)SCB->HFSR, (unsigned long)SCB->MMFAR);
    rt_kprintf("  BFAR= 0x%08lX\r\n", (unsigned long)SCB->BFAR);
    rt_kprintf("========================================================\r\n");

    while (1) {
        gd_eval_led_toggle(LED2);
        for (volatile int i = 0; i < 3000000; i++);
    }
}

__attribute__((naked)) void HardFault_Handler(void)
{
    __asm volatile(
        "tst lr, #4 \n"
        "ite eq \n"
        "mrseq r0, msp \n"
        "mrsne r0, psp \n"
        "b prvGetRegistersFromStack \n"
    );
}

__attribute__((naked)) void MemManage_Handler(void)
{
    __asm volatile(
        "tst lr, #4 \n"
        "ite eq \n"
        "mrseq r0, msp \n"
        "mrsne r0, psp \n"
        "b prvGetRegistersFromStack \n"
    );
}

__attribute__((naked)) void BusFault_Handler(void)
{
    __asm volatile(
        "tst lr, #4 \n"
        "ite eq \n"
        "mrseq r0, msp \n"
        "mrsne r0, psp \n"
        "b prvGetRegistersFromStack \n"
    );
}

__attribute__((naked)) void UsageFault_Handler(void)
{
    __asm volatile(
        "tst lr, #4 \n"
        "ite eq \n"
        "mrseq r0, msp \n"
        "mrsne r0, psp \n"
        "b prvGetRegistersFromStack \n"
    );
}

int main(void)
{
    SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_MEMFAULTENA_Msk;

    volatile uint32_t *p_param = (volatile uint32_t *)BOOT_PARAM_MAGIC_ADDR;
    uint32_t boot_mode = *p_param;
    *p_param = 0U;

    rt_kprintf("\n[OS] Bootloader Hand-off: BootMode=0x%08lX\n", (unsigned long)boot_mode);

    if (boot_mode != BOOT_MODE_NORMAL && boot_mode != BOOT_MODE_OS_DEBUG) {
        gd_eval_key_init(KEY_USER, KEY_MODE_GPIO);
        gpio_mode_set(USER_KEY_GPIO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, USER_KEY_PIN);

        gd_eval_key_init(KEY_TAMPER, KEY_MODE_GPIO);
        gpio_mode_set(TAMPER_KEY_GPIO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, TAMPER_KEY_PIN);

        gd_eval_key_init(KEY_WAKEUP, KEY_MODE_GPIO);
        gpio_mode_set(WAKEUP_KEY_GPIO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, WAKEUP_KEY_PIN);

        uint8_t u_pressed = (gd_eval_key_state_get(KEY_USER) == RESET);
        uint8_t t_pressed = (gd_eval_key_state_get(KEY_TAMPER) == RESET);
        uint8_t w_pressed = (gd_eval_key_state_get(KEY_WAKEUP) == SET);

        if (u_pressed || t_pressed || w_pressed) {
            rt_kprintf("[OS] Standalone boot: Button held (USER=%u, TAMPER=%u, WAKEUP=%u) -> OS-DEBUG mode\n",
                       u_pressed, t_pressed, w_pressed);
            boot_mode = BOOT_MODE_OS_DEBUG;
        } else {
            rt_kprintf("[OS] Standalone boot: All buttons released -> Normal Application\n");
            boot_mode = BOOT_MODE_NORMAL;
        }
    }

    if (boot_mode == BOOT_MODE_OS_DEBUG) {
        rt_kprintf("[OS] Launching OS-DEBUG Mode...\n");
        os_debug_start();
    } else {
        rt_kprintf("[OS] Launching AeroDENS Application...\n");
        app_main_start();
    }

    return 0;
}
