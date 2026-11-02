/*!
    \file    src/os/app_main.h
    \brief   AeroDENS Main Application
*/

#ifndef AERODENS_APP_MAIN_H
#define AERODENS_APP_MAIN_H

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

extern rt_mutex_t g_lvgl_mutex;

void app_lvgl_lock(void);
void app_lvgl_unlock(void);
void app_main_start(void);

#ifdef __cplusplus
}
#endif

#endif /* AERODENS_APP_MAIN_H */
