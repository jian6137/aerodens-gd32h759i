/*!
    \file    src/os/ui/screens/screen_home.c
    \brief   Screen 1: AeroDENS Saved Flights List & Floating Action Button
*/

#include "screen_home.h"
#include "ui_manager.h"
#include "ui_theme.h"
#include "ui_topbar.h"
#include "flight_store.h"
#include "configs/allowed_flights.h"
#include "img_ic_sdcard.h"
#include "img_ic_empty.h"
#include <stdio.h>

static lv_obj_t *s_scr_home = NULL;
static lv_obj_t *s_list_cont = NULL;
static lv_obj_t *s_fab = NULL;

static void home_delete_cb(lv_event_t *e)
{
    (void)e;
    s_scr_home = NULL;
    s_list_cont = NULL;
    s_fab = NULL;
}

static void fab_click_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        if (flight_store_is_sd_present()) {
            ui_manager_navigate_to(SCREEN_NEW_FLIGHT, LV_SCR_LOAD_ANIM_MOVE_LEFT);
        }
    }
}

static void flight_card_click_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        uint32_t idx = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
        const flight_record_t *flight = flight_store_get(idx);
        if (flight != NULL) {
            ui_manager_set_active_flight(flight);
            ui_manager_navigate_to(SCREEN_FLIGHT_INFO, LV_SCR_LOAD_ANIM_MOVE_LEFT);
        }
    }
}

void screen_home_refresh(void)
{
    if (s_scr_home == NULL || !lv_obj_is_valid(s_scr_home) ||
        s_list_cont == NULL || !lv_obj_is_valid(s_list_cont)) {
        return;
    }

    lv_obj_clean(s_list_cont);

    bool sd_present = flight_store_is_sd_present();

    if (!sd_present) {
        /* Hide + FAB when no SD Card is inserted */
        if (s_fab != NULL && lv_obj_is_valid(s_fab)) {
            lv_obj_add_flag(s_fab, LV_OBJ_FLAG_HIDDEN);
        }

        lv_obj_t *sd_card = lv_obj_create(s_list_cont);
        lv_obj_add_style(sd_card, ui_style_card(), 0);
        lv_obj_set_size(sd_card, 440, 176);
        lv_obj_set_style_pad_row(sd_card, 8, 0);
        lv_obj_set_flex_flow(sd_card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(sd_card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(sd_card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *sd_icon = lv_img_create(sd_card);
        lv_img_set_src(sd_icon, &img_ic_sdcard);

        lv_obj_t *sd_title = lv_label_create(sd_card);
        lv_label_set_text(sd_title, "Insert SD Card");
        lv_obj_set_style_text_font(sd_title, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(sd_title, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);

        lv_obj_t *sd_sub = lv_label_create(sd_card);
        lv_label_set_text(sd_sub, "Insert a microSD card to load or create flights");
        lv_obj_set_style_text_font(sd_sub, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(sd_sub, lv_color_hex(UI_COLOR_HEX_TEXT_MUTED), 0);
        return;
    }

    /* SD Card is present: show + FAB */
    if (s_fab != NULL && lv_obj_is_valid(s_fab)) {
        lv_obj_clear_flag(s_fab, LV_OBJ_FLAG_HIDDEN);
    }

    uint32_t count = flight_store_get_count();

    if (count == 0U) {
        lv_obj_t *empty_card = lv_obj_create(s_list_cont);
        lv_obj_add_style(empty_card, ui_style_card(), 0);
        lv_obj_set_size(empty_card, 440, 176);
        lv_obj_set_style_pad_row(empty_card, 8, 0);
        lv_obj_set_flex_flow(empty_card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(empty_card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(empty_card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *empty_icon = lv_img_create(empty_card);
        lv_img_set_src(empty_icon, &img_ic_empty);

        lv_obj_t *empty_title = lv_label_create(empty_card);
        lv_label_set_text(empty_title, "Nothing Here");
        lv_obj_set_style_text_font(empty_title, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(empty_title, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);

        lv_obj_t *empty_sub = lv_label_create(empty_card);
        lv_label_set_text(empty_sub, "Create a new flight now");
        lv_obj_set_style_text_font(empty_sub, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(empty_sub, lv_color_hex(UI_COLOR_HEX_TEXT_MUTED), 0);
    } else {
        for (uint32_t i = 0U; i < count; ++i) {
            const flight_record_t *f = flight_store_get(i);
            if (f == NULL) continue;

            lv_obj_t *card = lv_obj_create(s_list_cont);
            lv_obj_add_style(card, ui_style_card(), 0);
            lv_obj_set_size(card, 448, 64);
            lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(card, flight_card_click_event_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);

            const airline_info_t *airline = airline_get_info_by_flight_no(f->flight_no);

            lv_obj_t *icon_circle = lv_obj_create(card);
            lv_obj_set_size(icon_circle, 38, 38);
            lv_obj_set_pos(icon_circle, 2, 3);
            lv_obj_set_style_radius(icon_circle, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(icon_circle, lv_color_hex(UI_COLOR_HEX_SURFACE_HIGH), 0);
            lv_obj_set_style_border_color(icon_circle, lv_color_hex(UI_COLOR_HEX_OUTLINE), 0);
            lv_obj_set_style_border_width(icon_circle, 1, 0);
            lv_obj_set_style_pad_all(icon_circle, 0, 0);
            lv_obj_clear_flag(icon_circle, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_clear_flag(icon_circle, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(icon_circle, LV_OBJ_FLAG_EVENT_BUBBLE);

            if (airline != NULL && airline->icon != NULL) {
                lv_obj_t *icon_img = lv_img_create(icon_circle);
                lv_img_set_src(icon_img, airline->icon);
                lv_obj_center(icon_img);
                lv_obj_clear_flag(icon_img, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_add_flag(icon_img, LV_OBJ_FLAG_EVENT_BUBBLE);
            } else {
                lv_obj_t *code_lbl = lv_label_create(icon_circle);
                lv_label_set_text(code_lbl, (airline != NULL) ? airline->code : "FL");
                lv_obj_set_style_text_font(code_lbl, &lv_font_montserrat_12, 0);
                lv_obj_set_style_text_color(code_lbl, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
                lv_obj_center(code_lbl);
            }

            char route_text[48];
            snprintf(route_text, sizeof(route_text), "%s -> %s", f->origin, f->dest);
            lv_obj_t *route_lbl = lv_label_create(card);
            lv_label_set_text(route_lbl, route_text);
            lv_obj_set_style_text_font(route_lbl, &lv_font_montserrat_16, 0);
            lv_obj_set_style_text_color(route_lbl, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
            lv_obj_set_pos(route_lbl, 48, 2);
            lv_obj_clear_flag(route_lbl, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(route_lbl, LV_OBJ_FLAG_EVENT_BUBBLE);

            char detail_text[96];
            const char *air_name = (f->carrier[0] != '\0')
                                       ? f->carrier
                                       : ((airline != NULL) ? airline->name : "Aviation");
            snprintf(detail_text, sizeof(detail_text), "%s | %s | %s", f->flight_no, air_name, f->aircraft_type);
            lv_obj_t *detail_lbl = lv_label_create(card);
            lv_label_set_text(detail_lbl, detail_text);
            lv_obj_set_style_text_font(detail_lbl, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(detail_lbl, lv_color_hex(UI_COLOR_HEX_TEXT_SECONDARY), 0);
            lv_obj_set_pos(detail_lbl, 48, 24);
            lv_obj_clear_flag(detail_lbl, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(detail_lbl, LV_OBJ_FLAG_EVENT_BUBBLE);

            char time_text[80];
            snprintf(time_text, sizeof(time_text), "%s - %s", f->depart_time, f->arrive_time);
            lv_obj_t *time_lbl = lv_label_create(card);
            lv_label_set_text(time_lbl, time_text);
            lv_obj_set_style_text_font(time_lbl, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(time_lbl, lv_color_hex(UI_COLOR_HEX_TEXT_MUTED), 0);
            lv_obj_align(time_lbl, LV_ALIGN_RIGHT_MID, -28, 0);
            lv_obj_clear_flag(time_lbl, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(time_lbl, LV_OBJ_FLAG_EVENT_BUBBLE);

            lv_obj_t *arrow_lbl = lv_label_create(card);
            lv_label_set_text(arrow_lbl, LV_SYMBOL_RIGHT);
            lv_obj_set_style_text_color(arrow_lbl, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
            lv_obj_align(arrow_lbl, LV_ALIGN_RIGHT_MID, -6, 0);
            lv_obj_clear_flag(arrow_lbl, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(arrow_lbl, LV_OBJ_FLAG_EVENT_BUBBLE);
        }
    }
}

lv_obj_t *screen_home_create(void)
{
    s_scr_home = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr_home, lv_color_hex(UI_COLOR_HEX_SURFACE), 0);
    lv_obj_set_style_bg_opa(s_scr_home, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(s_scr_home, home_delete_cb, LV_EVENT_DELETE, NULL);

    (void)ui_topbar_create(s_scr_home, "AeroDENS", false, NULL, NULL);

    s_list_cont = lv_obj_create(s_scr_home);
    lv_obj_set_size(s_list_cont, 480, 234);
    lv_obj_set_pos(s_list_cont, 0, 38);
    lv_obj_set_style_bg_opa(s_list_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list_cont, 0, 0);
    lv_obj_set_style_pad_hor(s_list_cont, 16, 0);
    lv_obj_set_style_pad_ver(s_list_cont, 12, 0);
    lv_obj_set_flex_flow(s_list_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_list_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_fab = lv_btn_create(s_scr_home);
    lv_obj_add_style(s_fab, ui_style_fab(), 0);
    lv_obj_set_size(s_fab, 52, 52);
    lv_obj_set_pos(s_fab, 412, 204);
    lv_obj_add_event_cb(s_fab, fab_click_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *fab_lbl = lv_label_create(s_fab);
    lv_label_set_text(fab_lbl, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_font(fab_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(fab_lbl);

    if (flight_store_is_sd_present()) {
        flight_store_scan_flights();
    }
    screen_home_refresh();

    return s_scr_home;
}
