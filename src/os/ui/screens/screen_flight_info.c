/*!
    \file    src/os/ui/screens/screen_flight_info.c
    \brief   Saved Flight Info / Overview Viewer Screen Implementation
*/

#include "screen_flight_info.h"
#include "ui_manager.h"
#include "ui_theme.h"
#include "ui_topbar.h"
#include "flight_store.h"
#include "configs/allowed_flights.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

static void topbar_back_cb(void)
{
    ui_manager_navigate_to(SCREEN_HOME, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
}

static void start_flight_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_manager_navigate_to(SCREEN_CONNECT, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }
}

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

static void format_flight_duration_long(const char *dep_str, const char *arr_str, char *out_buf, size_t out_len)
{
    int dep_min = parse_time_minutes(dep_str);
    int arr_min = parse_time_minutes(arr_str);

    if (dep_min < 0 || arr_min < 0) {
        snprintf(out_buf, out_len, "1hr 30mins");
        return;
    }

    int diff = arr_min - dep_min;
    if (diff <= 0) {
        diff += 24 * 60;
    }

    int hrs = diff / 60;
    int mins = diff % 60;
    snprintf(out_buf, out_len, "%dhr %dmins", hrs, mins);
}

static lv_obj_t *create_overview_card(lv_obj_t *parent,
                                      lv_coord_t width,
                                      const char *title,
                                      const char *val_str,
                                      uint32_t val_color_hex,
                                      bool highlight_border,
                                      uint32_t border_color_hex)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_add_style(card, ui_style_card(), 0);
    lv_obj_set_size(card, width, 48);
    lv_obj_set_style_pad_all(card, 6, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    if (highlight_border) {
        lv_obj_set_style_bg_color(card, lv_color_hex(UI_COLOR_HEX_SURFACE_HIGH), 0);
        lv_obj_set_style_border_color(card, lv_color_hex(border_color_hex), 0);
        lv_obj_set_style_border_width(card, 1, 0);
    }

    lv_obj_t *title_lbl = lv_label_create(card);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(UI_COLOR_HEX_TEXT_MUTED), 0);
    lv_obj_set_pos(title_lbl, 4, 1);

    lv_obj_t *val_lbl = lv_label_create(card);
    lv_label_set_text(val_lbl, val_str);
    lv_obj_set_style_text_font(val_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(val_lbl, lv_color_hex(val_color_hex), 0);
    lv_obj_set_pos(val_lbl, 4, 17);

    return card;
}

lv_obj_t *screen_flight_info_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(UI_COLOR_HEX_SURFACE), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    const flight_record_t *f = ui_manager_get_active_flight();
    const char *flight_no = (f != NULL && f->flight_no[0] != '\0') ? f->flight_no : "5J 383";
    const char *origin    = (f != NULL && f->origin[0] != '\0')    ? f->origin    : "MNL";
    const char *dest      = (f != NULL && f->dest[0] != '\0')      ? f->dest      : "CGY";
    const char *dep_time  = (f != NULL && f->depart_time[0] != '\0') ? f->depart_time : "4:10 AM";
    const char *arr_time  = (f != NULL && f->arrive_time[0] != '\0') ? f->arrive_time : "5:40 AM";
    const char *ac_type   = (f != NULL && f->aircraft_type[0] != '\0') ? f->aircraft_type : "A320N";

    const airline_info_t *airline = airline_get_info_by_flight_no(flight_no);
    const char *carrier = (f != NULL && f->carrier[0] != '\0')
                              ? f->carrier
                              : ((airline != NULL && airline->name != NULL) ? airline->name : "Aviation");

    (void)ui_topbar_create(scr, "Flight Info", true, topbar_back_cb, flight_no);

    /* Main Scrollable Content Area */
    lv_obj_t *body = lv_obj_create(scr);
    lv_obj_set_size(body, 480, 234);
    lv_obj_set_pos(body, 0, 38);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_hor(body, 16, 0);
    lv_obj_set_style_pad_top(body, 10, 0);
    lv_obj_set_style_pad_bottom(body, 18, 0);
    lv_obj_set_style_pad_row(body, 8, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);

    /* Top Header Row: Large Circular Airline Icon + Route/Duration/Carrier + Start Telemetry Button */
    lv_obj_t *header_row = lv_obj_create(body);
    lv_obj_set_size(header_row, 448, 76);
    lv_obj_set_style_bg_opa(header_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header_row, 0, 0);
    lv_obj_set_style_pad_all(header_row, 0, 0);
    lv_obj_clear_flag(header_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon_circle = lv_obj_create(header_row);
    lv_obj_set_size(icon_circle, 72, 72);
    lv_obj_align(icon_circle, LV_ALIGN_LEFT_MID, 0, 0);
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

    char route_str[40];
    snprintf(route_str, sizeof(route_str), "%s -> %s", origin, dest);
    lv_obj_t *lbl_route = lv_label_create(header_row);
    lv_label_set_text(lbl_route, route_str);
    lv_obj_set_style_text_font(lbl_route, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_route, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
    lv_obj_set_pos(lbl_route, 86, 4);

    char dur_str[32];
    format_flight_duration_long(dep_time, arr_time, dur_str, sizeof(dur_str));
    lv_obj_t *lbl_dur = lv_label_create(header_row);
    lv_label_set_text(lbl_dur, dur_str);
    lv_obj_set_style_text_font(lbl_dur, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_dur, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
    lv_obj_set_pos(lbl_dur, 86, 30);

    char sub_str[80];
    snprintf(sub_str, sizeof(sub_str), "%s | %s", carrier, ac_type);
    lv_obj_t *lbl_sub = lv_label_create(header_row);
    lv_label_set_text(lbl_sub, sub_str);
    lv_obj_set_style_text_font(lbl_sub, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_sub, lv_color_hex(UI_COLOR_HEX_TEXT_SECONDARY), 0);
    lv_obj_set_pos(lbl_sub, 86, 49);

    lv_obj_t *btn_start = lv_btn_create(header_row);
    lv_obj_add_style(btn_start, ui_style_btn_primary(), 0);
    lv_obj_set_size(btn_start, 96, 34);
    lv_obj_align(btn_start, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(btn_start, start_flight_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_start = lv_label_create(btn_start);
    lv_label_set_text(lbl_start, LV_SYMBOL_PLAY " Start");
    lv_obj_set_style_text_font(lbl_start, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_start);

    /* Horizontal Divider Line */
    lv_obj_t *divider = lv_obj_create(body);
    lv_obj_set_size(divider, 448, 1);
    lv_obj_set_style_bg_color(divider, lv_color_hex(UI_COLOR_HEX_OUTLINE), 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_set_style_radius(divider, 0, 0);
    lv_obj_set_style_pad_all(divider, 0, 0);
    lv_obj_clear_flag(divider, LV_OBJ_FLAG_SCROLLABLE);

    /* Section Title: Flight Overview */
    lv_obj_t *lbl_overview = lv_label_create(body);
    lv_label_set_text(lbl_overview, "Flight Overview");
    lv_obj_set_style_text_font(lbl_overview, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_overview, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);

    /* 2-Column Overview Grid */
    lv_obj_t *grid = lv_obj_create(body);
    lv_obj_set_size(grid, 448, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_style_pad_row(grid, 6, 0);
    lv_obj_set_style_pad_column(grid, 16, 0);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    char buf1[80], buf2[80];

    /* FIRST 2 CARDS AT THE TOP: FUEL SAVED & CARBON EMISSION REDUCED */
    double fuel_saved = (f != NULL) ? f->fuel_saved_kg : 0.0;
    double co2_saved  = (f != NULL) ? f->co2_reduced_kg : 0.0;
    snprintf(buf1, sizeof(buf1), "%.2f kg", fuel_saved);
    snprintf(buf2, sizeof(buf2), "%.2f kg", co2_saved);

    (void)create_overview_card(grid, 216, "FUEL SAVED", buf1,
                               UI_COLOR_HEX_SUCCESS, true, UI_COLOR_HEX_SUCCESS);
    (void)create_overview_card(grid, 216, "CARBON EMISSION REDUCED", buf2,
                               UI_COLOR_HEX_SUCCESS, true, UI_COLOR_HEX_SUCCESS);

    /* Row 2: True Fuel Weight & True Gross Weight */
    snprintf(buf1, sizeof(buf1), "%.1f kg", (double)((f != NULL) ? f->true_fuel_weight_kg : 0.0f));
    snprintf(buf2, sizeof(buf2), "%.1f kg", (double)((f != NULL) ? f->true_gross_weight_kg : 0.0f));
    (void)create_overview_card(grid, 216, "TRUE FUEL WEIGHT", buf1,
                               UI_COLOR_HEX_TEXT_PRIMARY, false, 0);
    (void)create_overview_card(grid, 216, "TRUE GROSS WEIGHT", buf2,
                               UI_COLOR_HEX_TEXT_PRIMARY, false, 0);

    /* Row 3: FMS GW Variance & Target Refuel Volume */
    float var_kg = (f != NULL) ? f->fms_gw_variance_kg : 0.0f;
    snprintf(buf1, sizeof(buf1), "%+.1f kg", (double)var_kg);
    snprintf(buf2, sizeof(buf2), "%.1f L", (double)((f != NULL) ? f->target_refuel_volume_l : 0.0f));
    (void)create_overview_card(grid, 216, "FMS GW VARIANCE", buf1,
                               (fabsf(var_kg) > 300.0f) ? UI_COLOR_HEX_WARNING : UI_COLOR_HEX_TEXT_PRIMARY,
                               false, 0);
    (void)create_overview_card(grid, 216, "TARGET REFUEL VOLUME", buf2,
                               UI_COLOR_HEX_PRIMARY, false, 0);

    /* Row 4: Live SAF Density & Base Density */
    snprintf(buf1, sizeof(buf1), "%.4f kg/L", (double)((f != NULL) ? f->live_saf_density : 0.0f));
    snprintf(buf2, sizeof(buf2), "%.4f kg/L", (double)((f != NULL) ? f->base_density : 0.0f));
    (void)create_overview_card(grid, 216, "LIVE SAF DENSITY", buf1,
                               UI_COLOR_HEX_TEXT_PRIMARY, false, 0);
    (void)create_overview_card(grid, 216, "BASE DENSITY", buf2,
                               UI_COLOR_HEX_TEXT_PRIMARY, false, 0);

    /* Row 5: Indicated Volume & Optimal Altitude */
    snprintf(buf1, sizeof(buf1), "%.1f L", (double)((f != NULL) ? f->indicated_volume_l : 0.0f));
    if (f != NULL && f->target_fl > 0) {
        snprintf(buf2, sizeof(buf2), "%.0f ft (FL%d)", (double)f->optimal_alt_ft, f->target_fl);
    } else {
        snprintf(buf2, sizeof(buf2), "0 ft");
    }
    (void)create_overview_card(grid, 216, "INDICATED VOLUME", buf1,
                               UI_COLOR_HEX_TEXT_PRIMARY, false, 0);
    (void)create_overview_card(grid, 216, "OPTIMAL ALTITUDE", buf2,
                               UI_COLOR_HEX_TEXT_PRIMARY, false, 0);

    /* Row 6: Fuel Blend Ratio & Zero Fuel Weight */
    uint8_t saf_pct = (f != NULL) ? f->saf_blend_pct : 0U;
    snprintf(buf1, sizeof(buf1), "%u%% SAF / %u%% JET-A1",
             (unsigned int)saf_pct, (unsigned int)(100U - saf_pct));
    snprintf(buf2, sizeof(buf2), "%lu kg", (unsigned long)((f != NULL) ? f->base_zfw_kg : 0U));
    (void)create_overview_card(grid, 216, "FUEL BLEND RATIO", buf1,
                               UI_COLOR_HEX_PRIMARY, false, 0);
    (void)create_overview_card(grid, 216, "ZERO FUEL WEIGHT", buf2,
                               UI_COLOR_HEX_TEXT_PRIMARY, false, 0);

    /* Row 7: Planned Fuel & Schedule */
    snprintf(buf1, sizeof(buf1), "%lu L", (unsigned long)((f != NULL) ? f->planned_fuel_l : 0U));
    snprintf(buf2, sizeof(buf2), "%s - %s", dep_time, arr_time);
    (void)create_overview_card(grid, 216, "PLANNED FUEL", buf1,
                               UI_COLOR_HEX_TEXT_PRIMARY, false, 0);
    (void)create_overview_card(grid, 216, "SCHEDULE", buf2,
                               UI_COLOR_HEX_TEXT_PRIMARY, false, 0);

    /* Row 8: Created Timestamp & Last Reading Timestamp */
    const char *ts_created = (f != NULL && f->timestamp_created[0] != '\0') ? f->timestamp_created : "N/A";
    const char *ts_last    = (f != NULL && f->timestamp_last_reading[0] != '\0') ? f->timestamp_last_reading : "N/A";
    (void)create_overview_card(grid, 216, "CREATED", ts_created,
                               UI_COLOR_HEX_TEXT_SECONDARY, false, 0);
    (void)create_overview_card(grid, 216, "LAST READING", ts_last,
                               UI_COLOR_HEX_TEXT_SECONDARY, false, 0);

    return scr;
}

