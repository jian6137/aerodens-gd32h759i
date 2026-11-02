/*!
    \file    src/os/ui/screens/screen_launcher.h
    \brief   AeroDENS main launcher homescreen
*/

#ifndef SCREEN_LAUNCHER_H
#define SCREEN_LAUNCHER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

lv_obj_t *screen_launcher_create(void);
void screen_launcher_set_clock(const char *time_str);

#ifdef __cplusplus
}
#endif

#endif /* SCREEN_LAUNCHER_H */

