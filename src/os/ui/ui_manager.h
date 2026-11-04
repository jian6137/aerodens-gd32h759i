/*!
    \file    src/os/ui/ui_manager.h
    \brief   AeroDENS Screen Router & Application Lifecycle Manager
*/

#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "flight_store.h"

typedef enum {
    SCREEN_LAUNCHER = 0,
    SCREEN_SETUP_RTCLOCK = 1,
    SCREEN_HOME = 2,
    SCREEN_NEW_FLIGHT = 3,
    SCREEN_CONFIRM_FLIGHT = 4,
    SCREEN_CONNECT = 5,
    SCREEN_DASHBOARD = 6,
    SCREEN_FLIGHT_INFO = 7,
    SCREEN_SD_REMOVED = 8
} screen_id_t;

/* Initialize UI Manager, Material 3 Theme, and launch the home screen */
void ui_manager_init(void);

/* Navigate between screens with smooth Material 3 slide transitions */
void ui_manager_navigate_to(screen_id_t screen_id, lv_scr_load_anim_t anim_type);

/* Set the currently active flight session */
void ui_manager_set_active_flight(const flight_record_t *flight);

/* Get the currently active flight session */
flight_record_t *ui_manager_get_active_flight(void);

/* Periodic ticker update (called by LVGL loop to update live clock, SD card state, etc.) */
void ui_manager_update(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_MANAGER_H */
