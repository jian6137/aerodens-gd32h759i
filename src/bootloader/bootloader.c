/*!
    \file    bootloader.c
    \brief   AeroDENS Bootloader Module (Splash Logo, Hardware/Partition Self-Test, Boot Animation, OS Jump)
*/

#include "bootloader.h"
#include "gd32h759i_eval.h"
#include "gd32h759i_lcd.h"
#include "exmc_sdram.h"
#include "systick.h"
#include <stdio.h>

typedef void (*p_app_entry_t)(void);

static uint32_t s_splash_start_ms = 0U;
static uint32_t s_boot_mode = BOOT_MODE_NORMAL;

int bootloader_show_logo_by_offset(uint32_t offset, uint32_t size, uint8_t format)
{
    int rc = logo_decode_from_offset(
        (uintptr_t)LOGO_PARTITION_FLASH_ADDR,
        offset,
        size,
        format,
        (uint16_t *)DISPLAY_FRAMEBUFFER,
        (size_t)(LCD_WIDTH * LCD_HEIGHT)
    );
    if (rc == 0) {
        gd32h759i_lcd_flush();
    }
    return rc;
}

int bootloader_show_logo_by_id(uint32_t logo_id)
{
    int rc = logo_decode_by_id(
        (uintptr_t)LOGO_PARTITION_FLASH_ADDR,
        logo_id,
        (uint16_t *)DISPLAY_FRAMEBUFFER,
        (size_t)(LCD_WIDTH * LCD_HEIGHT)
    );
    if (rc == 0) {
        gd32h759i_lcd_flush();
    }
    return rc;
}

boot_status_t bootloader_early_display_init(void)
{
    int rc = bootloader_show_logo_by_offset(LOGO_OFFSET_BOOT, LOGO_SIZE_BOOT, LOGO_FORMAT_BOOT);
    if (rc != 0) {
        gd32h759i_lcd_fill(0xFFFFU);
    }

    gd32h759i_lcd_init();
    gd32h759i_lcd_flush();

    s_splash_start_ms = systick_get_ms();

    const logo_pack_header_t *hdr = (const logo_pack_header_t *)LOGO_PARTITION_FLASH_ADDR;
    if (hdr->magic != LOGO_PACK_MAGIC) {
        return BOOT_ERR_LOGO_MISSING;
    }

    return BOOT_OK;
}

static int verify_sdram_hardware(void)
{
    exmc_synchronous_dynamic_ram_init(EXMC_SDRAM_DEVICE0);

    volatile uint32_t *sdram = (volatile uint32_t *)SDRAM_DEVICE0_ADDR;
    sdram[0] = 0xA55AA55AUL;
    sdram[1] = 0x5AA55AA5UL;
    __DSB();

    if (sdram[0] != 0xA55AA55AUL || sdram[1] != 0x5AA55AA5UL) {
        return -1;
    }
    return 0;
}

static boot_status_t verify_system_partition(uint32_t sys_addr)
{
    const uint32_t *vector_table = (const uint32_t *)sys_addr;
    uint32_t initial_msp   = vector_table[0];
    uint32_t reset_handler = vector_table[1];

    if (initial_msp == 0xFFFFFFFFUL || initial_msp == 0x00000000UL ||
        reset_handler == 0xFFFFFFFFUL || reset_handler == 0x00000000UL) {
        return BOOT_ERR_SYS_MISSING;
    }

    uint8_t valid_dtcm = (initial_msp >= 0x20000000UL && initial_msp <= 0x20020000UL);
    uint8_t valid_axi  = (initial_msp >= 0x24000000UL && initial_msp <= 0x24080000UL);
    if (!valid_dtcm && !valid_axi) {
        return BOOT_ERR_SYS_INVALID_SP;
    }

    if ((reset_handler & 1UL) == 0UL ||
        reset_handler < sys_addr ||
        reset_handler >= LOGO_PARTITION_FLASH_ADDR) {
        return BOOT_ERR_SYS_INVALID_PC;
    }

    return BOOT_OK;
}

boot_status_t bootloader_check_system(void)
{
    boot_status_t status = BOOT_OK;

    printf("[BOOT] Starting background hardware & partition checks...\r\n");

    const logo_pack_header_t *hdr = (const logo_pack_header_t *)LOGO_PARTITION_FLASH_ADDR;
    if (hdr->magic != LOGO_PACK_MAGIC) {
        printf("[BOOT] ERROR: Invalid logo.bin magic at 0x%08lX\r\n",
               (unsigned long)LOGO_PARTITION_FLASH_ADDR);
        status = BOOT_ERR_LOGO_MISSING;
    } else {
        printf("[BOOT] logo.bin OK (%u images at 0x%08lX)\r\n",
               (unsigned int)hdr->image_count, (unsigned long)LOGO_PARTITION_FLASH_ADDR);
    }

    if (status == BOOT_OK) {
        if (verify_sdram_hardware() != 0) {
            printf("[BOOT] ERROR: 32 MB EXMC SDRAM verification failed!\r\n");
            status = BOOT_ERR_SDRAM_FAULT;
        } else {
            printf("[BOOT] 32 MB EXMC SDRAM OK at 0xC0000000\r\n");
        }
    }

    if (status == BOOT_OK) {
        status = verify_system_partition(SYSTEM_PARTITION_FLASH_ADDR);
        if (status != BOOT_OK) {
            printf("[BOOT] ERROR: System partition check failed at 0x%08lX (code %d)\r\n",
                   (unsigned long)SYSTEM_PARTITION_FLASH_ADDR, (int)status);
        } else {
            printf("[BOOT] System partition valid at 0x%08lX\r\n",
                   (unsigned long)SYSTEM_PARTITION_FLASH_ADDR);
        }
    }

    /* Hardware buttons: USER (PF8, pull-up), TAMPER (PC13, pull-up), WAKEUP (PA0, pull-down) */
    gd_eval_key_init(KEY_USER, KEY_MODE_GPIO);
    gpio_mode_set(USER_KEY_GPIO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, USER_KEY_PIN);

    gd_eval_key_init(KEY_TAMPER, KEY_MODE_GPIO);
    gpio_mode_set(TAMPER_KEY_GPIO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, TAMPER_KEY_PIN);

    gd_eval_key_init(KEY_WAKEUP, KEY_MODE_GPIO);
    gpio_mode_set(WAKEUP_KEY_GPIO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, WAKEUP_KEY_PIN);

    gd_eval_led_off(LED2);

    uint32_t key_held_ms = 0U;
    uint32_t last_diag_ms = 0U;
    s_boot_mode = BOOT_MODE_NORMAL;

    printf("[BOOT] Splash active (3s). Hold USER (K4), TAMPER (K3), or WAKEUP (K2) to enter OS-DEBUG mode...\r\n");

    while ((systick_get_ms() - s_splash_start_ms) < BOOTLOADER_SPLASH_TIME_MS) {
        uint8_t u_pressed = (gd_eval_key_state_get(KEY_USER) == RESET);
        uint8_t t_pressed = (gd_eval_key_state_get(KEY_TAMPER) == RESET);
        uint8_t w_pressed = (gd_eval_key_state_get(KEY_WAKEUP) == SET);
        uint8_t any_pressed = (u_pressed || t_pressed || w_pressed);

        if (any_pressed) {
            key_held_ms += 10U;
            if (key_held_ms >= BOOTLOADER_KEY_HOLD_TIME_MS && s_boot_mode != BOOT_MODE_OS_DEBUG) {
                s_boot_mode = BOOT_MODE_OS_DEBUG;
                gd_eval_led_on(LED2);
                printf("\r\n>>> [BOOT] Key hold confirmed (%lu ms: USER=%u, TAMPER=%u, WAKEUP=%u) -> OS-DEBUG mode ARMED! (LED2 ON) <<<\r\n",
                       (unsigned long)key_held_ms, u_pressed, t_pressed, w_pressed);
            }
        } else {
            if (s_boot_mode != BOOT_MODE_OS_DEBUG) {
                key_held_ms = 0U;
            }
        }

        if ((systick_get_ms() - last_diag_ms) >= 500U) {
            last_diag_ms = systick_get_ms();
            printf("[BOOT] Splash %4lu/%u ms | USER[PF8]=%s TAMPER[PC13]=%s WAKEUP[PA0]=%s | Mode: %s\r\n",
                   (unsigned long)(systick_get_ms() - s_splash_start_ms),
                   (unsigned int)BOOTLOADER_SPLASH_TIME_MS,
                   u_pressed ? "PRESSED" : "IDLE",
                   t_pressed ? "PRESSED" : "IDLE",
                   w_pressed ? "PRESSED" : "IDLE",
                   (s_boot_mode == BOOT_MODE_OS_DEBUG) ? "OS-DEBUG (ARMED)" : "NORMAL");
        }

        delay_1ms(10U);
    }

    return status;
}

uint32_t bootloader_get_boot_mode(void)
{
    return s_boot_mode;
}

int bootloader_play_animation(void)
{
    const anim_pack_header_t *hdr = (const anim_pack_header_t *)ANIM_PARTITION_FLASH_ADDR;
    if (hdr->magic != ANIM_PACK_MAGIC || hdr->frame_count == 0U) {
        printf("[BOOT] No anim.bin found at 0x%08lX (skipping boot animation)\r\n",
               (unsigned long)ANIM_PARTITION_FLASH_ADDR);
        return -1;
    }

    const anim_frame_entry_t *frames = (const anim_frame_entry_t *)(
        ANIM_PARTITION_FLASH_ADDR + sizeof(anim_pack_header_t)
    );
    uint32_t default_delay_ms = (hdr->fps > 0U) ? (1000U / hdr->fps) : 40U;

    printf("[BOOT] Playing anim.bin (%u frames, %lu ms total)...\r\n",
           (unsigned int)hdr->frame_count, (unsigned long)hdr->duration_ms);

    for (uint32_t i = 0U; i < hdr->frame_count; ++i) {
        uint32_t t0 = systick_get_ms();
        uint32_t target_delay_ms = (frames[i].delay_ms > 0U) ? frames[i].delay_ms : default_delay_ms;

        (void)anim_decode_frame(
            (uintptr_t)ANIM_PARTITION_FLASH_ADDR,
            frames[i].offset,
            frames[i].size,
            hdr->format,
            (uint16_t *)DISPLAY_FRAMEBUFFER,
            (size_t)(LCD_WIDTH * LCD_HEIGHT)
        );

        uint32_t vblank_timeout = 50000U;
        while (tli_flag_get(TLI_FLAG_VDE) != RESET && vblank_timeout-- > 0U) {
        }
        gd32h759i_lcd_flush();

        uint32_t elapsed = systick_get_ms() - t0;
        if (elapsed < target_delay_ms) {
            delay_1ms(target_delay_ms - elapsed);
        }
    }

    return 0;
}

void bootloader_jump_to_system(uint32_t system_base_addr, uint32_t boot_mode)
{
    const uint32_t *vector_table = (const uint32_t *)system_base_addr;
    uint32_t app_msp   = vector_table[0];
    uint32_t app_entry = vector_table[1];

    *(volatile uint32_t *)BOOT_PARAM_MAGIC_ADDR = boot_mode;
    __DSB();

    printf("[BOOT] Jumping to OS partition at 0x%08lX (MSP=0x%08lX, BootMode=0x%08lX)...\r\n",
           (unsigned long)app_entry, (unsigned long)app_msp, (unsigned long)boot_mode);

    __disable_irq();

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL  = 0U;

    SCB_CleanInvalidateDCache();
    SCB_InvalidateICache();

    SCB->VTOR = system_base_addr;
    __DSB();
    __ISB();

    __set_MSP(app_msp);
    p_app_entry_t jump_to_app = (p_app_entry_t)app_entry;
    jump_to_app();

    while (1) {
    }
}

void bootloader_enter_error_screen(boot_status_t err_code)
{
    printf("[BOOT] Entering error screen (status=%d) -> Loading error.png from logo.bin offset 0x%08lX\r\n",
           (int)err_code, (unsigned long)LOGO_OFFSET_ERROR);

    (void)bootloader_show_logo_by_offset(LOGO_OFFSET_ERROR, LOGO_SIZE_ERROR, LOGO_FORMAT_ERROR);

    while (1) {
        gd_eval_led_toggle(LED2);
        delay_1ms(250U);
    }
}

