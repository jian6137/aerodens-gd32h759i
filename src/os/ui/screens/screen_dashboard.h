/*!
    \file    src/os/ui/screens/screen_dashboard.h
    \brief   Screen 4: AeroDENS Active Dashboard & Draggable Bottom Sheet
*/

#ifndef SCREEN_DASHBOARD_H
#define SCREEN_DASHBOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

lv_obj_t *screen_dashboard_create(void);
void screen_dashboard_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* SCREEN_DASHBOARD_H */
