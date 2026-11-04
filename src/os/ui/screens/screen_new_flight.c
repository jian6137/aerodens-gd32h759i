/*!
    \file    src/os/ui/screen_new_flight.c
    \brief   Screen 2: Create New Flight Form Implementation
             1. Fuel Blend Ratio (0-100% SAF / JET-A1 full-width slider, default 0% SAF)
             2. Airline Selector Dropdown with circular airline icons (PR, 5J, TG)
             3. Flight Route Dropdown (populated per airline; "Select an Airline to Continue" initially)
             4. Aircraft Type Dropdown
             5. Zero Fuel Weight: ( - ) [ text ] kg ( + ) with Number-only IME popup
             6. Planned Fuel: ( - ) [ text ] l ( + ) with Number-only IME popup
*/

#include "screen_new_flight.h"
#include "ui_manager.h"
#include "ui_theme.h"
#include "ui_topbar.h"
#include "flight_store.h"
#include "configs/allowed_flights.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static lv_obj_t *s_form = NULL;
static lv_obj_t *s_lbl_saf_pct = NULL;
static lv_obj_t *s_lbl_jeta1_pct = NULL;
static lv_obj_t *s_slider_saf = NULL;

static lv_obj_t *s_airline_btn = NULL;
static lv_obj_t *s_airline_icon_cont = NULL;
static lv_obj_t *s_airline_lbl = NULL;
static lv_obj_t *s_airline_chevron = NULL;
static lv_obj_t *s_airline_menu = NULL;

static lv_obj_t *s_dd_flight = NULL;
static lv_obj_t *s_dd_aircraft = NULL;
static lv_obj_t *s_ta_zfw = NULL;
static lv_obj_t *s_ta_fuel = NULL;
static lv_obj_t *s_num_kb = NULL;

static uint8_t  s_saf_blend_pct = 0U;
static int      s_selected_airline_id = -1;
static uint32_t s_filtered_flight_indices[SCHEDULED_FLIGHT_COUNT];
static uint32_t s_filtered_flight_count = 0U;
static uint32_t s_selected_route_pos = 0U;
static uint32_t s_curr_zfw_kg = 62400U;
static uint32_t s_curr_fuel_l = 6800U;

static const char * const s_num_kb_map[] = {
    "1", "2", "3", LV_SYMBOL_BACKSPACE, "\n",
    "4", "5", "6", "0", "\n",
    "7", "8", "9", LV_SYMBOL_OK, ""
};

static const lv_btnmatrix_ctrl_t s_num_kb_ctrl[] = {
    1, 1, 1, 1,
    1, 1, 1, 1,
    1, 1, 1, LV_BTNMATRIX_CTRL_CHECKED | 1
};

static void topbar_back_cb(void)
{
    ui_manager_navigate_to(SCREEN_HOME, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
}

static void sync_textareas_from_values(void)
{
    char buf[32];
    if (s_ta_zfw != NULL) {
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)s_curr_zfw_kg);
        lv_textarea_set_text(s_ta_zfw, buf);
    }
    if (s_ta_fuel != NULL) {
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)s_curr_fuel_l);
        lv_textarea_set_text(s_ta_fuel, buf);
    }
}

static void parse_values_from_textareas(void)
{
    if (s_ta_zfw != NULL) {
        const char *txt = lv_textarea_get_text(s_ta_zfw);
        if (txt != NULL && txt[0] != '\0') {
            unsigned long val = strtoul(txt, NULL, 10);
            if (val >= 500UL && val <= 500000UL) {
                s_curr_zfw_kg = (uint32_t)val;
            }
        }
    }
    if (s_ta_fuel != NULL) {
        const char *txt = lv_textarea_get_text(s_ta_fuel);
        if (txt != NULL && txt[0] != '\0') {
            unsigned long val = strtoul(txt, NULL, 10);
            if (val >= 50UL && val <= 300000UL) {
                s_curr_fuel_l = (uint32_t)val;
            }
        }
    }
    sync_textareas_from_values();
}

static void hide_num_ime(void)
{
    if (s_num_kb != NULL && !lv_obj_has_flag(s_num_kb, LV_OBJ_FLAG_HIDDEN)) {
        parse_values_from_textareas();
        lv_keyboard_set_textarea(s_num_kb, NULL);
        lv_obj_add_flag(s_num_kb, LV_OBJ_FLAG_HIDDEN);
        if (s_form != NULL) {
            lv_obj_set_style_pad_bottom(s_form, 14, 0);
        }
    }
}

static void show_num_ime_for(lv_obj_t *ta)
{
    if (s_num_kb == NULL || ta == NULL) {
        return;
    }
    if (s_form != NULL) {
        lv_obj_set_style_pad_bottom(s_form, 128, 0);
    }
    lv_keyboard_set_textarea(s_num_kb, ta);
    lv_obj_clear_flag(s_num_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_num_kb);
    lv_obj_scroll_to_view_recursive(ta, LV_ANIM_ON);
}

static void saf_slider_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        int32_t val = lv_slider_get_value(s_slider_saf);
        if (val < 0) val = 0;
        if (val > 100) val = 100;
        s_saf_blend_pct = (uint8_t)val;

        char saf_str[24];
        char jet_str[24];
        snprintf(saf_str, sizeof(saf_str), "SAF - %u%%", (unsigned int)s_saf_blend_pct);
        snprintf(jet_str, sizeof(jet_str), "JET-A1 - %u%%", (unsigned int)(100U - s_saf_blend_pct));

        if (s_lbl_saf_pct != NULL) {
            lv_label_set_text(s_lbl_saf_pct, saf_str);
        }
        if (s_lbl_jeta1_pct != NULL) {
            lv_label_set_text(s_lbl_jeta1_pct, jet_str);
        }
    }
}

static void render_airline_badge_contents(lv_obj_t *circle_cont, const airline_info_t *info)
{
    lv_obj_clean(circle_cont);
    if (info != NULL && info->icon != NULL) {
        lv_obj_t *img = lv_img_create(circle_cont);
        lv_img_set_src(img, info->icon);
        lv_obj_center(img);
        lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(img, LV_OBJ_FLAG_EVENT_BUBBLE);
    } else {
        lv_obj_t *lbl = lv_label_create(circle_cont);
        lv_label_set_text(lbl, (info != NULL) ? info->code : "--");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
        lv_obj_center(lbl);
        lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(lbl, LV_OBJ_FLAG_EVENT_BUBBLE);
    }
}

static void apply_selected_route(void)
{
    if (s_filtered_flight_count == 0U || s_selected_route_pos >= s_filtered_flight_count) {
        return;
    }

    uint32_t global_idx = s_filtered_flight_indices[s_selected_route_pos];
    const scheduled_flight_t *f = &k_scheduled_flights[global_idx];

    s_curr_zfw_kg = f->base_zfw_kg;
    s_curr_fuel_l = f->planned_fuel_l;
    sync_textareas_from_values();

    if (s_dd_aircraft != NULL && f->default_ac != NULL) {
        for (size_t i = 0; i < AIRCRAFT_TYPE_COUNT; ++i) {
            if (strcmp(k_aircraft_types[i].code, f->default_ac) == 0) {
                lv_dropdown_set_selected(s_dd_aircraft, (uint16_t)i);
                break;
            }
        }
    }
}

static void populate_flights_for_airline(int airline_id)
{
    s_filtered_flight_count = 0U;
    s_selected_route_pos = 0U;

    if (s_dd_flight == NULL) {
        return;
    }

    if (airline_id < 0 || airline_id >= (int)AIRLINE_COUNT) {
        lv_dropdown_set_options(s_dd_flight, "Select an Airline to Continue");
        lv_obj_add_state(s_dd_flight, LV_STATE_DISABLED);
        return;
    }

    char options_buf[1280];
    options_buf[0] = '\0';

    for (uint32_t i = 0U; i < SCHEDULED_FLIGHT_COUNT; ++i) {
        if ((int)k_scheduled_flights[i].airline == airline_id) {
            s_filtered_flight_indices[s_filtered_flight_count++] = i;
        }
    }

    if (s_filtered_flight_count == 0U) {
        lv_dropdown_set_options(s_dd_flight, "No Scheduled Flights");
        lv_obj_add_state(s_dd_flight, LV_STATE_DISABLED);
        return;
    }

    for (uint32_t k = 0U; k < s_filtered_flight_count; ++k) {
        const scheduled_flight_t *f = &k_scheduled_flights[s_filtered_flight_indices[k]];
        char line[80];
        snprintf(line, sizeof(line), "%s  (%s -> %s)  %s - %s%s",
                 f->flight_no, f->origin, f->dest, f->depart_time, f->arrive_time,
                 (k + 1U < s_filtered_flight_count) ? "\n" : "");
        strncat(options_buf, line, sizeof(options_buf) - strlen(options_buf) - 1U);
    }

    lv_obj_clear_state(s_dd_flight, LV_STATE_DISABLED);
    lv_dropdown_set_options(s_dd_flight, options_buf);
    lv_dropdown_set_selected(s_dd_flight, 0);
    apply_selected_route();
}

static void airline_btn_toggle_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        hide_num_ime();
        if (s_airline_menu == NULL) {
            return;
        }
        bool is_hidden = lv_obj_has_flag(s_airline_menu, LV_OBJ_FLAG_HIDDEN);
        if (is_hidden) {
            lv_obj_clear_flag(s_airline_menu, LV_OBJ_FLAG_HIDDEN);
            if (s_airline_chevron != NULL) {
                lv_label_set_text(s_airline_chevron, LV_SYMBOL_UP);
            }
        } else {
            lv_obj_add_flag(s_airline_menu, LV_OBJ_FLAG_HIDDEN);
            if (s_airline_chevron != NULL) {
                lv_label_set_text(s_airline_chevron, LV_SYMBOL_DOWN);
            }
        }
    }
}

static void airline_option_select_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        hide_num_ime();
        int airline_id = (int)(intptr_t)lv_event_get_user_data(e);
        if (airline_id < 0 || airline_id >= (int)AIRLINE_COUNT) {
            return;
        }

        s_selected_airline_id = airline_id;
        const airline_info_t *info = &k_allowed_airlines[airline_id];

        if (s_airline_icon_cont != NULL) {
            render_airline_badge_contents(s_airline_icon_cont, info);
        }
        if (s_airline_lbl != NULL) {
            lv_label_set_text(s_airline_lbl, info->name);
            lv_obj_set_style_text_color(s_airline_lbl, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
        }
        if (s_airline_menu != NULL) {
            lv_obj_add_flag(s_airline_menu, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_airline_chevron != NULL) {
            lv_label_set_text(s_airline_chevron, LV_SYMBOL_DOWN);
        }

        populate_flights_for_airline(airline_id);
    }
}

static void flight_dropdown_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        hide_num_ime();
        s_selected_route_pos = lv_dropdown_get_selected(s_dd_flight);
        apply_selected_route();
    }
}

static void zfw_step_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        parse_values_from_textareas();
        int delta = (int)(intptr_t)lv_event_get_user_data(e);
        if (delta < 0 && s_curr_zfw_kg <= 1000U) {
            return;
        }
        s_curr_zfw_kg = (uint32_t)((int)s_curr_zfw_kg + delta);
        sync_textareas_from_values();
    }
}

static void fuel_step_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        parse_values_from_textareas();
        int delta = (int)(intptr_t)lv_event_get_user_data(e);
        if (delta < 0 && s_curr_fuel_l <= 200U) {
            return;
        }
        s_curr_fuel_l = (uint32_t)((int)s_curr_fuel_l + delta);
        sync_textareas_from_values();
    }
}

static void textarea_click_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);
    if (code == LV_EVENT_CLICKED) {
        show_num_ime_for(ta);
    } else if (code == LV_EVENT_DEFOCUSED) {
        parse_values_from_textareas();
    }
}

static void num_kb_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        hide_num_ime();
    }
}

static void next_btn_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        hide_num_ime();

        if (s_selected_airline_id < 0 || s_filtered_flight_count == 0U) {
            if (s_airline_menu != NULL && lv_obj_has_flag(s_airline_menu, LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_clear_flag(s_airline_menu, LV_OBJ_FLAG_HIDDEN);
                if (s_airline_chevron != NULL) {
                    lv_label_set_text(s_airline_chevron, LV_SYMBOL_UP);
                }
            }
            if (s_airline_btn != NULL) {
                lv_obj_scroll_to_view_recursive(s_airline_btn, LV_ANIM_ON);
            }
            return;
        }

        parse_values_from_textareas();

        uint32_t global_idx = s_filtered_flight_indices[
            (s_selected_route_pos < s_filtered_flight_count) ? s_selected_route_pos : 0U
        ];
        const scheduled_flight_t *f = &k_scheduled_flights[global_idx];

        uint16_t ac_sel = (s_dd_aircraft != NULL) ? lv_dropdown_get_selected(s_dd_aircraft) : 0U;
        const aircraft_type_info_t *ac = &k_aircraft_types[ac_sel < AIRCRAFT_TYPE_COUNT ? ac_sel : 0U];

        flight_record_t rec;
        memset(&rec, 0, sizeof(rec));
        if (s_selected_airline_id >= 0 && s_selected_airline_id < (int)AIRLINE_COUNT) {
            strncpy(rec.carrier, k_allowed_airlines[s_selected_airline_id].name, sizeof(rec.carrier) - 1U);
        }
        strncpy(rec.flight_no, f->flight_no, sizeof(rec.flight_no) - 1U);
        strncpy(rec.origin, f->origin, sizeof(rec.origin) - 1U);
        strncpy(rec.dest, f->dest, sizeof(rec.dest) - 1U);
        strncpy(rec.aircraft_type, ac->code, sizeof(rec.aircraft_type) - 1U);
        strncpy(rec.depart_time, f->depart_time, sizeof(rec.depart_time) - 1U);
        strncpy(rec.arrive_time, f->arrive_time, sizeof(rec.arrive_time) - 1U);
        rec.base_zfw_kg = s_curr_zfw_kg;
        rec.planned_fuel_l = s_curr_fuel_l;
        rec.saf_blend_pct = s_saf_blend_pct;
        rec.is_completed = false;

        ui_manager_set_active_flight(&rec);
        ui_manager_navigate_to(SCREEN_CONFIRM_FLIGHT, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }
}

static void new_flight_delete_cb(lv_event_t *e)
{
    (void)e;
    s_form = NULL;
    s_lbl_saf_pct = NULL;
    s_lbl_jeta1_pct = NULL;
    s_slider_saf = NULL;
    s_airline_btn = NULL;
    s_airline_icon_cont = NULL;
    s_airline_lbl = NULL;
    s_airline_chevron = NULL;
    s_airline_menu = NULL;
    s_dd_flight = NULL;
    s_dd_aircraft = NULL;
    s_ta_zfw = NULL;
    s_ta_fuel = NULL;
    s_num_kb = NULL;
}

static lv_obj_t *create_section_header(lv_obj_t *parent, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
    return lbl;
}

lv_obj_t *screen_new_flight_create(void)
{
    s_saf_blend_pct = 0U;
    s_selected_airline_id = -1;
    s_filtered_flight_count = 0U;
    s_selected_route_pos = 0U;
    s_curr_zfw_kg = 62400U;
    s_curr_fuel_l = 6800U;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(UI_COLOR_HEX_SURFACE), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scr, new_flight_delete_cb, LV_EVENT_DELETE, NULL);

    (void)ui_topbar_create(scr, "New Flight", true, topbar_back_cb, NULL);

    s_form = lv_obj_create(scr);
    lv_obj_set_size(s_form, 480, 234);
    lv_obj_set_pos(s_form, 0, 38);
    lv_obj_set_style_bg_opa(s_form, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_form, 0, 0);
    lv_obj_set_style_pad_hor(s_form, 16, 0);
    lv_obj_set_style_pad_top(s_form, 10, 0);
    lv_obj_set_style_pad_bottom(s_form, 14, 0);
    lv_obj_set_style_pad_row(s_form, 6, 0);
    lv_obj_set_flex_flow(s_form, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_form, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(s_form, LV_DIR_VER);

    (void)create_section_header(s_form, "1. Fuel Blend Ratio");

    lv_obj_t *saf_card = lv_obj_create(s_form);
    lv_obj_add_style(saf_card, ui_style_card(), 0);
    lv_obj_set_size(saf_card, 444, 58);
    lv_obj_set_style_pad_all(saf_card, 8, 0);
    lv_obj_clear_flag(saf_card, LV_OBJ_FLAG_SCROLLABLE);

    s_lbl_saf_pct = lv_label_create(saf_card);
    lv_label_set_text(s_lbl_saf_pct, "SAF - 0%");
    lv_obj_set_style_text_font(s_lbl_saf_pct, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_lbl_saf_pct, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
    lv_obj_align(s_lbl_saf_pct, LV_ALIGN_TOP_LEFT, 4, 0);

    s_lbl_jeta1_pct = lv_label_create(saf_card);
    lv_label_set_text(s_lbl_jeta1_pct, "JET-A1 - 100%");
    lv_obj_set_style_text_font(s_lbl_jeta1_pct, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_lbl_jeta1_pct, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
    lv_obj_align(s_lbl_jeta1_pct, LV_ALIGN_TOP_RIGHT, -4, 0);

    s_slider_saf = lv_slider_create(saf_card);
    lv_obj_set_size(s_slider_saf, 412, 10);
    lv_obj_align(s_slider_saf, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_slider_set_range(s_slider_saf, 0, 100);
    lv_slider_set_value(s_slider_saf, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_slider_saf, lv_color_hex(UI_COLOR_HEX_PRIMARY_CONTAINER), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_slider_saf, lv_color_hex(UI_COLOR_HEX_PRIMARY), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider_saf, lv_color_hex(UI_COLOR_HEX_PRIMARY), LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_slider_saf, 4, LV_PART_KNOB);
    lv_obj_add_event_cb(s_slider_saf, saf_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    (void)create_section_header(s_form, "2. Airline");

    s_airline_btn = lv_obj_create(s_form);
    lv_obj_add_style(s_airline_btn, ui_style_card(), 0);
    lv_obj_set_size(s_airline_btn, 444, 46);
    lv_obj_set_style_pad_hor(s_airline_btn, 8, 0);
    lv_obj_set_style_pad_ver(s_airline_btn, 4, 0);
    lv_obj_clear_flag(s_airline_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_airline_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_airline_btn, airline_btn_toggle_cb, LV_EVENT_CLICKED, NULL);

    s_airline_icon_cont = lv_obj_create(s_airline_btn);
    lv_obj_set_size(s_airline_icon_cont, 36, 36);
    lv_obj_align(s_airline_icon_cont, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_radius(s_airline_icon_cont, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_airline_icon_cont, lv_color_hex(UI_COLOR_HEX_SURFACE_HIGH), 0);
    lv_obj_set_style_border_color(s_airline_icon_cont, lv_color_hex(UI_COLOR_HEX_OUTLINE), 0);
    lv_obj_set_style_border_width(s_airline_icon_cont, 1, 0);
    lv_obj_set_style_pad_all(s_airline_icon_cont, 0, 0);
    lv_obj_clear_flag(s_airline_icon_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_airline_icon_cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_airline_icon_cont, LV_OBJ_FLAG_EVENT_BUBBLE);
    render_airline_badge_contents(s_airline_icon_cont, NULL);

    s_airline_lbl = lv_label_create(s_airline_btn);
    lv_label_set_text(s_airline_lbl, "Select an Airline...");
    lv_obj_set_style_text_font(s_airline_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_airline_lbl, lv_color_hex(UI_COLOR_HEX_TEXT_MUTED), 0);
    lv_obj_align(s_airline_lbl, LV_ALIGN_LEFT_MID, 46, 0);
    lv_obj_clear_flag(s_airline_lbl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_airline_lbl, LV_OBJ_FLAG_EVENT_BUBBLE);

    s_airline_chevron = lv_label_create(s_airline_btn);
    lv_label_set_text(s_airline_chevron, LV_SYMBOL_DOWN);
    lv_obj_set_style_text_font(s_airline_chevron, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_airline_chevron, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
    lv_obj_align(s_airline_chevron, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_clear_flag(s_airline_chevron, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_airline_chevron, LV_OBJ_FLAG_EVENT_BUBBLE);

    s_airline_menu = lv_obj_create(s_form);
    lv_obj_add_style(s_airline_menu, ui_style_card(), 0);
    lv_obj_set_size(s_airline_menu, 444, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(s_airline_menu, lv_color_hex(UI_COLOR_HEX_SURFACE_HIGH), 0);
    lv_obj_set_style_pad_all(s_airline_menu, 6, 0);
    lv_obj_set_style_pad_row(s_airline_menu, 4, 0);
    lv_obj_set_flex_flow(s_airline_menu, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(s_airline_menu, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_airline_menu, LV_OBJ_FLAG_HIDDEN);

    static const airline_id_t k_menu_order[AIRLINE_COUNT] = {
        AIRLINE_PR,
        AIRLINE_5J,
        AIRLINE_TG
    };

    for (uint32_t m = 0U; m < AIRLINE_COUNT; ++m) {
        airline_id_t aid = k_menu_order[m];
        const airline_info_t *ainfo = &k_allowed_airlines[aid];

        lv_obj_t *row = lv_obj_create(s_airline_menu);
        lv_obj_set_size(row, 428, 42);
        lv_obj_set_style_bg_color(row, lv_color_hex(UI_COLOR_HEX_SURFACE_CONTAINER), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(UI_COLOR_HEX_OUTLINE), 0);
        lv_obj_set_style_pad_hor(row, 8, 0);
        lv_obj_set_style_pad_ver(row, 2, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, airline_option_select_cb, LV_EVENT_CLICKED, (void *)(intptr_t)aid);

        lv_obj_t *circle = lv_obj_create(row);
        lv_obj_set_size(circle, 36, 36);
        lv_obj_align(circle, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(circle, lv_color_hex(UI_COLOR_HEX_SURFACE_HIGH), 0);
        lv_obj_set_style_border_color(circle, lv_color_hex(UI_COLOR_HEX_OUTLINE), 0);
        lv_obj_set_style_border_width(circle, 1, 0);
        lv_obj_set_style_pad_all(circle, 0, 0);
        lv_obj_clear_flag(circle, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(circle, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(circle, LV_OBJ_FLAG_EVENT_BUBBLE);
        render_airline_badge_contents(circle, ainfo);

        lv_obj_t *rlbl = lv_label_create(row);
        lv_label_set_text(rlbl, ainfo->name);
        lv_obj_set_style_text_font(rlbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(rlbl, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
        lv_obj_align(rlbl, LV_ALIGN_LEFT_MID, 46, 0);
        lv_obj_clear_flag(rlbl, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(rlbl, LV_OBJ_FLAG_EVENT_BUBBLE);
    }

    (void)create_section_header(s_form, "3. Flight Route");

    s_dd_flight = lv_dropdown_create(s_form);
    lv_obj_set_size(s_dd_flight, 444, 38);
    lv_obj_set_style_bg_color(s_dd_flight, lv_color_hex(UI_COLOR_HEX_SURFACE_CONTAINER), 0);
    lv_obj_set_style_border_color(s_dd_flight, lv_color_hex(UI_COLOR_HEX_OUTLINE), 0);
    lv_obj_set_style_text_color(s_dd_flight, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(s_dd_flight, &lv_font_montserrat_12, 0);
    lv_dropdown_set_options(s_dd_flight, "Select an Airline to Continue");
    lv_obj_add_state(s_dd_flight, LV_STATE_DISABLED);
    lv_obj_add_event_cb(s_dd_flight, flight_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    (void)create_section_header(s_form, "4. Aircraft Type");

    s_dd_aircraft = lv_dropdown_create(s_form);
    lv_obj_set_size(s_dd_aircraft, 444, 38);
    lv_obj_set_style_bg_color(s_dd_aircraft, lv_color_hex(UI_COLOR_HEX_SURFACE_CONTAINER), 0);
    lv_obj_set_style_border_color(s_dd_aircraft, lv_color_hex(UI_COLOR_HEX_OUTLINE), 0);
    lv_obj_set_style_text_color(s_dd_aircraft, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(s_dd_aircraft, &lv_font_montserrat_12, 0);

    char ac_buf[256] = {0};
    for (size_t i = 0; i < AIRCRAFT_TYPE_COUNT; ++i) {
        char item[64];
        snprintf(item, sizeof(item), "%s (%s)%s",
                 k_aircraft_types[i].code,
                 k_aircraft_types[i].full_name,
                 (i < AIRCRAFT_TYPE_COUNT - 1) ? "\n" : "");
        strncat(ac_buf, item, sizeof(ac_buf) - strlen(ac_buf) - 1);
    }
    lv_dropdown_set_options(s_dd_aircraft, ac_buf);

    (void)create_section_header(s_form, "5. Zero Fuel Weight");

    lv_obj_t *zfw_card = lv_obj_create(s_form);
    lv_obj_add_style(zfw_card, ui_style_card(), 0);
    lv_obj_set_size(zfw_card, 444, 52);
    lv_obj_set_style_pad_all(zfw_card, 6, 0);
    lv_obj_set_style_pad_column(zfw_card, 12, 0);
    lv_obj_set_flex_flow(zfw_card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(zfw_card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(zfw_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *btn_zfw_minus = lv_btn_create(zfw_card);
    lv_obj_add_style(btn_zfw_minus, ui_style_btn_outline(), 0);
    lv_obj_set_size(btn_zfw_minus, 44, 32);
    lv_obj_add_event_cb(btn_zfw_minus, zfw_step_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)-500);
    lv_obj_t *l_zm = lv_label_create(btn_zfw_minus);
    lv_label_set_text(l_zm, "-");
    lv_obj_center(l_zm);

    s_ta_zfw = lv_textarea_create(zfw_card);
    lv_obj_set_size(s_ta_zfw, 130, 36);
    lv_textarea_set_one_line(s_ta_zfw, true);
    lv_textarea_set_accepted_chars(s_ta_zfw, "0123456789");
    lv_textarea_set_max_length(s_ta_zfw, 7);
    lv_obj_set_style_bg_color(s_ta_zfw, lv_color_hex(UI_COLOR_HEX_SURFACE), 0);
    lv_obj_set_style_border_color(s_ta_zfw, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
    lv_obj_set_style_border_width(s_ta_zfw, 1, 0);
    lv_obj_set_style_radius(s_ta_zfw, 8, 0);
    lv_obj_set_style_text_color(s_ta_zfw, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(s_ta_zfw, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(s_ta_zfw, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_event_cb(s_ta_zfw, textarea_click_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *lbl_kg = lv_label_create(zfw_card);
    lv_label_set_text(lbl_kg, "kg");
    lv_obj_set_style_text_font(lbl_kg, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_kg, lv_color_hex(UI_COLOR_HEX_TEXT_SECONDARY), 0);

    lv_obj_t *btn_zfw_plus = lv_btn_create(zfw_card);
    lv_obj_add_style(btn_zfw_plus, ui_style_btn_outline(), 0);
    lv_obj_set_size(btn_zfw_plus, 44, 32);
    lv_obj_add_event_cb(btn_zfw_plus, zfw_step_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)+500);
    lv_obj_t *l_zp = lv_label_create(btn_zfw_plus);
    lv_label_set_text(l_zp, "+");
    lv_obj_center(l_zp);

    (void)create_section_header(s_form, "6. Planned Fuel");

    lv_obj_t *fuel_card = lv_obj_create(s_form);
    lv_obj_add_style(fuel_card, ui_style_card(), 0);
    lv_obj_set_size(fuel_card, 444, 52);
    lv_obj_set_style_pad_all(fuel_card, 6, 0);
    lv_obj_set_style_pad_column(fuel_card, 12, 0);
    lv_obj_set_flex_flow(fuel_card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(fuel_card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(fuel_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *btn_fuel_minus = lv_btn_create(fuel_card);
    lv_obj_add_style(btn_fuel_minus, ui_style_btn_outline(), 0);
    lv_obj_set_size(btn_fuel_minus, 44, 32);
    lv_obj_add_event_cb(btn_fuel_minus, fuel_step_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)-100);
    lv_obj_t *l_fm = lv_label_create(btn_fuel_minus);
    lv_label_set_text(l_fm, "-");
    lv_obj_center(l_fm);

    s_ta_fuel = lv_textarea_create(fuel_card);
    lv_obj_set_size(s_ta_fuel, 130, 36);
    lv_textarea_set_one_line(s_ta_fuel, true);
    lv_textarea_set_accepted_chars(s_ta_fuel, "0123456789");
    lv_textarea_set_max_length(s_ta_fuel, 7);
    lv_obj_set_style_bg_color(s_ta_fuel, lv_color_hex(UI_COLOR_HEX_SURFACE), 0);
    lv_obj_set_style_border_color(s_ta_fuel, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
    lv_obj_set_style_border_width(s_ta_fuel, 1, 0);
    lv_obj_set_style_radius(s_ta_fuel, 8, 0);
    lv_obj_set_style_text_color(s_ta_fuel, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(s_ta_fuel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(s_ta_fuel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_event_cb(s_ta_fuel, textarea_click_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *lbl_liters = lv_label_create(fuel_card);
    lv_label_set_text(lbl_liters, "l");
    lv_obj_set_style_text_font(lbl_liters, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_liters, lv_color_hex(UI_COLOR_HEX_TEXT_SECONDARY), 0);

    lv_obj_t *btn_fuel_plus = lv_btn_create(fuel_card);
    lv_obj_add_style(btn_fuel_plus, ui_style_btn_outline(), 0);
    lv_obj_set_size(btn_fuel_plus, 44, 32);
    lv_obj_add_event_cb(btn_fuel_plus, fuel_step_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)+100);
    lv_obj_t *l_fp = lv_label_create(btn_fuel_plus);
    lv_label_set_text(l_fp, "+");
    lv_obj_center(l_fp);

    lv_obj_t *footer_row = lv_obj_create(s_form);
    lv_obj_set_size(footer_row, 444, 48);
    lv_obj_set_style_bg_opa(footer_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer_row, 0, 0);
    lv_obj_set_style_pad_all(footer_row, 0, 0);
    lv_obj_clear_flag(footer_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *next_btn = lv_btn_create(footer_row);
    lv_obj_add_style(next_btn, ui_style_btn_primary(), 0);
    lv_obj_set_size(next_btn, 120, 38);
    lv_obj_align(next_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(next_btn, next_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *next_lbl = lv_label_create(next_btn);
    lv_label_set_text(next_lbl, "Next  " LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(next_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(next_lbl);

    s_num_kb = lv_keyboard_create(scr);
    lv_obj_set_size(s_num_kb, 480, 114);
    lv_obj_align(s_num_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_map(s_num_kb, LV_KEYBOARD_MODE_USER_1, (const char **)s_num_kb_map, s_num_kb_ctrl);
    lv_keyboard_set_mode(s_num_kb, LV_KEYBOARD_MODE_USER_1);
    lv_obj_set_style_bg_color(s_num_kb, lv_color_hex(UI_COLOR_HEX_SURFACE_CONTAINER), 0);
    lv_obj_set_style_border_width(s_num_kb, 1, 0);
    lv_obj_set_style_border_color(s_num_kb, lv_color_hex(UI_COLOR_HEX_OUTLINE), 0);
    lv_obj_set_style_bg_color(s_num_kb, lv_color_hex(UI_COLOR_HEX_SURFACE_HIGH), LV_PART_ITEMS);
    lv_obj_set_style_text_color(s_num_kb, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(s_num_kb, lv_color_hex(UI_COLOR_HEX_PRIMARY), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(s_num_kb, lv_color_hex(UI_COLOR_HEX_ON_PRIMARY), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_add_flag(s_num_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_num_kb, num_kb_event_cb, LV_EVENT_ALL, NULL);

    sync_textareas_from_values();

    return scr;
}
