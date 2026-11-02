/*!
    \file    src/os/ui/ui_topbar.h
    \brief   Material 3 Top App Bar with Logo & Clock
*/

#ifndef UI_TOPBAR_H
#define UI_TOPBAR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

typedef void (*ui_topbar_back_cb_t)(void);

lv_obj_t *ui_topbar_create(lv_obj_t *parent,
                           const char *title,
                           bool show_back_btn,
                           ui_topbar_back_cb_t back_cb,
                           const char *badge_text);

void ui_topbar_set_clock(const char *time_str);

#ifdef __cplusplus
}
#endif

#endif /* UI_TOPBAR_H */

