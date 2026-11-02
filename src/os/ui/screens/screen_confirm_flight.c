/*!
    \file    src/os/ui/screen_confirm_flight.c
    \brief   Confirm Flight Summary Screen Implementation
             Displays large circular Airline logo, IATA -> separator SVG -> IATA,
             flight number on top of separator, duration below separator, and Confirm button.
*/

#include "screen_confirm_flight.h"
#include "ui_manager.h"
#include "ui_theme.h"
#include "ui_topbar.h"
#include "flight_store.h"
#include "configs/allowed_flights.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static void topbar_back_cb(void)
{
    ui_manager_navigate_to(SCREEN_NEW_FLIGHT, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
}

static void confirm_btn_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_manager_navigate_to(SCREEN_CONNECT, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }
}

/* Parse "H:MM AM" or "HH:MM PM" into minutes from midnight (0..1439) */
static int parse_time_minutes(const char *t_str)
{
    if (t_str == NULL) {
        return -1;
    }

    int hr = 0;
    int mn = 0;
    char ampm[8] = {0};

    if (sscanf(t_str, "%d:%d %7s", &hr, &mn, ampm) < 2) {
        return -1;
    }

    bool is_pm = (toupper((unsigned char)ampm[0]) == 'P');
    bool is_am = (toupper((unsigned char)ampm[0]) == 'A');

    if (is_pm && hr < 12) {
        hr += 12;
    } else if (is_am && hr == 12) {
        hr = 0;
    }

    return (hr * 60) + mn;
}

static void format_flight_duration(const char *dep_str, const char *arr_str, char *out_buf, size_t out_len)
{
    int dep_min = parse_time_minutes(dep_str);
    int arr_min = parse_time_minutes(arr_str);

    if (dep_min < 0 || arr_min < 0) {
        snprintf(out_buf, out_len, "1h 30m");
        return;
    }

    int diff = arr_min - dep_min;
    if (diff <= 0) {
        diff += 24 * 60;
    }

    int hrs = diff / 60;
    int mins = diff % 60;
    snprintf(out_buf, out_len, "%dh %dm", hrs, mins);
}

lv_obj_t *screen_confirm_flight_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(UI_COLOR_HEX_SURFACE), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    (void)ui_topbar_create(scr, "Confirm Flight", true, topbar_back_cb, NULL);

    const flight_record_t *f = ui_manager_get_active_flight();
    const char *flight_no = (f != NULL && f->flight_no[0] != '\0') ? f->flight_no : "5J 383";
    const char *origin    = (f != NULL && f->origin[0] != '\0')    ? f->origin    : "MNL";
    const char *dest      = (f != NULL && f->dest[0] != '\0')      ? f->dest      : "CGY";
    const char *dep_time  = (f != NULL && f->depart_time[0] != '\0') ? f->depart_time : "4:10 AM";
    const char *arr_time  = (f != NULL && f->arrive_time[0] != '\0') ? f->arrive_time : "5:40 AM";

    const airline_info_t *airline = airline_get_info_by_flight_no(flight_no);

    lv_obj_t *icon_circle = lv_obj_create(scr);
    lv_obj_set_size(icon_circle, 76, 76);
    lv_obj_align(icon_circle, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_radius(icon_circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(icon_circle, lv_color_hex(UI_COLOR_HEX_SURFACE_HIGH), 0);
    lv_obj_set_style_border_color(icon_circle, lv_color_hex(UI_COLOR_HEX_OUTLINE), 0);
    lv_obj_set_style_border_width(icon_circle, 1, 0);
    lv_obj_set_style_pad_all(icon_circle, 0, 0);
    lv_obj_clear_flag(icon_circle, LV_OBJ_FLAG_SCROLLABLE);

    if (airline != NULL && airline->icon_lg != NULL) {
        lv_obj_t *icon_img = lv_img_create(icon_circle);
        lv_img_set_src(icon_img, airline->icon_lg);
        lv_obj_center(icon_img);
    } else {
        lv_obj_t *code_lbl = lv_label_create(icon_circle);
        lv_label_set_text(code_lbl, (airline != NULL) ? airline->code : "FL");
        lv_obj_set_style_text_font(code_lbl, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(code_lbl, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
        lv_obj_center(code_lbl);
    }

    lv_obj_t *sep_img = lv_img_create(scr);
    lv_img_set_src(sep_img, &img_ic_flight_separator);
    lv_obj_align(sep_img, LV_ALIGN_TOP_MID, 0, 162);

    lv_obj_t *lbl_flight_no = lv_label_create(scr);
    lv_label_set_text(lbl_flight_no, flight_no);
    lv_obj_set_style_text_font(lbl_flight_no, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_flight_no, lv_color_hex(UI_COLOR_HEX_TEXT_SECONDARY), 0);
    lv_obj_align_to(lbl_flight_no, sep_img, LV_ALIGN_OUT_TOP_MID, 0, -4);

    char dur_buf[24];
    format_flight_duration(dep_time, arr_time, dur_buf, sizeof(dur_buf));
    lv_obj_t *lbl_duration = lv_label_create(scr);
    lv_label_set_text(lbl_duration, dur_buf);
    lv_obj_set_style_text_font(lbl_duration, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_duration, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
    lv_obj_align_to(lbl_duration, sep_img, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);

    lv_obj_t *lbl_origin = lv_label_create(scr);
    lv_label_set_text(lbl_origin, origin);
    lv_obj_set_style_text_font(lbl_origin, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_origin, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
    lv_obj_align_to(lbl_origin, sep_img, LV_ALIGN_OUT_LEFT_MID, -14, 0);

    lv_obj_t *lbl_dest = lv_label_create(scr);
    lv_label_set_text(lbl_dest, dest);
    lv_obj_set_style_text_font(lbl_dest, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_dest, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
    lv_obj_align_to(lbl_dest, sep_img, LV_ALIGN_OUT_RIGHT_MID, 14, 0);

    lv_obj_t *btn_confirm = lv_btn_create(scr);
    lv_obj_add_style(btn_confirm, ui_style_btn_primary(), 0);
    lv_obj_set_size(btn_confirm, 118, 36);
    lv_obj_set_pos(btn_confirm, 346, 224);
    lv_obj_add_event_cb(btn_confirm, confirm_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_confirm = lv_label_create(btn_confirm);
    lv_label_set_text(lbl_confirm, "Confirm");
    lv_obj_set_style_text_font(lbl_confirm, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_confirm);

    return scr;
}
