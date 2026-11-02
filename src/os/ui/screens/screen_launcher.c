/*!
    \file    src/os/ui/screens/screen_launcher.c
    \brief   AeroDENS Launcher Homescreen Implementation
*/

#include "screen_launcher.h"
#include "ui_manager.h"
#include "ui_theme.h"
#include "img_wallpaper.h"
#include "app_version.h"
#include "rtc_manager.h"
#include <stdio.h>

static lv_obj_t *s_clock_label = NULL;

static void launcher_clock_del_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    if (obj == s_clock_label) {
        s_clock_label = NULL;
    }
}

static void start_card_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_manager_navigate_to(SCREEN_HOME, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }
}

lv_obj_t *screen_launcher_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *wp = lv_img_create(scr);
    lv_img_set_src(wp, &img_wallpaper);
    lv_obj_set_pos(wp, 0, 0);
    lv_obj_set_size(wp, 480, 272);

    char init_time[48];
    rtc_manager_get_time_str(init_time, sizeof(init_time));

    s_clock_label = lv_label_create(scr);
    lv_label_set_text(s_clock_label, init_time);
    lv_obj_set_style_text_color(s_clock_label, lv_color_hex(UI_COLOR_HEX_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(s_clock_label, &lv_font_montserrat_12, 0);
    lv_obj_align(s_clock_label, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_add_event_cb(s_clock_label, launcher_clock_del_cb, LV_EVENT_DELETE, NULL);

    lv_obj_t *col_cont = lv_obj_create(scr);
    lv_obj_set_size(col_cont, 220, 110);
    lv_obj_align(col_cont, LV_ALIGN_TOP_MID, 0, 126);
    lv_obj_set_style_bg_opa(col_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col_cont, 0, 0);
    lv_obj_set_style_pad_all(col_cont, 0, 0);
    lv_obj_set_flex_flow(col_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col_cont, 10, 0);
    lv_obj_clear_flag(col_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *card_start = lv_obj_create(col_cont);
    lv_obj_set_size(card_start, 200, 44);
    lv_obj_set_style_bg_color(card_start, lv_color_hex(UI_COLOR_HEX_SURFACE_CONTAINER), 0);
    lv_obj_set_style_bg_color(card_start, lv_color_hex(UI_COLOR_HEX_PRIMARY_CONTAINER), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(card_start, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card_start, 12, 0);
    lv_obj_set_style_border_width(card_start, 1, 0);
    lv_obj_set_style_border_color(card_start, lv_color_hex(UI_COLOR_HEX_OUTLINE), 0);
    lv_obj_set_style_pad_all(card_start, 0, 0);
    lv_obj_clear_flag(card_start, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card_start, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card_start, start_card_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_start = lv_label_create(card_start);
    lv_label_set_text(lbl_start, LV_SYMBOL_PLAY "  Start");
    lv_obj_set_style_text_font(lbl_start, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_start, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
    lv_obj_center(lbl_start);

    lv_obj_t *card_settings = lv_obj_create(col_cont);
    lv_obj_set_size(card_settings, 200, 44);
    lv_obj_set_style_bg_color(card_settings, lv_color_hex(UI_COLOR_HEX_SURFACE_CONTAINER), 0);
    lv_obj_set_style_bg_opa(card_settings, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card_settings, 12, 0);
    lv_obj_set_style_border_width(card_settings, 1, 0);
    lv_obj_set_style_border_color(card_settings, lv_color_hex(UI_COLOR_HEX_OUTLINE), 0);
    lv_obj_set_style_pad_all(card_settings, 0, 0);
    lv_obj_clear_flag(card_settings, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(card_settings, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_state(card_settings, LV_STATE_DISABLED);
    lv_obj_set_style_opa(card_settings, LV_OPA_60, 0);

    lv_obj_t *lbl_settings = lv_label_create(card_settings);
    lv_label_set_text(lbl_settings, LV_SYMBOL_SETTINGS "  Settings");
    lv_obj_set_style_text_font(lbl_settings, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_settings, lv_color_hex(UI_COLOR_HEX_TEXT_MUTED), 0);
    lv_obj_center(lbl_settings);

    lv_obj_t *ver_label = lv_label_create(scr);
    lv_label_set_text(ver_label, AERODENS_VERSION_STRING);
    lv_obj_set_style_text_font(ver_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ver_label, lv_color_hex(UI_COLOR_HEX_TEXT_MUTED), 0);
    lv_obj_align(ver_label, LV_ALIGN_BOTTOM_RIGHT, -10, -6);

    return scr;
}

void screen_launcher_set_clock(const char *time_str)
{
    if (s_clock_label != NULL && lv_obj_is_valid(s_clock_label) && time_str != NULL) {
        lv_label_set_text(s_clock_label, time_str);
        lv_obj_align(s_clock_label, LV_ALIGN_TOP_MID, 0, 24);
    }
}

