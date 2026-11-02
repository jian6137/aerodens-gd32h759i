/*!
    \file    src/os/ui/screen_home.c
    \brief   Screen 1: AeroDENS Saved Flights List & Floating Action Button
*/

#include "screen_home.h"
#include "ui_manager.h"
#include "ui_theme.h"
#include "ui_topbar.h"
#include "flight_store.h"
#include "configs/allowed_flights.h"
#include <stdio.h>

static void fab_click_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ui_manager_navigate_to(SCREEN_NEW_FLIGHT, LV_SCR_LOAD_ANIM_MOVE_LEFT);
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
            ui_manager_navigate_to(SCREEN_CONNECT, LV_SCR_LOAD_ANIM_MOVE_LEFT);
        }
    }
}

lv_obj_t *screen_home_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(UI_COLOR_HEX_SURFACE), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    (void)ui_topbar_create(scr, "AeroDENS", false, NULL, NULL);

    lv_obj_t *list_cont = lv_obj_create(scr);
    lv_obj_set_size(list_cont, 480, 234);
    lv_obj_set_pos(list_cont, 0, 38);
    lv_obj_set_style_bg_opa(list_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list_cont, 0, 0);
    lv_obj_set_style_pad_hor(list_cont, 16, 0);
    lv_obj_set_style_pad_ver(list_cont, 12, 0);
    lv_obj_set_flex_flow(list_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    uint32_t count = flight_store_get_count();

    if (count == 0U) {
        lv_obj_t *empty_card = lv_obj_create(list_cont);
        lv_obj_add_style(empty_card, ui_style_card(), 0);
        lv_obj_set_size(empty_card, 440, 160);
        lv_obj_set_flex_flow(empty_card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(empty_card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *empty_icon = lv_label_create(empty_card);
        lv_label_set_text(empty_icon, LV_SYMBOL_PLAY);
        lv_obj_set_style_text_font(empty_icon, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(empty_icon, lv_color_hex(UI_COLOR_HEX_TEXT_MUTED), 0);

        lv_obj_t *empty_title = lv_label_create(empty_card);
        lv_label_set_text(empty_title, "No Scheduled Flights");
        lv_obj_set_style_text_font(empty_title, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(empty_title, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);

        lv_obj_t *empty_btn = lv_btn_create(empty_card);
        lv_obj_add_style(empty_btn, ui_style_btn_primary(), 0);
        lv_obj_add_event_cb(empty_btn, fab_click_event_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *btn_lbl = lv_label_create(empty_btn);
        lv_label_set_text(btn_lbl, "+ Create New Flight");
    } else {
        for (uint32_t i = 0U; i < count; ++i) {
            const flight_record_t *f = flight_store_get(i);
            if (f == NULL) continue;

            lv_obj_t *card = lv_obj_create(list_cont);
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

            char route_text[32];
            snprintf(route_text, sizeof(route_text), "%s -> %s", f->origin, f->dest);
            lv_obj_t *route_lbl = lv_label_create(card);
            lv_label_set_text(route_lbl, route_text);
            lv_obj_set_style_text_font(route_lbl, &lv_font_montserrat_16, 0);
            lv_obj_set_style_text_color(route_lbl, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
            lv_obj_set_pos(route_lbl, 48, 2);
            lv_obj_clear_flag(route_lbl, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(route_lbl, LV_OBJ_FLAG_EVENT_BUBBLE);

            char detail_text[96];
            const char *air_name = (airline != NULL) ? airline->name : "Aviation";
            snprintf(detail_text, sizeof(detail_text), "%s | %s | %s", f->flight_no, air_name, f->aircraft_type);
            lv_obj_t *detail_lbl = lv_label_create(card);
            lv_label_set_text(detail_lbl, detail_text);
            lv_obj_set_style_text_font(detail_lbl, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(detail_lbl, lv_color_hex(UI_COLOR_HEX_TEXT_SECONDARY), 0);
            lv_obj_set_pos(detail_lbl, 48, 24);
            lv_obj_clear_flag(detail_lbl, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(detail_lbl, LV_OBJ_FLAG_EVENT_BUBBLE);

            char time_text[48];
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

    lv_obj_t *fab = lv_btn_create(scr);
    lv_obj_add_style(fab, ui_style_fab(), 0);
    lv_obj_set_size(fab, 52, 52);
    lv_obj_set_pos(fab, 412, 204);
    lv_obj_add_event_cb(fab, fab_click_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *fab_lbl = lv_label_create(fab);
    lv_label_set_text(fab_lbl, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_font(fab_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(fab_lbl);

    return scr;
}

