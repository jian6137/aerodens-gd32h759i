/*!
    \file    src/os/ui/screens/screen_sd_removed.c
    \brief   Global SD Card Hot-Removal Warning Screen Implementation
*/

#include "screen_sd_removed.h"
#include "ui_manager.h"
#include "ui_theme.h"
#include "ui_topbar.h"
#include "img_ic_sdcard.h"

static void btn_ack_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_manager_navigate_to(SCREEN_HOME, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    }
}

lv_obj_t *screen_sd_removed_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(UI_COLOR_HEX_SURFACE), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    (void)ui_topbar_create(scr, "Storage Alert", false, NULL, "SD ERR");

    lv_obj_t *card = lv_obj_create(scr);
    lv_obj_add_style(card, ui_style_card(), 0);
    lv_obj_set_size(card, 440, 210);
    lv_obj_set_pos(card, 20, 48);
    lv_obj_set_style_border_color(card, lv_color_hex(UI_COLOR_HEX_WARNING), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_set_style_pad_row(card, 8, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon = lv_img_create(card);
    lv_img_set_src(icon, &img_ic_sdcard);

    lv_obj_t *heading = lv_label_create(card);
    lv_label_set_text(heading, "SD Card Removed");
    lv_obj_set_style_text_font(heading, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(heading, lv_color_hex(UI_COLOR_HEX_WARNING), 0);
    lv_obj_set_style_text_align(heading, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *subtext = lv_label_create(card);
    lv_obj_set_width(subtext, 396);
    lv_label_set_long_mode(subtext, LV_LABEL_LONG_WRAP);
    lv_label_set_text(
        subtext,
        "The last recorded flight data may be corrupted. "
        "Please proceed with caution when resuming and verify all flight records first."
    );
    lv_obj_set_style_text_font(subtext, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(subtext, lv_color_hex(UI_COLOR_HEX_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_align(subtext, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *btn_ok = lv_btn_create(card);
    lv_obj_add_style(btn_ok, ui_style_btn_primary(), 0);
    lv_obj_set_size(btn_ok, 176, 34);
    lv_obj_add_event_cb(btn_ok, btn_ack_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_ok = lv_label_create(btn_ok);
    lv_label_set_text(lbl_ok, "Return to Flights");
    lv_obj_set_style_text_font(lbl_ok, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_ok);

    return scr;
}

