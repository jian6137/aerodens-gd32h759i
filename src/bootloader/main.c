/*!
    \file    src/bootloader/main.c
    \brief   AeroDENS Stage-1 Bootloader Entrypoint (0x08000000)
*/

#include "gd32h7xx.h"
#include "gd32h759i_eval.h"
#include "gd32h759i_lcd.h"
#include "systick.h"
#include "bootloader.h"
#include <stdio.h>

int main(void)
{
    SCB_EnableICache();
    SCB_EnableDCache();
    systick_config();

    (void)bootloader_early_display_init();

    gd_eval_led_init(LED2);
    gd_eval_com_init(EVAL_COM0);

    printf("\r\n=== AeroDENS Stage-1 Bootloader (0x08000000) ===\r\n");

    boot_status_t status = bootloader_check_system();
    if (status != BOOT_OK) {
        bootloader_enter_error_screen(status);
    }

    uint32_t boot_mode = bootloader_get_boot_mode();

    if (boot_mode == BOOT_MODE_OS_DEBUG) {
        printf("[BOOT] Key hold detected! Bypassing boot animation -> Launching OS-DEBUG.\r\n");
    } else {
        printf("[BOOT] Normal Boot -> Playing anim.bin boot animation...\r\n");
        (void)bootloader_play_animation();
    }

    gd32h759i_lcd_fill(0x0000U);
    bootloader_jump_to_system(SYSTEM_PARTITION_FLASH_ADDR, boot_mode);

    while (1) {
    }
}

