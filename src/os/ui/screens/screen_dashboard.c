/*!
    \file    src/os/ui/screens/screen_dashboard.c
    \brief   Flight telemetry dashboard and advisory bottom sheet
*/

#include "screen_dashboard.h"
#include "modal_exit.h"
#include "ui_manager.h"
#include "ui_theme.h"
#include "ui_topbar.h"
#include "img_ac_a320.h"
#include "img_ac_a330.h"
#include <stdio.h>
#include <string.h>

#define SHEET_Y_EXPANDED    38
#define SHEET_Y_COLLAPSED   258
#define SHEET_SNAP_MIDPOINT ((SHEET_Y_EXPANDED + SHEET_Y_COLLAPSED) / 2)

static lv_obj_t *s_scr_dashboard = NULL;
static lv_obj_t *s_bottom_sheet = NULL;
static lv_obj_t *s_sheet_chevron = NULL;
static lv_obj_t *s_lbl_saf_val = NULL;

static bool s_sheet_expanded = false;
static lv_point_t s_drag_start_point;
static lv_coord_t s_drag_start_y = 0;
static bool s_is_dragging = false;

static lv_timer_t *s_countdown_timer = NULL;
static int s_countdown_sec = 5;
static lv_obj_t *s_countdown_cont = NULL;
static lv_obj_t *s_countdown_num_lbl = NULL;
static lv_obj_t *s_alerts_cont = NULL;
static lv_obj_t *s_alerts_list = NULL;
static lv_obj_t *s_metric_val_lbls[6] = { NULL };

static lv_timer_t *s_blink_timer = NULL;
static lv_obj_t *s_warning_icon = NULL;

static void anim_sheet_y_cb(void *var, int32_t val)
{
    if (var != NULL) {
        lv_obj_set_y((lv_obj_t *)var, (lv_coord_t)val);
    }
}

static void dashboard_delete_cb(lv_event_t *e)
{
    (void)e;
    if (s_countdown_timer != NULL) {
        lv_timer_del(s_countdown_timer);
        s_countdown_timer = NULL;
    }
    if (s_blink_timer != NULL) {
        lv_timer_del(s_blink_timer);
        s_blink_timer = NULL;
    }
    if (s_bottom_sheet != NULL) {
        lv_anim_del(s_bottom_sheet, anim_sheet_y_cb);
    }
    s_scr_dashboard = NULL;
    s_bottom_sheet = NULL;
    s_sheet_chevron = NULL;
    s_lbl_saf_val = NULL;
    s_countdown_cont = NULL;
    s_countdown_num_lbl = NULL;
    s_alerts_cont = NULL;
    s_alerts_list = NULL;
    s_warning_icon = NULL;
    for (int i = 0; i < 6; ++i) {
        s_metric_val_lbls[i] = NULL;
    }
    s_sheet_expanded = false;
    s_is_dragging = false;
}

static void blink_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (s_warning_icon != NULL && lv_obj_is_valid(s_warning_icon)) {
        if (lv_obj_has_flag(s_warning_icon, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_clear_flag(s_warning_icon, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_warning_icon, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void countdown_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (s_countdown_sec > 1) {
        s_countdown_sec--;
        if (s_countdown_num_lbl != NULL && lv_obj_is_valid(s_countdown_num_lbl)) {
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", s_countdown_sec);
            lv_label_set_text(s_countdown_num_lbl, buf);
        }
    } else {
        s_countdown_sec = 0;
        if (s_countdown_cont != NULL && lv_obj_is_valid(s_countdown_cont)) {
            lv_obj_add_flag(s_countdown_cont, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_alerts_cont != NULL && lv_obj_is_valid(s_alerts_cont)) {
            lv_obj_clear_flag(s_alerts_cont, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_blink_timer == NULL) {
            s_blink_timer = lv_timer_create(blink_timer_cb, 500, NULL);
        }

        if (s_metric_val_lbls[0] && lv_obj_is_valid(s_metric_val_lbls[0])) {
            lv_label_set_text(s_metric_val_lbls[0], "65,200 kg");
            lv_obj_set_style_text_color(s_metric_val_lbls[0], lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
        }
        if (s_metric_val_lbls[1] && lv_obj_is_valid(s_metric_val_lbls[1])) {
            lv_label_set_text(s_metric_val_lbls[1], "64,600 kg");
            lv_obj_set_style_text_color(s_metric_val_lbls[1], lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
        }
        if (s_metric_val_lbls[2] && lv_obj_is_valid(s_metric_val_lbls[2])) {
            lv_label_set_text(s_metric_val_lbls[2], "-52 deg C");
            lv_obj_set_style_text_color(s_metric_val_lbls[2], lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
        }
        if (s_metric_val_lbls[3] && lv_obj_is_valid(s_metric_val_lbls[3])) {
            lv_label_set_text(s_metric_val_lbls[3], "0.78");
            lv_obj_set_style_text_color(s_metric_val_lbls[3], lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
        }
        if (s_metric_val_lbls[4] && lv_obj_is_valid(s_metric_val_lbls[4])) {
            lv_label_set_text(s_metric_val_lbls[4], "2,400 kg/hr");
            lv_obj_set_style_text_color(s_metric_val_lbls[4], lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
        }
        if (s_metric_val_lbls[5] && lv_obj_is_valid(s_metric_val_lbls[5])) {
            lv_label_set_text(s_metric_val_lbls[5], "84.5%");
            lv_obj_set_style_text_color(s_metric_val_lbls[5], lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
        }

        if (s_countdown_timer != NULL) {
            lv_timer_del(s_countdown_timer);
            s_countdown_timer = NULL;
        }
    }
}

static void snap_bottom_sheet(bool expand)
{
    if (s_bottom_sheet == NULL) return;
    s_sheet_expanded = expand;
    lv_coord_t target_y = expand ? SHEET_Y_EXPANDED : SHEET_Y_COLLAPSED;

    lv_anim_del(s_bottom_sheet, anim_sheet_y_cb);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_bottom_sheet);
    lv_anim_set_values(&a, lv_obj_get_y(s_bottom_sheet), target_y);
    lv_anim_set_time(&a, 200);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, anim_sheet_y_cb);
    lv_anim_start(&a);

    if (s_sheet_chevron != NULL) {
        lv_label_set_text(s_sheet_chevron, expand ? LV_SYMBOL_DOWN : LV_SYMBOL_UP);
    }
}

static void sheet_handle_event_cb(lv_event_t *e)
{
    if (s_bottom_sheet == NULL) return;
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_get_act();

    if (code == LV_EVENT_PRESSED) {
        lv_anim_del(s_bottom_sheet, anim_sheet_y_cb);
        if (indev) {
            lv_indev_get_point(indev, &s_drag_start_point);
            s_drag_start_y = lv_obj_get_y(s_bottom_sheet);
            s_is_dragging = false;
        }
    } else if (code == LV_EVENT_PRESSING) {
        if (indev) {
            lv_point_t p;
            lv_indev_get_point(indev, &p);
            lv_coord_t dy = p.y - s_drag_start_point.y;

            if (LV_ABS(dy) > 4) {
                s_is_dragging = true;
                lv_coord_t new_y = s_drag_start_y + dy;
                if (new_y < SHEET_Y_EXPANDED) new_y = SHEET_Y_EXPANDED;
                if (new_y > SHEET_Y_COLLAPSED) new_y = SHEET_Y_COLLAPSED;
                if (lv_obj_get_y(s_bottom_sheet) != new_y) {
                    lv_obj_set_y(s_bottom_sheet, new_y);
                }
            }
        }
    } else if (code == LV_EVENT_RELEASED) {
        if (s_is_dragging) {
            lv_coord_t curr_y = lv_obj_get_y(s_bottom_sheet);
            snap_bottom_sheet(curr_y < SHEET_SNAP_MIDPOINT);
            s_is_dragging = false;
        } else {
            snap_bottom_sheet(!s_sheet_expanded);
        }
    }
}

static void saf_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);

    char buf[16];
    snprintf(buf, sizeof(buf), "%ld%%", (long)val);
    lv_label_set_text(s_lbl_saf_val, buf);

    flight_record_t *f = ui_manager_get_active_flight();
    if (f != NULL) {
        f->saf_blend_pct = (uint8_t)val;
    }
}

static void end_flight_btn_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        modal_exit_show(s_scr_dashboard);
    }
}

static lv_obj_t *create_pilot_alert_card(lv_obj_t *parent, const char *title, const char *desc)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_add_style(card, ui_style_card_alert(), 0);
    lv_obj_set_size(card, 220, 48);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_pad_all(card, 6, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_lbl = lv_label_create(card);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(UI_COLOR_HEX_WARNING), 0);
    lv_obj_set_pos(title_lbl, 4, 2);

    lv_obj_t *desc_lbl = lv_label_create(card);
    lv_label_set_text(desc_lbl, desc);
    lv_obj_set_style_text_font(desc_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(desc_lbl, lv_color_hex(UI_COLOR_HEX_ON_WARNING_CONT), 0);
    lv_obj_set_pos(desc_lbl, 4, 19);

    return card;
}

static lv_obj_t *create_metric_card(lv_obj_t *parent,
                                    const char *title,
                                    const char *val,
                                    const char *tag,
                                    bool is_warning,
                                    lv_obj_t **out_val_lbl)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_add_style(card, ui_style_card(), 0);
    lv_obj_set_size(card, 216, 52);
    lv_obj_set_style_pad_all(card, 5, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    if (is_warning) {
        lv_obj_set_style_border_color(card, lv_color_hex(UI_COLOR_HEX_WARNING), 0);
        lv_obj_set_style_border_width(card, 1, 0);
    }

    lv_obj_t *title_lbl = lv_label_create(card);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title_lbl, is_warning ? lv_color_hex(UI_COLOR_HEX_WARNING) : lv_color_hex(UI_COLOR_HEX_TEXT_MUTED), 0);
    lv_obj_set_pos(title_lbl, 4, 2);

    lv_obj_t *val_lbl = lv_label_create(card);
    lv_label_set_text(val_lbl, val);
    lv_obj_set_style_text_font(val_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(val_lbl, (strcmp(val, "CALIBRATING...") == 0) ? lv_color_hex(UI_COLOR_HEX_TEXT_MUTED) : lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
    lv_obj_set_pos(val_lbl, 4, 18);

    if (out_val_lbl != NULL) {
        *out_val_lbl = val_lbl;
    }

    if (tag != NULL) {
        lv_obj_t *tag_lbl = lv_label_create(card);
        lv_label_set_text(tag_lbl, tag);
        lv_obj_set_style_text_font(tag_lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(tag_lbl, lv_color_hex(UI_COLOR_HEX_WARNING), 0);
        lv_obj_align(tag_lbl, LV_ALIGN_RIGHT_MID, -6, 8);
    }

    return card;
}

lv_obj_t *screen_dashboard_create(void)
{
    s_scr_dashboard = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr_dashboard, lv_color_hex(UI_COLOR_HEX_SURFACE), 0);
    lv_obj_set_style_bg_opa(s_scr_dashboard, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_scr_dashboard, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_scr_dashboard, dashboard_delete_cb, LV_EVENT_DELETE, NULL);

    flight_record_t *f = ui_manager_get_active_flight();
    const char *badge = (f != NULL && f->flight_no[0] != '\0') ? f->flight_no : "5J-561";
    const char *ac_type_str = (f != NULL && f->aircraft_type[0] != '\0') ? f->aircraft_type : "A320neo";
    bool is_a330 = (strstr(ac_type_str, "330") != NULL);

    char route_title[48];
    if (f != NULL && f->origin[0] != '\0' && f->dest[0] != '\0') {
        snprintf(route_title, sizeof(route_title), "%s -> %s", f->origin, f->dest);
    } else {
        snprintf(route_title, sizeof(route_title), "MNL -> CGY");
    }
    (void)ui_topbar_create(s_scr_dashboard, route_title, false, NULL, badge);

    lv_obj_t *left_area = lv_obj_create(s_scr_dashboard);
    lv_obj_set_size(left_area, 240, 218);
    lv_obj_set_pos(left_area, 0, 38);
    lv_obj_set_style_bg_opa(left_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left_area, 0, 0);
    lv_obj_set_style_pad_all(left_area, 0, 0);
    lv_obj_set_style_pad_row(left_area, 8, 0);
    lv_obj_set_flex_flow(left_area, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(left_area, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(left_area, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *cruise_lbl = lv_label_create(left_area);
    lv_label_set_text(cruise_lbl, "CRUISE FL340 | M 0.78");
    lv_obj_set_style_text_font(cruise_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(cruise_lbl, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
    lv_obj_set_style_text_align(cruise_lbl, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *ac_img = lv_img_create(left_area);
    lv_img_set_src(ac_img, is_a330 ? &img_ac_a330 : &img_ac_a320);

    lv_obj_t *ac_chip = lv_obj_create(left_area);
    lv_obj_add_style(ac_chip, ui_style_chip(), 0);
    lv_obj_set_size(ac_chip, LV_SIZE_CONTENT, 24);
    lv_obj_set_style_pad_hor(ac_chip, 14, 0);
    lv_obj_set_style_pad_ver(ac_chip, 0, 0);
    lv_obj_set_style_border_width(ac_chip, 0, 0);
    lv_obj_set_style_bg_color(ac_chip, lv_color_hex(UI_COLOR_HEX_PRIMARY_CONTAINER), 0);
    lv_obj_set_flex_flow(ac_chip, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ac_chip, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(ac_chip, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ac_chip_lbl = lv_label_create(ac_chip);
    lv_label_set_text(ac_chip_lbl, ac_type_str);
    lv_obj_set_style_text_font(ac_chip_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ac_chip_lbl, lv_color_hex(UI_COLOR_HEX_ON_PRIMARY_CONT), 0);

    s_countdown_cont = lv_obj_create(s_scr_dashboard);
    lv_obj_set_size(s_countdown_cont, 240, 178);
    lv_obj_set_pos(s_countdown_cont, 240, 38);
    lv_obj_set_style_bg_opa(s_countdown_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_countdown_cont, 0, 0);
    lv_obj_set_style_pad_all(s_countdown_cont, 0, 0);
    lv_obj_set_style_pad_row(s_countdown_cont, 6, 0);
    lv_obj_set_flex_flow(s_countdown_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_countdown_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(s_countdown_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *cd_hdr_lbl = lv_label_create(s_countdown_cont);
    lv_label_set_text(cd_hdr_lbl, "STARTING IN");
    lv_obj_set_style_text_font(cd_hdr_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(cd_hdr_lbl, lv_color_hex(UI_COLOR_HEX_TEXT_SECONDARY), 0);

    s_countdown_num_lbl = lv_label_create(s_countdown_cont);
    lv_label_set_text(s_countdown_num_lbl, "5");
    lv_obj_set_style_text_font(s_countdown_num_lbl, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_countdown_num_lbl, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);

    s_alerts_cont = lv_obj_create(s_scr_dashboard);
    lv_obj_set_size(s_alerts_cont, 240, 178);
    lv_obj_set_pos(s_alerts_cont, 240, 38);
    lv_obj_set_style_bg_opa(s_alerts_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_alerts_cont, 0, 0);
    lv_obj_set_style_pad_all(s_alerts_cont, 0, 0);
    lv_obj_clear_flag(s_alerts_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_alerts_cont, LV_OBJ_FLAG_HIDDEN);

    s_warning_icon = lv_label_create(s_alerts_cont);
    lv_label_set_text(s_warning_icon, LV_SYMBOL_WARNING);
    lv_obj_set_style_text_font(s_warning_icon, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_warning_icon, lv_color_hex(UI_COLOR_HEX_WARNING), 0);
    lv_obj_align(s_warning_icon, LV_ALIGN_TOP_MID, 0, 2);

    s_alerts_list = lv_obj_create(s_alerts_cont);
    lv_obj_set_size(s_alerts_list, 232, 148);
    lv_obj_set_pos(s_alerts_list, 4, 28);
    lv_obj_set_style_bg_opa(s_alerts_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_alerts_list, 0, 0);
    lv_obj_set_style_pad_all(s_alerts_list, 2, 0);
    lv_obj_set_style_pad_row(s_alerts_list, 6, 0);
    lv_obj_set_flex_flow(s_alerts_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_alerts_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(s_alerts_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_alerts_list, LV_SCROLLBAR_MODE_AUTO);

    (void)create_pilot_alert_card(s_alerts_list, "STEP CLIMB TO FL360", "Fuel Burn Optimization (-4.2%)");
    (void)create_pilot_alert_card(s_alerts_list, "TRIM SPEED TO M 0.76", "Maximum SAF Range Profile");
    (void)create_pilot_alert_card(s_alerts_list, "AFT CG SHIFT ADVISORY", "Crossfeed balance recommended");

    lv_obj_t *btn_end = lv_btn_create(s_scr_dashboard);
    lv_obj_add_style(btn_end, ui_style_btn_outline(), 0);
    lv_obj_set_size(btn_end, 114, 30);
    lv_obj_set_pos(btn_end, 358, 222);
    lv_obj_set_style_pad_all(btn_end, 0, 0);
    lv_obj_add_event_cb(btn_end, end_flight_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_end_lbl = lv_label_create(btn_end);
    lv_label_set_text(btn_end_lbl, LV_SYMBOL_POWER " End Flight");
    lv_obj_set_style_text_font(btn_end_lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(btn_end_lbl);

    s_bottom_sheet = lv_obj_create(s_scr_dashboard);
    lv_obj_add_style(s_bottom_sheet, ui_style_bottom_sheet(), 0);
    lv_obj_set_size(s_bottom_sheet, 480, 280);
    lv_obj_set_pos(s_bottom_sheet, 0, SHEET_Y_COLLAPSED);
    lv_obj_set_style_pad_all(s_bottom_sheet, 0, 0);
    lv_obj_clear_flag(s_bottom_sheet, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *handle_bar = lv_obj_create(s_bottom_sheet);
    lv_obj_set_size(handle_bar, 480, 44);
    lv_obj_set_pos(handle_bar, 0, 0);
    lv_obj_set_style_bg_opa(handle_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(handle_bar, 0, 0);
    lv_obj_set_style_pad_all(handle_bar, 0, 0);
    lv_obj_clear_flag(handle_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(handle_bar, sheet_handle_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *pill = lv_obj_create(handle_bar);
    lv_obj_set_size(pill, 48, 4);
    lv_obj_set_pos(pill, 216, 5);
    lv_obj_set_style_bg_color(pill, lv_color_hex(UI_COLOR_HEX_TEXT_MUTED), 0);
    lv_obj_set_style_border_width(pill, 0, 0);
    lv_obj_set_style_radius(pill, 2, 0);

    lv_obj_t *sheet_title = lv_label_create(handle_bar);
    lv_label_set_text(sheet_title, "AVIONICS & AIR DATA SENSORS");
    lv_obj_set_style_text_font(sheet_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sheet_title, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
    lv_obj_set_pos(sheet_title, 16, 20);

    s_sheet_chevron = lv_label_create(handle_bar);
    lv_label_set_text(s_sheet_chevron, LV_SYMBOL_UP);
    lv_obj_set_style_text_font(s_sheet_chevron, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_sheet_chevron, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
    lv_obj_align(s_sheet_chevron, LV_ALIGN_TOP_RIGHT, -20, 20);

    lv_obj_t *sheet_body = lv_obj_create(s_bottom_sheet);
    lv_obj_set_size(sheet_body, 480, 190);
    lv_obj_set_pos(sheet_body, 0, 44);
    lv_obj_set_style_bg_opa(sheet_body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sheet_body, 0, 0);
    lv_obj_set_style_pad_hor(sheet_body, 16, 0);
    lv_obj_set_style_pad_top(sheet_body, 4, 0);
    lv_obj_set_style_pad_bottom(sheet_body, 28, 0);
    lv_obj_set_scroll_dir(sheet_body, LV_DIR_VER);

    lv_obj_t *saf_card = lv_obj_create(sheet_body);
    lv_obj_add_style(saf_card, ui_style_card(), 0);
    lv_obj_set_size(saf_card, 448, 54);
    lv_obj_set_pos(saf_card, 0, 2);
    lv_obj_set_style_pad_all(saf_card, 8, 0);
    lv_obj_clear_flag(saf_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_saf_tag = lv_label_create(saf_card);
    lv_label_set_text(lbl_saf_tag, "SAF Blend Ratio");
    lv_obj_set_style_text_font(lbl_saf_tag, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_saf_tag, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
    lv_obj_set_pos(lbl_saf_tag, 6, 2);

    uint8_t init_saf = (f != NULL) ? f->saf_blend_pct : 0U;
    char saf_init_str[16];
    snprintf(saf_init_str, sizeof(saf_init_str), "%u%%", (unsigned int)init_saf);

    s_lbl_saf_val = lv_label_create(saf_card);
    lv_label_set_text(s_lbl_saf_val, saf_init_str);
    lv_obj_set_style_text_font(s_lbl_saf_val, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl_saf_val, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
    lv_obj_align(s_lbl_saf_val, LV_ALIGN_TOP_RIGHT, -10, 0);

    lv_obj_t *saf_slider = lv_slider_create(saf_card);
    lv_obj_set_size(saf_slider, 420, 8);
    lv_obj_set_pos(saf_slider, 6, 26);
    lv_slider_set_range(saf_slider, 0, 100);
    lv_slider_set_value(saf_slider, init_saf, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(saf_slider, lv_color_hex(UI_COLOR_HEX_PRIMARY_CONTAINER), LV_PART_MAIN);
    lv_obj_set_style_bg_color(saf_slider, lv_color_hex(UI_COLOR_HEX_PRIMARY), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(saf_slider, lv_color_hex(UI_COLOR_HEX_PRIMARY), LV_PART_KNOB);
    lv_obj_set_style_pad_all(saf_slider, 4, LV_PART_KNOB);
    lv_obj_add_event_cb(saf_slider, saf_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *c1 = create_metric_card(sheet_body, "FMS MASS", "CALIBRATING...", NULL, false, &s_metric_val_lbls[0]);
    lv_obj_set_pos(c1, 0, 62);

    lv_obj_t *c2 = create_metric_card(sheet_body, "TRUE MASS", "CALIBRATING...", "-600 kg", true, &s_metric_val_lbls[1]);
    lv_obj_set_pos(c2, 232, 62);

    lv_obj_t *c3 = create_metric_card(sheet_body, "OAT (OUTSIDE TEMP)", "CALIBRATING...", NULL, false, &s_metric_val_lbls[2]);
    lv_obj_set_pos(c3, 0, 118);

    lv_obj_t *c4 = create_metric_card(sheet_body, "MACH NUMBER", "CALIBRATING...", NULL, false, &s_metric_val_lbls[3]);
    lv_obj_set_pos(c4, 232, 118);

    lv_obj_t *c5 = create_metric_card(sheet_body, "FUEL FLOW", "CALIBRATING...", NULL, false, &s_metric_val_lbls[4]);
    lv_obj_set_pos(c5, 0, 174);

    lv_obj_t *c6 = create_metric_card(sheet_body, "N1 ENGINE RPM", "CALIBRATING...", NULL, false, &s_metric_val_lbls[5]);
    lv_obj_set_pos(c6, 232, 174);

    s_sheet_expanded = false;

    s_countdown_sec = 5;
    s_countdown_timer = lv_timer_create(countdown_timer_cb, 1000, NULL);

    return s_scr_dashboard;
}
