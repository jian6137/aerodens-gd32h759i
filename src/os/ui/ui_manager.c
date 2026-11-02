/*!
    \file    src/os/ui/ui_manager.c
    \brief   AeroDENS Screen Router & Lifecycle Manager Implementation
*/

#include "ui_manager.h"
#include "ui_theme.h"
#include "ui_topbar.h"
#include "screen_launcher.h"
#include "screen_setup_rtclock.h"
#include "screen_home.h"
#include "screen_new_flight.h"
#include "screen_confirm_flight.h"
#include "screen_connect.h"
#include "screen_dashboard.h"
#include "rtc_manager.h"
#include <stdio.h>
#include <string.h>

static screen_id_t s_curr_screen_id = SCREEN_LAUNCHER;
static flight_record_t s_active_flight;
static uint32_t s_last_clock_ms = 0U;
static uint32_t s_last_nav_tick = 0U;

void ui_manager_init(void)
{
    flight_store_init();
    rtc_manager_init();
    ui_theme_init();

    memset(&s_active_flight, 0, sizeof(s_active_flight));
    const flight_record_t *def_f = flight_store_get(0);
    if (def_f != NULL) {
        memcpy(&s_active_flight, def_f, sizeof(flight_record_t));
    }

    if (!rtc_manager_is_valid()) {
        lv_obj_t *setup_scr = screen_setup_rtclock_create();
        lv_scr_load(setup_scr);
        s_curr_screen_id = SCREEN_SETUP_RTCLOCK;
    } else {
        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
        lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);

        lv_obj_t *launcher_scr = screen_launcher_create();
        lv_scr_load_anim(launcher_scr, LV_SCR_LOAD_ANIM_FADE_IN, 500, 50, false);
        s_curr_screen_id = SCREEN_LAUNCHER;
    }
}

void ui_manager_navigate_to(screen_id_t screen_id, lv_scr_load_anim_t anim_type)
{
    uint32_t now = lv_tick_get();
    if (s_last_nav_tick != 0U && (now - s_last_nav_tick) < 350U) {
        return;
    }
    s_last_nav_tick = now;

    lv_obj_t *new_scr = NULL;

    switch (screen_id) {
    case SCREEN_LAUNCHER:
        new_scr = screen_launcher_create();
        break;
    case SCREEN_SETUP_RTCLOCK:
        new_scr = screen_setup_rtclock_create();
        break;
    case SCREEN_HOME:
        new_scr = screen_home_create();
        break;
    case SCREEN_NEW_FLIGHT:
        new_scr = screen_new_flight_create();
        break;
    case SCREEN_CONFIRM_FLIGHT:
        new_scr = screen_confirm_flight_create();
        break;
    case SCREEN_CONNECT:
        new_scr = screen_connect_create();
        break;
    case SCREEN_DASHBOARD:
        new_scr = screen_dashboard_create();
        break;
    default:
        new_scr = screen_home_create();
        break;
    }

    if (new_scr != NULL) {
        s_curr_screen_id = screen_id;
        lv_scr_load_anim(new_scr, anim_type, 200, 0, true);
    }
}

void ui_manager_set_active_flight(const flight_record_t *flight)
{
    if (flight != NULL) {
        memcpy(&s_active_flight, flight, sizeof(flight_record_t));
    }
}

flight_record_t *ui_manager_get_active_flight(void)
{
    return &s_active_flight;
}

void ui_manager_update(void)
{
    uint32_t now = lv_tick_get();
    if ((now - s_last_clock_ms) >= 1000U) {
        s_last_clock_ms = now;

        rtc_manager_second_tick();

        char clock_buf[48];
        rtc_manager_get_time_str(clock_buf, sizeof(clock_buf));
        ui_topbar_set_clock(clock_buf);
        screen_launcher_set_clock(clock_buf);
    }
}

