/*!
    \file    include/boot_param.h
    \brief   Shared Boot Parameters & Hand-off Protocol between Bootloader & OS
             Project AeroDENS - GD32H759I-EVAL
*/

#ifndef AERODENS_BOOT_PARAM_H
#define AERODENS_BOOT_PARAM_H

#include <stdint.h>

#define BOOT_PARAM_MAGIC_ADDR       0x2001FFF0UL

#define BOOT_MODE_NORMAL            0xAEB00001UL
#define BOOT_MODE_OS_DEBUG          0xAEB0DEDBUL

#endif /* AERODENS_BOOT_PARAM_H */

