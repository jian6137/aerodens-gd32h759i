/*!
    \file    src/os/ui/ui_topbar.c
    \brief   Top App Bar Implementation
*/

#include "ui_topbar.h"
#include "ui_theme.h"
#include "img_ic_os_white.h"
#include "rtc_manager.h"

static lv_obj_t *s_clock_label = NULL;
static lv_obj_t *s_badge_obj = NULL;
static ui_topbar_back_cb_t s_back_cb = NULL;

static void clock_label_del_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    if (obj == s_clock_label) {
        s_clock_label = NULL;
    }
}

static void badge_obj_del_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    if (obj == s_badge_obj) {
        s_badge_obj = NULL;
    }
}

static void back_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED && s_back_cb != NULL) {
        s_back_cb();
    }
}

lv_obj_t *ui_topbar_create(lv_obj_t *parent,
                           const char *title,
                           bool show_back_btn,
                           ui_topbar_back_cb_t back_cb,
                           const char *badge_text)
{
    s_back_cb = back_cb;

    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, 480, 38);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(UI_COLOR_HEX_SURFACE_CONTAINER), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(bar, lv_color_hex(UI_COLOR_HEX_OUTLINE), 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_coord_t left_x = 8;

    if (show_back_btn) {
        lv_obj_t *back_btn = lv_btn_create(bar);
        lv_obj_set_size(back_btn, 32, 32);
        lv_obj_set_pos(back_btn, 4, 3);
        lv_obj_set_style_bg_opa(back_btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_shadow_width(back_btn, 0, 0);
        lv_obj_set_style_pad_all(back_btn, 0, 0);
        lv_obj_add_event_cb(back_btn, back_btn_event_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *back_lbl = lv_label_create(back_btn);
        lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_color(back_lbl, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
        lv_obj_center(back_lbl);

        left_x = 40;
    } else {
        lv_obj_t *icon_img = lv_img_create(bar);
        lv_img_set_src(icon_img, &img_ic_os_white);
        lv_obj_set_pos(icon_img, 10, 8);
        left_x = 36;
    }

    lv_obj_t *title_lbl = lv_label_create(bar);
    lv_label_set_text(title_lbl, (title && title[0] != '\0') ? title : "AeroDENS");
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(title_lbl, left_x, 11);

    char init_time[48];
    rtc_manager_get_time_str(init_time, sizeof(init_time));
    s_clock_label = lv_label_create(bar);
    lv_label_set_text(s_clock_label, init_time);
    lv_obj_set_style_text_color(s_clock_label, lv_color_hex(UI_COLOR_HEX_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(s_clock_label, &lv_font_montserrat_12, 0);
    lv_obj_align(s_clock_label, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_obj_add_event_cb(s_clock_label, clock_label_del_cb, LV_EVENT_DELETE, NULL);

    s_badge_obj = NULL;
    if (badge_text != NULL) {
        lv_obj_t *badge = lv_obj_create(bar);
        lv_obj_add_style(badge, ui_style_chip(), 0);
        lv_obj_set_size(badge, LV_SIZE_CONTENT, 22);
        lv_obj_set_style_pad_ver(badge, 2, 0);
        lv_obj_set_style_pad_hor(badge, 8, 0);
        lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *badge_lbl = lv_label_create(badge);
        lv_label_set_text(badge_lbl, badge_text);
        lv_obj_set_style_text_font(badge_lbl, &lv_font_montserrat_12, 0);
        lv_obj_center(badge_lbl);

        lv_obj_align_to(badge, s_clock_label, LV_ALIGN_OUT_LEFT_MID, -10, 0);
        lv_obj_add_event_cb(badge, badge_obj_del_cb, LV_EVENT_DELETE, NULL);
        s_badge_obj = badge;
    }

    return bar;
}

void ui_topbar_set_clock(const char *time_str)
{
    if (s_clock_label != NULL && lv_obj_is_valid(s_clock_label) && time_str != NULL) {
        lv_label_set_text(s_clock_label, time_str);
        lv_obj_align(s_clock_label, LV_ALIGN_RIGHT_MID, -12, 0);
        if (s_badge_obj != NULL && lv_obj_is_valid(s_badge_obj)) {
            lv_obj_align_to(s_badge_obj, s_clock_label, LV_ALIGN_OUT_LEFT_MID, -10, 0);
        }
    }
}

