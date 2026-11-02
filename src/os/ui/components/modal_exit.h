/*!
    \file    src/os/ui/modal_exit.h
    \brief   Exit & Save Telemetry Confirmation Modal Dialog
*/

#ifndef MODAL_EXIT_H
#define MODAL_EXIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/* Show the exit confirmation modal over parent screen */
void modal_exit_show(lv_obj_t *parent_scr);

/* Hide/close the exit modal dialog */
void modal_exit_hide(void);

#ifdef __cplusplus
}
#endif

#endif /* MODAL_EXIT_H */

