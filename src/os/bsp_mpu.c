/*!
    \file    src/os/bsp_mpu.c
    \brief   MPU memory region configuration for GD32H759I
*/

#include "bsp_mpu.h"

void mpu_config(void)
{
    mpu_region_init_struct mpu_init_struct;
    mpu_region_struct_para_init(&mpu_init_struct);

    /* Disable the MPU */
    ARM_MPU_Disable();
    ARM_MPU_SetRegion(0, 0);

    /* Region 0: 4GB address space background / reserved, no access */
    mpu_init_struct.region_base_address  = 0x0U;
    mpu_init_struct.region_size          = MPU_REGION_SIZE_4GB;
    mpu_init_struct.access_permission    = MPU_AP_NO_ACCESS;
    mpu_init_struct.access_bufferable    = MPU_ACCESS_NON_BUFFERABLE;
    mpu_init_struct.access_cacheable     = MPU_ACCESS_NON_CACHEABLE;
    mpu_init_struct.access_shareable     = MPU_ACCESS_SHAREABLE;
    mpu_init_struct.region_number        = MPU_REGION_NUMBER0;
    mpu_init_struct.subregion_disable    = 0x87U;
    mpu_init_struct.instruction_exec     = MPU_INSTRUCTION_EXEC_NOT_PERMIT;
    mpu_init_struct.tex_type             = MPU_TEX_TYPE0;
    mpu_region_config(&mpu_init_struct);
    mpu_region_enable();

    /* Region 1: 1MB AXI SRAM (0x24000000 - 0x240FFFFF)
     * Configured as Normal memory, Cacheable, Non-Bufferable (TEX=0, C=1, B=0 = Write-Through) */
    mpu_init_struct.region_base_address  = 0x24000000U;
    mpu_init_struct.region_size          = MPU_REGION_SIZE_1MB;
    mpu_init_struct.access_permission    = MPU_AP_FULL_ACCESS;
    mpu_init_struct.access_bufferable    = MPU_ACCESS_NON_BUFFERABLE;
    mpu_init_struct.access_cacheable     = MPU_ACCESS_CACHEABLE;
    mpu_init_struct.access_shareable     = MPU_ACCESS_NON_SHAREABLE;
    mpu_init_struct.region_number        = MPU_REGION_NUMBER1;
    mpu_init_struct.subregion_disable    = MPU_SUBREGION_ENABLE;
    mpu_init_struct.instruction_exec     = MPU_INSTRUCTION_EXEC_PERMIT;
    mpu_init_struct.tex_type             = MPU_TEX_TYPE0;
    mpu_region_config(&mpu_init_struct);
    mpu_region_enable();

    /* Region 2: 32MB External SDRAM (0xC0000000 - 0xC1FFFFFF)
     * Configured as Cacheable, Non-Bufferable (Write-Through) */
    mpu_init_struct.region_base_address  = 0xC0000000U;
    mpu_init_struct.region_size          = MPU_REGION_SIZE_32MB;
    mpu_init_struct.access_permission    = MPU_AP_FULL_ACCESS;
    mpu_init_struct.access_bufferable    = MPU_ACCESS_NON_BUFFERABLE;
    mpu_init_struct.access_cacheable     = MPU_ACCESS_CACHEABLE;
    mpu_init_struct.access_shareable     = MPU_ACCESS_NON_SHAREABLE;
    mpu_init_struct.region_number        = MPU_REGION_NUMBER2;
    mpu_init_struct.subregion_disable    = MPU_SUBREGION_ENABLE;
    mpu_init_struct.instruction_exec     = MPU_INSTRUCTION_EXEC_NOT_PERMIT;
    mpu_init_struct.tex_type             = MPU_TEX_TYPE0;
    mpu_region_config(&mpu_init_struct);
    mpu_region_enable();

    /* Enable MPU with default memory map for privileged software access */
    ARM_MPU_Enable(MPU_MODE_PRIV_DEFAULT);
}

