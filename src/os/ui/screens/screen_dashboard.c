/*!
    \file    src/os/ui/screens/screen_dashboard.c
    \brief   Flight telemetry dashboard and advisory bottom sheet
*/

#include "screen_dashboard.h"
#include "modal_exit.h"
#include "telemetry_manager.h"
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
#define METRIC_CARD_COUNT   8

static lv_obj_t *s_scr_dashboard = NULL;
static lv_obj_t *s_bottom_sheet = NULL;
static lv_obj_t *s_sheet_chevron = NULL;

static bool s_sheet_expanded = false;
static lv_point_t s_drag_start_point;
static lv_coord_t s_drag_start_y = 0;
static bool s_is_dragging = false;

static lv_obj_t *s_cruise_lbl = NULL;
static lv_obj_t *s_lbl_wing_left = NULL;
static lv_obj_t *s_lbl_wing_right = NULL;

static lv_obj_t *s_waiting_cont = NULL;
static lv_obj_t *s_alerts_cont = NULL;
static lv_obj_t *s_alerts_list = NULL;
static lv_obj_t *s_metric_val_lbls[METRIC_CARD_COUNT] = { NULL };

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
    s_cruise_lbl = NULL;
    s_lbl_wing_left = NULL;
    s_lbl_wing_right = NULL;
    s_waiting_cont = NULL;
    s_alerts_cont = NULL;
    s_alerts_list = NULL;
    s_warning_icon = NULL;
    for (int i = 0; i < METRIC_CARD_COUNT; ++i) {
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

void screen_dashboard_refresh(void)
{
    if (s_scr_dashboard == NULL || !lv_obj_is_valid(s_scr_dashboard)) {
        return;
    }

    const telemetry_data_t *t = telemetry_manager_get();

    if (t->is_started) {
        if (s_waiting_cont != NULL && lv_obj_is_valid(s_waiting_cont)) {
            lv_obj_add_flag(s_waiting_cont, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_alerts_cont != NULL && lv_obj_is_valid(s_alerts_cont)) {
            lv_obj_clear_flag(s_alerts_cont, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_blink_timer == NULL) {
            s_blink_timer = lv_timer_create(blink_timer_cb, 500, NULL);
        }
    } else {
        if (s_waiting_cont != NULL && lv_obj_is_valid(s_waiting_cont)) {
            lv_obj_clear_flag(s_waiting_cont, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_alerts_cont != NULL && lv_obj_is_valid(s_alerts_cont)) {
            lv_obj_add_flag(s_alerts_cont, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_blink_timer != NULL) {
            lv_timer_del(s_blink_timer);
            s_blink_timer = NULL;
        }
    }

    if (!t->is_started || !t->has_data) {
        if (s_cruise_lbl != NULL && lv_obj_is_valid(s_cruise_lbl)) {
            lv_label_set_text(s_cruise_lbl, "CRUISE --- ft | M ---");
        }
        if (s_lbl_wing_left != NULL && lv_obj_is_valid(s_lbl_wing_left)) {
            lv_label_set_text(s_lbl_wing_left, "---");
            lv_obj_set_style_text_color(s_lbl_wing_left, lv_color_hex(UI_COLOR_HEX_TEXT_MUTED), 0);
        }
        if (s_lbl_wing_right != NULL && lv_obj_is_valid(s_lbl_wing_right)) {
            lv_label_set_text(s_lbl_wing_right, "---");
            lv_obj_set_style_text_color(s_lbl_wing_right, lv_color_hex(UI_COLOR_HEX_TEXT_MUTED), 0);
        }
        for (int i = 0; i < METRIC_CARD_COUNT; ++i) {
            if (s_metric_val_lbls[i] != NULL && lv_obj_is_valid(s_metric_val_lbls[i])) {
                lv_label_set_text(s_metric_val_lbls[i], "---");
                lv_obj_set_style_text_color(s_metric_val_lbls[i], lv_color_hex(UI_COLOR_HEX_TEXT_MUTED), 0);
            }
        }
        return;
    }

    char buf[48];

    if (s_cruise_lbl != NULL && lv_obj_is_valid(s_cruise_lbl)) {
        snprintf(buf, sizeof(buf), "CRUISE %ld ft | M %.2f", (long)t->alt_ft, (double)t->mach);
        lv_label_set_text(s_cruise_lbl, buf);
    }

    if (s_lbl_wing_left != NULL && lv_obj_is_valid(s_lbl_wing_left)) {
        snprintf(buf, sizeof(buf), "%.1f kg", (double)t->left_tank_kg);
        lv_label_set_text(s_lbl_wing_left, buf);
        lv_obj_set_style_text_color(s_lbl_wing_left, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
    }

    if (s_lbl_wing_right != NULL && lv_obj_is_valid(s_lbl_wing_right)) {
        snprintf(buf, sizeof(buf), "%.1f kg", (double)t->right_tank_kg);
        lv_label_set_text(s_lbl_wing_right, buf);
        lv_obj_set_style_text_color(s_lbl_wing_right, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
    }

    if (s_metric_val_lbls[0] != NULL && lv_obj_is_valid(s_metric_val_lbls[0])) {
        snprintf(buf, sizeof(buf), "%ld ft", (long)t->alt_ft);
        lv_label_set_text(s_metric_val_lbls[0], buf);
        lv_obj_set_style_text_color(s_metric_val_lbls[0], lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
    }
    if (s_metric_val_lbls[1] != NULL && lv_obj_is_valid(s_metric_val_lbls[1])) {
        snprintf(buf, sizeof(buf), "%.2f", (double)t->mach);
        lv_label_set_text(s_metric_val_lbls[1], buf);
        lv_obj_set_style_text_color(s_metric_val_lbls[1], lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
    }
    if (s_metric_val_lbls[2] != NULL && lv_obj_is_valid(s_metric_val_lbls[2])) {
        snprintf(buf, sizeof(buf), "%.1f \xC2\xB0" "C", (double)t->oat_c);
        lv_label_set_text(s_metric_val_lbls[2], buf);
        lv_obj_set_style_text_color(s_metric_val_lbls[2], lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
    }
    if (s_metric_val_lbls[3] != NULL && lv_obj_is_valid(s_metric_val_lbls[3])) {
        snprintf(buf, sizeof(buf), "%.1f \xC2\xB0" "C", (double)t->fuel_temp_c);
        lv_label_set_text(s_metric_val_lbls[3], buf);
        lv_obj_set_style_text_color(s_metric_val_lbls[3], lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
    }
    if (s_metric_val_lbls[4] != NULL && lv_obj_is_valid(s_metric_val_lbls[4])) {
        snprintf(buf, sizeof(buf), "%ld\xC2\xB0", (long)t->heading_deg);
        lv_label_set_text(s_metric_val_lbls[4], buf);
        lv_obj_set_style_text_color(s_metric_val_lbls[4], lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
    }
    if (s_metric_val_lbls[5] != NULL && lv_obj_is_valid(s_metric_val_lbls[5])) {
        snprintf(buf, sizeof(buf), "%.1f kg", (double)t->left_tank_kg);
        lv_label_set_text(s_metric_val_lbls[5], buf);
        lv_obj_set_style_text_color(s_metric_val_lbls[5], lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
    }
    if (s_metric_val_lbls[6] != NULL && lv_obj_is_valid(s_metric_val_lbls[6])) {
        snprintf(buf, sizeof(buf), "%.1f kg", (double)t->right_tank_kg);
        lv_label_set_text(s_metric_val_lbls[6], buf);
        lv_obj_set_style_text_color(s_metric_val_lbls[6], lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
    }
    if (s_metric_val_lbls[7] != NULL && lv_obj_is_valid(s_metric_val_lbls[7])) {
        snprintf(buf, sizeof(buf), "%.1f kg", (double)t->total_weight_kg);
        lv_label_set_text(s_metric_val_lbls[7], buf);
        lv_obj_set_style_text_color(s_metric_val_lbls[7], lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
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
                                    lv_obj_t **out_val_lbl)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_add_style(card, ui_style_card(), 0);
    lv_obj_set_size(card, 216, 52);
    lv_obj_set_style_pad_all(card, 5, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_lbl = lv_label_create(card);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(UI_COLOR_HEX_TEXT_MUTED), 0);
    lv_obj_set_pos(title_lbl, 4, 2);

    lv_obj_t *val_lbl = lv_label_create(card);
    lv_label_set_text(val_lbl, val);
    lv_obj_set_style_text_font(val_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(val_lbl, lv_color_hex(UI_COLOR_HEX_TEXT_MUTED), 0);
    lv_obj_set_pos(val_lbl, 4, 18);

    if (out_val_lbl != NULL) {
        *out_val_lbl = val_lbl;
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

    s_cruise_lbl = lv_label_create(left_area);
    lv_label_set_text(s_cruise_lbl, "CRUISE --- ft | M ---");
    lv_obj_set_style_text_font(s_cruise_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_cruise_lbl, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
    lv_obj_set_style_text_align(s_cruise_lbl, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *ac_cont = lv_obj_create(left_area);
    lv_obj_set_size(ac_cont, 240, 132);
    lv_obj_set_style_bg_opa(ac_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ac_cont, 0, 0);
    lv_obj_set_style_pad_all(ac_cont, 0, 0);
    lv_obj_clear_flag(ac_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ac_img = lv_img_create(ac_cont);
    lv_img_set_src(ac_img, is_a330 ? &img_ac_a330 : &img_ac_a320);
    lv_obj_center(ac_img);

    s_lbl_wing_left = lv_label_create(ac_cont);
    lv_label_set_text(s_lbl_wing_left, "---");
    lv_obj_set_width(s_lbl_wing_left, 92);
    lv_obj_set_style_text_font(s_lbl_wing_left, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_lbl_wing_left, lv_color_hex(UI_COLOR_HEX_TEXT_MUTED), 0);
    lv_obj_set_style_text_align(s_lbl_wing_left, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_lbl_wing_left, LV_ALIGN_CENTER, -56, 20);

    s_lbl_wing_right = lv_label_create(ac_cont);
    lv_label_set_text(s_lbl_wing_right, "---");
    lv_obj_set_width(s_lbl_wing_right, 92);
    lv_obj_set_style_text_font(s_lbl_wing_right, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_lbl_wing_right, lv_color_hex(UI_COLOR_HEX_TEXT_MUTED), 0);
    lv_obj_set_style_text_align(s_lbl_wing_right, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_lbl_wing_right, LV_ALIGN_CENTER, 56, 20);

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

    s_waiting_cont = lv_obj_create(s_scr_dashboard);
    lv_obj_set_size(s_waiting_cont, 240, 178);
    lv_obj_set_pos(s_waiting_cont, 240, 38);
    lv_obj_set_style_bg_opa(s_waiting_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_waiting_cont, 0, 0);
    lv_obj_set_style_pad_all(s_waiting_cont, 0, 0);
    lv_obj_set_flex_flow(s_waiting_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_waiting_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(s_waiting_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *wait_lbl = lv_label_create(s_waiting_cont);
    lv_label_set_text(wait_lbl, "Waiting...");
    lv_obj_set_style_text_font(wait_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(wait_lbl, lv_color_hex(UI_COLOR_HEX_TEXT_SECONDARY), 0);

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

    lv_obj_t *c0 = create_metric_card(sheet_body, "ALTITUDE", "---", &s_metric_val_lbls[0]);
    lv_obj_set_pos(c0, 0, 2);

    lv_obj_t *c1 = create_metric_card(sheet_body, "MACH SPEED", "---", &s_metric_val_lbls[1]);
    lv_obj_set_pos(c1, 232, 2);

    lv_obj_t *c2 = create_metric_card(sheet_body, "OAT (OUTSIDE TEMP)", "---", &s_metric_val_lbls[2]);
    lv_obj_set_pos(c2, 0, 58);

    lv_obj_t *c3 = create_metric_card(sheet_body, "FUEL TEMP", "---", &s_metric_val_lbls[3]);
    lv_obj_set_pos(c3, 232, 58);

    lv_obj_t *c4 = create_metric_card(sheet_body, "HEADING", "---", &s_metric_val_lbls[4]);
    lv_obj_set_pos(c4, 0, 114);

    lv_obj_t *c5 = create_metric_card(sheet_body, "LEFT TANK", "---", &s_metric_val_lbls[5]);
    lv_obj_set_pos(c5, 232, 114);

    lv_obj_t *c6 = create_metric_card(sheet_body, "RIGHT TANK", "---", &s_metric_val_lbls[6]);
    lv_obj_set_pos(c6, 0, 170);

    lv_obj_t *c7 = create_metric_card(sheet_body, "TOTAL WEIGHT", "---", &s_metric_val_lbls[7]);
    lv_obj_set_pos(c7, 232, 170);

    s_sheet_expanded = false;

    screen_dashboard_refresh();

    return s_scr_dashboard;
}
