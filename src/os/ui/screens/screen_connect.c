/*!
    \file    src/os/ui/screens/screen_connect.c
    \brief   Screen 3: Hardware Setup & Live Telemetry Mode Selection
*/

#include "screen_connect.h"
#include "ui_manager.h"
#include "ui_theme.h"
#include "ui_topbar.h"
#include "img_ic_connect.h"

static void topbar_back_cb(void)
{
    ui_manager_navigate_to(SCREEN_CONFIRM_FLIGHT, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
}

static void usb_mode_event_cb(lv_event_t *e)
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

    lv_obj_t *main_cont = lv_obj_create(scr);
    lv_obj_set_size(main_cont, 480, 234);
    lv_obj_set_pos(main_cont, 0, 38);
    lv_obj_set_style_bg_opa(main_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(main_cont, 0, 0);
    lv_obj_set_style_pad_all(main_cont, 0, 0);
    lv_obj_set_style_pad_column(main_cont, 0, 0);
    lv_obj_set_style_pad_row(main_cont, 0, 0);
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(main_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *left_cont = lv_obj_create(main_cont);
    lv_obj_set_size(left_cont, 240, 234);
    lv_obj_set_style_bg_opa(left_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left_cont, 0, 0);
    lv_obj_set_style_pad_all(left_cont, 16, 0);
    lv_obj_set_flex_flow(left_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(left_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(left_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon_img = lv_img_create(left_cont);
    lv_img_set_src(icon_img, &img_ic_connect);
    lv_obj_set_style_pad_bottom(icon_img, 14, 0);

    lv_obj_t *title_lbl = lv_label_create(left_cont);
    lv_label_set_text(title_lbl, "Select Live\nTelemetry Mode");
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_align(title_lbl, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *right_cont = lv_obj_create(main_cont);
    lv_obj_set_size(right_cont, 240, 234);
    lv_obj_set_style_bg_opa(right_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_cont, 0, 0);
    lv_obj_set_style_pad_all(right_cont, 16, 0);
    lv_obj_set_flex_flow(right_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(right_cont, 16, 0);
    lv_obj_clear_flag(right_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *btn_usb = lv_btn_create(right_cont);
    lv_obj_add_style(btn_usb, ui_style_btn_primary(), 0);
    lv_obj_set_size(btn_usb, 180, 46);
    lv_obj_set_style_radius(btn_usb, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_event_cb(btn_usb, usb_mode_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_usb = lv_label_create(btn_usb);
    lv_label_set_text(lbl_usb, "USB Mode");
    lv_obj_set_style_text_font(lbl_usb, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_usb);

    lv_obj_t *btn_arinc = lv_btn_create(right_cont);
    lv_obj_add_style(btn_arinc, ui_style_btn_tonal(), 0);
    lv_obj_set_size(btn_arinc, 180, 46);
    lv_obj_set_style_radius(btn_arinc, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(btn_arinc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_state(btn_arinc, LV_STATE_DISABLED);
    lv_obj_set_style_opa(btn_arinc, LV_OPA_50, 0);

    lv_obj_t *lbl_arinc = lv_label_create(btn_arinc);
    lv_label_set_text(lbl_arinc, "ARINC 429");
    lv_obj_set_style_text_font(lbl_arinc, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_arinc);

    return scr;
}
