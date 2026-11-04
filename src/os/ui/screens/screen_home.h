/*!
    \file    src/os/ui/screens/screen_home.h
    \brief   Screen 1: AeroDENS Saved Flights List & FAB
*/

#ifndef SCREEN_HOME_H
#define SCREEN_HOME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

lv_obj_t *screen_home_create(void);
void screen_home_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* SCREEN_HOME_H */
