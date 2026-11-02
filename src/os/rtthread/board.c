/*
 * RT-Thread Board Support Package for GD32H759I-EVAL OS Partition (src/os/rtthread/board.c)
 * Relocates Vector Table to 0x08010000, configures 600 MHz Cortex-M7 Cache + 1000Hz SysTick,
 * initializes 64 KB RT-Thread Heap in AXI SRAM, and routes rt_kprintf to USART0.
 */

#include <rthw.h>
#include <rtthread.h>
#include "gd32h7xx.h"
#include "gd32h759i_eval.h"
#include "systick.h"
#include "bsp_mpu.h"

#define OS_PARTITION_VECTOR_BASE  0x08010000UL

#if defined(RT_USING_HEAP)
static RT_ALIGN(RT_ALIGN_SIZE) rt_uint8_t rt_heap[RT_HEAP_SIZE];
#endif

/*
 * Hook into GD32H759I_EVAL's SysTick_Handler (called every 1ms at 1000 Hz)
 */
void systick_user_callback(void)
{
    if (rt_thread_self() != RT_NULL) {
        rt_interrupt_enter();
        rt_tick_increase();
        rt_interrupt_leave();
    }
}

void rt_hw_board_init(void)
{
    SCB->VTOR = OS_PARTITION_VECTOR_BASE;
    __DSB();
    __ISB();

    mpu_config();
    if ((SCB->CCR & SCB_CCR_IC_Msk) == 0U) {
        SCB_EnableICache();
    }
    if ((SCB->CCR & SCB_CCR_DC_Msk) == 0U) {
        SCB_EnableDCache();
    }

    systick_config();

    gd_eval_led_init(LED2);
    gd_eval_com_init(EVAL_COM0);

#if defined(RT_USING_HEAP)
    rt_system_heap_init((void *)rt_heap, (void *)(rt_heap + sizeof(rt_heap)));
#endif

#ifdef RT_USING_COMPONENTS_INIT
    rt_components_board_init();
#endif
}

void rt_hw_console_output(const char *str)
{
    while (*str != '\0') {
        if (*str == '\n') {
            usart_data_transmit(EVAL_COM0, (uint8_t)'\r');
            while (RESET == usart_flag_get(EVAL_COM0, USART_FLAG_TBE)) {
            }
        }
        usart_data_transmit(EVAL_COM0, (uint8_t)(*str++));
        while (RESET == usart_flag_get(EVAL_COM0, USART_FLAG_TBE)) {
        }
    }
}

void rt_hw_us_delay(rt_uint32_t us)
{
    rt_uint32_t ticks;
    rt_uint32_t told, tnow, tcnt = 0;
    rt_uint32_t reload = SysTick->LOAD;

    ticks = us * (reload / (1000000 / RT_TICK_PER_SECOND));
    told = SysTick->VAL;
    while (1) {
        tnow = SysTick->VAL;
        if (tnow != told) {
            if (tnow < told) {
                tcnt += told - tnow;
            } else {
                tcnt += reload - tnow + told;
            }
            told = tnow;
            if (tcnt >= ticks) {
                break;
            }
        }
    }
}

