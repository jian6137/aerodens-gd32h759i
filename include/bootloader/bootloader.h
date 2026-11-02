/*!
    \file    bootloader.h
    \brief   AeroDENS Bootloader Module (Splash Logo, Hardware/Partition Self-Test, Boot Animation, OS Jump)
*/

#ifndef AERODENS_BOOTLOADER_H
#define AERODENS_BOOTLOADER_H

#include <stdint.h>
#include "gd32h7xx.h"
#include "logo_offsets.h"
#include "anim_offsets.h"
#include "boot_param.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BOOTLOADER_FLASH_BASE         0x08000000UL
#define SYSTEM_PARTITION_FLASH_ADDR   0x08010000UL
#define LOGO_PARTITION_FLASH_ADDR     0x08180000UL
#define ANIM_PARTITION_FLASH_ADDR     0x08200000UL

#define BOOTLOADER_SPLASH_TIME_MS     3000U
#define BOOTLOADER_KEY_HOLD_TIME_MS   500U

typedef enum {
    BOOT_OK = 0,
    BOOT_ERR_LOGO_MISSING   = -1,
    BOOT_ERR_SDRAM_FAULT    = -2,
    BOOT_ERR_SYS_MISSING    = -3,
    BOOT_ERR_SYS_INVALID_SP = -4,
    BOOT_ERR_SYS_INVALID_PC = -5
} boot_status_t;

boot_status_t bootloader_early_display_init(void);
int bootloader_show_logo_by_id(uint32_t logo_id);
int bootloader_show_logo_by_offset(uint32_t offset, uint32_t size, uint8_t format);
boot_status_t bootloader_check_system(void);
uint32_t bootloader_get_boot_mode(void);
int bootloader_play_animation(void);
void bootloader_jump_to_system(uint32_t system_base_addr, uint32_t boot_mode);
void bootloader_enter_error_screen(boot_status_t err_code);

#ifdef __cplusplus
}
#endif

#endif /* AERODENS_BOOTLOADER_H */
