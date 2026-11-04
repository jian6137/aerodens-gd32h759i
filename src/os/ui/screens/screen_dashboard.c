/*!
    \file    src/os/ui/screens/screen_dashboard.c
    \brief   Flight Telemetry Advisory Dashboard
*/

#include "screen_dashboard.h"
#include "edge_ai.h"
#include "modal_exit.h"
#include "telemetry_manager.h"
#include "ui_manager.h"
#include "ui_theme.h"
#include "ui_topbar.h"
#include "img_ac_a320.h"
#include "img_ac_a330.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define SHEET_Y_EXPANDED    38
#define SHEET_Y_COLLAPSED   258
#define SHEET_SNAP_MIDPOINT ((SHEET_Y_EXPANDED + SHEET_Y_COLLAPSED) / 2)

#define COMP_CARD_COUNT     7
#define RAW_CARD_COUNT      8

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
static lv_obj_t *s_lbl_ac_co2_saved = NULL;
static lv_obj_t *s_lbl_ac_fuel_saved = NULL;

static lv_obj_t *s_waiting_cont = NULL;
static lv_obj_t *s_alerts_cont = NULL;
static lv_obj_t *s_alerts_list = NULL;
static lv_obj_t *s_nominal_lbl = NULL;

static lv_obj_t *s_card_refuel = NULL;
static lv_obj_t *s_lbl_refuel_desc = NULL;
static lv_obj_t *s_card_step_climb = NULL;
static lv_obj_t *s_lbl_step_climb_title = NULL;
static lv_obj_t *s_lbl_step_climb_desc = NULL;
static lv_obj_t *s_card_trim_speed = NULL;
static lv_obj_t *s_card_cg_shift = NULL;

static lv_obj_t *s_card_fuel_saved = NULL;
static lv_obj_t *s_lbl_fuel_saved = NULL;
static lv_obj_t *s_card_co2_saved = NULL;
static lv_obj_t *s_lbl_co2_saved = NULL;
static lv_obj_t *s_comp_val_lbls[COMP_CARD_COUNT] = { NULL };
static lv_obj_t *s_raw_val_lbls[RAW_CARD_COUNT] = { NULL };

static lv_timer_t *s_blink_timer = NULL;
static lv_obj_t *s_warning_icon = NULL;
static bool s_any_alert_visible = false;

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
    s_lbl_ac_co2_saved = NULL;
    s_lbl_ac_fuel_saved = NULL;
    s_waiting_cont = NULL;
    s_alerts_cont = NULL;
    s_alerts_list = NULL;
    s_nominal_lbl = NULL;
    s_card_refuel = NULL;
    s_lbl_refuel_desc = NULL;
    s_card_step_climb = NULL;
    s_lbl_step_climb_title = NULL;
    s_lbl_step_climb_desc = NULL;
    s_card_trim_speed = NULL;
    s_card_cg_shift = NULL;
    s_card_fuel_saved = NULL;
    s_lbl_fuel_saved = NULL;
    s_card_co2_saved = NULL;
    s_lbl_co2_saved = NULL;
    s_warning_icon = NULL;
    s_any_alert_visible = false;

    for (int i = 0; i < COMP_CARD_COUNT; ++i) {
        s_comp_val_lbls[i] = NULL;
    }
    for (int i = 0; i < RAW_CARD_COUNT; ++i) {
        s_raw_val_lbls[i] = NULL;
    }
    s_sheet_expanded = false;
    s_is_dragging = false;
}

static void blink_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (s_warning_icon == NULL || !lv_obj_is_valid(s_warning_icon)) {
        return;
    }
    if (!s_any_alert_visible) {
        lv_obj_add_flag(s_warning_icon, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (lv_obj_has_flag(s_warning_icon, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(s_warning_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_warning_icon, LV_OBJ_FLAG_HIDDEN);
    }
}

static void set_metric_lbl(lv_obj_t *lbl, const char *text, uint32_t color_hex)
{
    if (lbl != NULL && lv_obj_is_valid(lbl)) {
        lv_label_set_text(lbl, text);
        lv_obj_set_style_text_color(lbl, lv_color_hex(color_hex), 0);
    }
}

void screen_dashboard_refresh(void)
{
    if (s_scr_dashboard == NULL || !lv_obj_is_valid(s_scr_dashboard)) {
        return;
    }

    telemetry_data_t t_snap;
    (void)telemetry_manager_get_snapshot(&t_snap);
    const telemetry_data_t *t = &t_snap;

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
        set_metric_lbl(s_lbl_wing_left, "---", UI_COLOR_HEX_TEXT_MUTED);
        set_metric_lbl(s_lbl_wing_right, "---", UI_COLOR_HEX_TEXT_MUTED);
        if (s_lbl_ac_co2_saved != NULL) lv_obj_add_flag(s_lbl_ac_co2_saved, LV_OBJ_FLAG_HIDDEN);
        if (s_lbl_ac_fuel_saved != NULL) lv_obj_add_flag(s_lbl_ac_fuel_saved, LV_OBJ_FLAG_HIDDEN);

        set_metric_lbl(s_lbl_fuel_saved, "---", UI_COLOR_HEX_TEXT_MUTED);
        set_metric_lbl(s_lbl_co2_saved, "---", UI_COLOR_HEX_TEXT_MUTED);
        if (s_card_fuel_saved != NULL && lv_obj_is_valid(s_card_fuel_saved)) {
            lv_obj_set_style_border_color(s_card_fuel_saved, lv_color_hex(UI_COLOR_HEX_OUTLINE), 0);
        }
        if (s_card_co2_saved != NULL && lv_obj_is_valid(s_card_co2_saved)) {
            lv_obj_set_style_border_color(s_card_co2_saved, lv_color_hex(UI_COLOR_HEX_OUTLINE), 0);
        }

        for (int i = 0; i < COMP_CARD_COUNT; ++i) {
            set_metric_lbl(s_comp_val_lbls[i], "---", UI_COLOR_HEX_TEXT_MUTED);
        }
        for (int i = 0; i < RAW_CARD_COUNT; ++i) {
            set_metric_lbl(s_raw_val_lbls[i], "---", UI_COLOR_HEX_TEXT_MUTED);
        }

        if (s_card_refuel != NULL) lv_obj_add_flag(s_card_refuel, LV_OBJ_FLAG_HIDDEN);
        if (s_card_step_climb != NULL) lv_obj_add_flag(s_card_step_climb, LV_OBJ_FLAG_HIDDEN);
        if (s_card_trim_speed != NULL) lv_obj_add_flag(s_card_trim_speed, LV_OBJ_FLAG_HIDDEN);
        if (s_card_cg_shift != NULL) lv_obj_add_flag(s_card_cg_shift, LV_OBJ_FLAG_HIDDEN);
        if (s_warning_icon != NULL) lv_obj_add_flag(s_warning_icon, LV_OBJ_FLAG_HIDDEN);
        if (s_nominal_lbl != NULL) lv_obj_clear_flag(s_nominal_lbl, LV_OBJ_FLAG_HIDDEN);
        s_any_alert_visible = false;
        return;
    }

    flight_record_t *f = ui_manager_get_active_flight();

    edge_ai_input_t ai_in;
    ai_in.saf_blend_pct = (f != NULL) ? f->saf_blend_pct : 0U;
    ai_in.planned_fuel_kg = (f != NULL && f->planned_fuel_l > 0U) ? (float)f->planned_fuel_l : 6800.0f;
    ai_in.zfw_kg = (f != NULL && f->base_zfw_kg > 0U) ? (float)f->base_zfw_kg : 62400.0f;
    ai_in.alt_ft = (float)t->alt_ft;
    ai_in.mach = t->mach;
    ai_in.oat_c = t->oat_c;
    ai_in.fuel_temp_c = t->fuel_temp_c;
    ai_in.heading_deg = (float)t->heading_deg;
    ai_in.left_tank_kg = t->left_tank_kg;
    ai_in.right_tank_kg = t->right_tank_kg;
    ai_in.total_weight_kg = t->total_weight_kg;

    edge_ai_output_t ai_out;
    edge_ai_compute(&ai_in, &ai_out);

    /* Log live USART telemetry reading & AeroDENS computation to SD Card CSV + flight.inf */
    if (f != NULL) {
        (void)flight_store_log_telemetry(f, t, &ai_out);
    }

    char buf[80];

    /* Main Flight Screen Updates */
    if (s_cruise_lbl != NULL && lv_obj_is_valid(s_cruise_lbl)) {
        snprintf(buf, sizeof(buf), "CRUISE %ld ft | M %.2f", (long)t->alt_ft, (double)ai_in.mach);
        lv_label_set_text(s_cruise_lbl, buf);
    }

    snprintf(buf, sizeof(buf), "%.1f kg", (double)ai_in.left_tank_kg);
    set_metric_lbl(s_lbl_wing_left, buf, UI_COLOR_HEX_PRIMARY);

    snprintf(buf, sizeof(buf), "%.1f kg", (double)ai_in.right_tank_kg);
    set_metric_lbl(s_lbl_wing_right, buf, UI_COLOR_HEX_PRIMARY);

    /* Top-left Aircraft SVG Banked Savings Indicators */
    if (s_lbl_ac_co2_saved != NULL && lv_obj_is_valid(s_lbl_ac_co2_saved)) {
        if (ai_out.total_banked_co2_kg > 0.0f) {
            snprintf(buf, sizeof(buf), "CO2 %.2f kg", (double)ai_out.total_banked_co2_kg);
            lv_label_set_text(s_lbl_ac_co2_saved, buf);
            lv_obj_clear_flag(s_lbl_ac_co2_saved, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_lbl_ac_co2_saved, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (s_lbl_ac_fuel_saved != NULL && lv_obj_is_valid(s_lbl_ac_fuel_saved)) {
        if (ai_out.total_banked_fuel_kg > 0.0f) {
            snprintf(buf, sizeof(buf), "FUEL %.2f kg", (double)ai_out.total_banked_fuel_kg);
            lv_label_set_text(s_lbl_ac_fuel_saved, buf);
            lv_obj_clear_flag(s_lbl_ac_fuel_saved, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_lbl_ac_fuel_saved, LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* Split Banked Fuel & CO2 Cards in Bottomsheet */
    if (s_lbl_fuel_saved != NULL && lv_obj_is_valid(s_lbl_fuel_saved)) {
        snprintf(buf, sizeof(buf), "%.2f kg", (double)ai_out.total_banked_fuel_kg);
        set_metric_lbl(s_lbl_fuel_saved, buf, UI_COLOR_HEX_SUCCESS);
        if (s_card_fuel_saved != NULL && lv_obj_is_valid(s_card_fuel_saved)) {
            lv_obj_set_style_border_color(s_card_fuel_saved, lv_color_hex(UI_COLOR_HEX_SUCCESS), 0);
        }
    }

    if (s_lbl_co2_saved != NULL && lv_obj_is_valid(s_lbl_co2_saved)) {
        snprintf(buf, sizeof(buf), "%.2f kg", (double)ai_out.total_banked_co2_kg);
        set_metric_lbl(s_lbl_co2_saved, buf, UI_COLOR_HEX_SUCCESS);
        if (s_card_co2_saved != NULL && lv_obj_is_valid(s_card_co2_saved)) {
            lv_obj_set_style_border_color(s_card_co2_saved, lv_color_hex(UI_COLOR_HEX_SUCCESS), 0);
        }
    }

    /* Top Section: System Computation (7 Outputs) */
    snprintf(buf, sizeof(buf), "%.4f kg/L", (double)ai_out.base_density);
    set_metric_lbl(s_comp_val_lbls[0], buf, UI_COLOR_HEX_TEXT_PRIMARY);

    snprintf(buf, sizeof(buf), "%.4f kg/L", (double)ai_out.live_saf_density);
    set_metric_lbl(s_comp_val_lbls[1], buf, UI_COLOR_HEX_TEXT_PRIMARY);

    snprintf(buf, sizeof(buf), "%.1f L", (double)ai_out.indicated_volume_l);
    set_metric_lbl(s_comp_val_lbls[2], buf, UI_COLOR_HEX_TEXT_PRIMARY);

    snprintf(buf, sizeof(buf), "%.1f kg", (double)ai_out.true_fuel_weight_kg);
    set_metric_lbl(s_comp_val_lbls[3], buf, UI_COLOR_HEX_TEXT_PRIMARY);

    snprintf(buf, sizeof(buf), "%.1f kg", (double)ai_out.true_gross_weight_kg);
    set_metric_lbl(s_comp_val_lbls[4], buf, UI_COLOR_HEX_TEXT_PRIMARY);

    snprintf(buf, sizeof(buf), "%+.1f kg", (double)ai_out.fms_gw_variance_kg);
    set_metric_lbl(s_comp_val_lbls[5], buf,
                   (fabsf(ai_out.fms_gw_variance_kg) > 300.0f) ? UI_COLOR_HEX_WARNING : UI_COLOR_HEX_TEXT_PRIMARY);

    snprintf(buf, sizeof(buf), "%.1f L", (double)ai_out.target_refuel_volume_l);
    set_metric_lbl(s_comp_val_lbls[6], buf, UI_COLOR_HEX_PRIMARY);

    /* Bottom Section: Avionics Data (8 Raw Telemetry Variables) */
    snprintf(buf, sizeof(buf), "%ld ft", (long)t->alt_ft);
    set_metric_lbl(s_raw_val_lbls[0], buf, UI_COLOR_HEX_TEXT_PRIMARY);

    snprintf(buf, sizeof(buf), "%.2f", (double)ai_in.mach);
    set_metric_lbl(s_raw_val_lbls[1], buf, UI_COLOR_HEX_TEXT_PRIMARY);

    snprintf(buf, sizeof(buf), "%.1f \xC2\xB0" "C", (double)ai_in.oat_c);
    set_metric_lbl(s_raw_val_lbls[2], buf, UI_COLOR_HEX_TEXT_PRIMARY);

    snprintf(buf, sizeof(buf), "%.1f \xC2\xB0" "C", (double)ai_in.fuel_temp_c);
    set_metric_lbl(s_raw_val_lbls[3], buf, UI_COLOR_HEX_TEXT_PRIMARY);

    snprintf(buf, sizeof(buf), "%ld\xC2\xB0", (long)t->heading_deg);
    set_metric_lbl(s_raw_val_lbls[4], buf, UI_COLOR_HEX_TEXT_PRIMARY);

    snprintf(buf, sizeof(buf), "%.1f kg", (double)ai_in.left_tank_kg);
    set_metric_lbl(s_raw_val_lbls[5], buf, UI_COLOR_HEX_TEXT_PRIMARY);

    snprintf(buf, sizeof(buf), "%.1f kg", (double)ai_in.right_tank_kg);
    set_metric_lbl(s_raw_val_lbls[6], buf, UI_COLOR_HEX_TEXT_PRIMARY);

    snprintf(buf, sizeof(buf), "%.1f kg", (double)ai_in.total_weight_kg);
    set_metric_lbl(s_raw_val_lbls[7], buf, UI_COLOR_HEX_TEXT_PRIMARY);

    /* Dynamic Closed-Loop Advisory Cards */
    if (s_lbl_refuel_desc != NULL && lv_obj_is_valid(s_lbl_refuel_desc)) {
        snprintf(buf, sizeof(buf), "Pump %.1f Liters", (double)ai_out.target_refuel_volume_l);
        lv_label_set_text(s_lbl_refuel_desc, buf);
    }
    if (s_card_refuel != NULL && lv_obj_is_valid(s_card_refuel)) {
        if (ai_out.trigger_refuel) {
            lv_obj_clear_flag(s_card_refuel, LV_OBJ_FLAG_HIDDEN);
        } else if (ai_out.fulfill_refuel) {
            lv_obj_add_flag(s_card_refuel, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (s_lbl_step_climb_title != NULL && lv_obj_is_valid(s_lbl_step_climb_title)) {
        snprintf(buf, sizeof(buf), "STEP CLIMB TO FL%d", ai_out.target_fl);
        lv_label_set_text(s_lbl_step_climb_title, buf);
    }
    if (s_lbl_step_climb_desc != NULL && lv_obj_is_valid(s_lbl_step_climb_desc)) {
        snprintf(buf, sizeof(buf), "Save %.1f kg/hr", (double)ai_out.step_climb_savings_hr);
        lv_label_set_text(s_lbl_step_climb_desc, buf);
    }
    if (s_card_step_climb != NULL && lv_obj_is_valid(s_card_step_climb)) {
        if (ai_out.trigger_step_climb) {
            lv_obj_clear_flag(s_card_step_climb, LV_OBJ_FLAG_HIDDEN);
        } else if (ai_out.fulfill_step_climb) {
            lv_obj_add_flag(s_card_step_climb, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (s_card_trim_speed != NULL && lv_obj_is_valid(s_card_trim_speed)) {
        if (ai_out.trigger_trim_speed) {
            lv_obj_clear_flag(s_card_trim_speed, LV_OBJ_FLAG_HIDDEN);
        } else if (ai_out.fulfill_trim_speed) {
            lv_obj_add_flag(s_card_trim_speed, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (s_card_cg_shift != NULL && lv_obj_is_valid(s_card_cg_shift)) {
        if (ai_out.trigger_cg_shift) {
            lv_obj_clear_flag(s_card_cg_shift, LV_OBJ_FLAG_HIDDEN);
        } else if (ai_out.fulfill_cg_shift) {
            lv_obj_add_flag(s_card_cg_shift, LV_OBJ_FLAG_HIDDEN);
        }
    }

    bool any_visible = false;
    if (s_card_refuel && !lv_obj_has_flag(s_card_refuel, LV_OBJ_FLAG_HIDDEN)) any_visible = true;
    if (s_card_step_climb && !lv_obj_has_flag(s_card_step_climb, LV_OBJ_FLAG_HIDDEN)) any_visible = true;
    if (s_card_trim_speed && !lv_obj_has_flag(s_card_trim_speed, LV_OBJ_FLAG_HIDDEN)) any_visible = true;
    if (s_card_cg_shift && !lv_obj_has_flag(s_card_cg_shift, LV_OBJ_FLAG_HIDDEN)) any_visible = true;

    s_any_alert_visible = any_visible;
    if (s_nominal_lbl != NULL && lv_obj_is_valid(s_nominal_lbl)) {
        if (any_visible) {
            lv_obj_add_flag(s_nominal_lbl, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(s_nominal_lbl, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (!any_visible && s_warning_icon != NULL && lv_obj_is_valid(s_warning_icon)) {
        lv_obj_add_flag(s_warning_icon, LV_OBJ_FLAG_HIDDEN);
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

static lv_obj_t *create_pilot_alert_card(lv_obj_t *parent,
                                         const char *title,
                                         const char *desc,
                                         lv_obj_t **out_title_lbl,
                                         lv_obj_t **out_desc_lbl)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_add_style(card, ui_style_card_alert(), 0);
    lv_obj_set_size(card, 220, 48);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_pad_all(card, 6, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_HIDDEN);

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

    if (out_title_lbl != NULL) {
        *out_title_lbl = title_lbl;
    }
    if (out_desc_lbl != NULL) {
        *out_desc_lbl = desc_lbl;
    }

    return card;
}

static lv_obj_t *create_section_title(lv_obj_t *parent, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_set_size(lbl, 448, 18);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
    return lbl;
}

static lv_obj_t *create_metric_card(lv_obj_t *parent,
                                    lv_coord_t width,
                                    const char *title,
                                    const char *val,
                                    lv_obj_t **out_val_lbl)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_add_style(card, ui_style_card(), 0);
    lv_obj_set_size(card, width, 50);
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

    s_lbl_ac_co2_saved = lv_label_create(ac_cont);
    lv_label_set_text(s_lbl_ac_co2_saved, "");
    lv_obj_set_style_text_font(s_lbl_ac_co2_saved, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(s_lbl_ac_co2_saved, lv_color_hex(UI_COLOR_HEX_SUCCESS), 0);
    lv_obj_align(s_lbl_ac_co2_saved, LV_ALIGN_TOP_LEFT, 6, 0);
    lv_obj_add_flag(s_lbl_ac_co2_saved, LV_OBJ_FLAG_HIDDEN);

    s_lbl_ac_fuel_saved = lv_label_create(ac_cont);
    lv_label_set_text(s_lbl_ac_fuel_saved, "");
    lv_obj_set_style_text_font(s_lbl_ac_fuel_saved, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(s_lbl_ac_fuel_saved, lv_color_hex(UI_COLOR_HEX_SUCCESS), 0);
    lv_obj_align(s_lbl_ac_fuel_saved, LV_ALIGN_TOP_LEFT, 6, 12);
    lv_obj_add_flag(s_lbl_ac_fuel_saved, LV_OBJ_FLAG_HIDDEN);

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
    lv_obj_add_flag(s_warning_icon, LV_OBJ_FLAG_HIDDEN);

    s_nominal_lbl = lv_label_create(s_alerts_cont);
    lv_label_set_text(s_nominal_lbl, "Nominal Flight Profile");
    lv_obj_set_style_text_font(s_nominal_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_nominal_lbl, lv_color_hex(UI_COLOR_HEX_TEXT_MUTED), 0);
    lv_obj_align(s_nominal_lbl, LV_ALIGN_CENTER, 0, 0);

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

    s_card_refuel = create_pilot_alert_card(
        s_alerts_list,
        "REFUEL TARGET ADJUSTMENT",
        "Pump --- Liters",
        NULL,
        &s_lbl_refuel_desc
    );
    s_card_step_climb = create_pilot_alert_card(
        s_alerts_list,
        "STEP CLIMB TO FL---",
        "Save 0.0 kg/hr",
        &s_lbl_step_climb_title,
        &s_lbl_step_climb_desc
    );
    s_card_trim_speed = create_pilot_alert_card(
        s_alerts_list,
        "TRIM SPEED TO M 0.76",
        "Maximum SAF Range Profile",
        NULL,
        NULL
    );
    s_card_cg_shift = create_pilot_alert_card(
        s_alerts_list,
        "AFT CG SHIFT ADVISORY",
        "Crossfeed Balance Recommended",
        NULL,
        NULL
    );

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

    /* Material 3 Draggable Bottom Sheet */
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
    lv_obj_set_style_pad_row(sheet_body, 6, 0);
    lv_obj_set_style_pad_column(sheet_body, 16, 0);
    lv_obj_set_flex_flow(sheet_body, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(sheet_body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(sheet_body, LV_DIR_VER);

    /* Top Section: System Computation (Edge AI Outputs) */
    (void)create_section_title(sheet_body, "SYSTEM COMPUTATION");

    /* Split Banked Fuel & Carbon Savings Cards */
    s_card_fuel_saved = create_metric_card(sheet_body, 216, "FUEL SAVED", "---", &s_lbl_fuel_saved);
    lv_obj_set_style_bg_color(s_card_fuel_saved, lv_color_hex(UI_COLOR_HEX_SURFACE_HIGH), 0);
    lv_obj_set_style_border_color(s_card_fuel_saved, lv_color_hex(UI_COLOR_HEX_OUTLINE), 0);
    lv_obj_set_style_border_width(s_card_fuel_saved, 1, 0);

    s_card_co2_saved = create_metric_card(sheet_body, 216, "CO2 EMISSION REDUCED", "---", &s_lbl_co2_saved);
    lv_obj_set_style_bg_color(s_card_co2_saved, lv_color_hex(UI_COLOR_HEX_SURFACE_HIGH), 0);
    lv_obj_set_style_border_color(s_card_co2_saved, lv_color_hex(UI_COLOR_HEX_OUTLINE), 0);
    lv_obj_set_style_border_width(s_card_co2_saved, 1, 0);

    (void)create_metric_card(sheet_body, 216, "BASE DENSITY", "---", &s_comp_val_lbls[0]);
    (void)create_metric_card(sheet_body, 216, "LIVE SAF DENSITY", "---", &s_comp_val_lbls[1]);
    (void)create_metric_card(sheet_body, 216, "INDICATED VOLUME", "---", &s_comp_val_lbls[2]);
    (void)create_metric_card(sheet_body, 216, "TRUE FUEL WEIGHT (TFW)", "---", &s_comp_val_lbls[3]);
    (void)create_metric_card(sheet_body, 216, "TRUE GROSS WEIGHT (TGW)", "---", &s_comp_val_lbls[4]);
    (void)create_metric_card(sheet_body, 216, "FMS GW VARIANCE", "---", &s_comp_val_lbls[5]);
    (void)create_metric_card(sheet_body, 448, "TARGET REFUEL VOLUME", "---", &s_comp_val_lbls[6]);

    /* Material 3 Horizontal Divider (1px height, subtle gray) */
    lv_obj_t *m3_divider = lv_obj_create(sheet_body);
    lv_obj_set_size(m3_divider, 448, 1);
    lv_obj_set_style_bg_color(m3_divider, lv_color_hex(UI_COLOR_HEX_OUTLINE), 0);
    lv_obj_set_style_bg_opa(m3_divider, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(m3_divider, 0, 0);
    lv_obj_set_style_radius(m3_divider, 0, 0);
    lv_obj_set_style_pad_all(m3_divider, 0, 0);
    lv_obj_clear_flag(m3_divider, LV_OBJ_FLAG_SCROLLABLE);

    /* Bottom Section: Avionics Data (Raw HIL Telemetry) */
    (void)create_section_title(sheet_body, "AVIONICS DATA");

    (void)create_metric_card(sheet_body, 216, "ALTITUDE", "---", &s_raw_val_lbls[0]);
    (void)create_metric_card(sheet_body, 216, "MACH SPEED", "---", &s_raw_val_lbls[1]);
    (void)create_metric_card(sheet_body, 216, "OAT (OUTSIDE TEMP)", "---", &s_raw_val_lbls[2]);
    (void)create_metric_card(sheet_body, 216, "FUEL TEMP", "---", &s_raw_val_lbls[3]);
    (void)create_metric_card(sheet_body, 216, "HEADING", "---", &s_raw_val_lbls[4]);
    (void)create_metric_card(sheet_body, 216, "LEFT TANK", "---", &s_raw_val_lbls[5]);
    (void)create_metric_card(sheet_body, 216, "RIGHT TANK", "---", &s_raw_val_lbls[6]);
    (void)create_metric_card(sheet_body, 216, "TOTAL WEIGHT", "---", &s_raw_val_lbls[7]);

    s_sheet_expanded = false;

    screen_dashboard_refresh();

    return s_scr_dashboard;
}
