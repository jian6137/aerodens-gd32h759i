/*!
    \file    src/os/ui/screen_connect.c
    \brief   Screen 3: Hardware Setup & ARINC 429 Telemetry Connection Implementation
*/

#include "screen_connect.h"
#include "ui_manager.h"
#include "ui_theme.h"
#include "ui_topbar.h"

static void topbar_back_cb(void)
{
    ui_manager_navigate_to(SCREEN_CONFIRM_FLIGHT, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
}

static void start_telemetry_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ui_manager_navigate_to(SCREEN_DASHBOARD, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }
}

lv_obj_t *screen_connect_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(UI_COLOR_HEX_SURFACE), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    (void)ui_topbar_create(scr, "Hardware Setup", true, topbar_back_cb, NULL);

    lv_obj_t *cont = lv_obj_create(scr);
    lv_obj_set_size(cont, 440, 220);
    lv_obj_set_pos(cont, 20, 44);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon = lv_label_create(cont);
    lv_label_set_text(icon, LV_SYMBOL_USB " " LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
    lv_obj_set_style_pad_bottom(icon, 8, 0);

    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "Connect AeroDENS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
    lv_obj_set_style_pad_bottom(title, 6, 0);

    lv_obj_t *desc = lv_label_create(cont);
    lv_label_set_text(desc, "Connect AeroDENS to Aircraft via ARINC 429 / USB\nto begin live telemetry stream.");
    lv_obj_set_style_text_font(desc, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(desc, lv_color_hex(UI_COLOR_HEX_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_align(desc, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_bottom(desc, 18, 0);

    lv_obj_t *btn = lv_btn_create(cont);
    lv_obj_add_style(btn, ui_style_btn_tonal(), 0);
    lv_obj_set_size(btn, 220, 44);
    lv_obj_add_event_cb(btn, start_telemetry_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, LV_SYMBOL_WIFI "  Start Telemetry");
    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(btn_lbl);

    return scr;
}

