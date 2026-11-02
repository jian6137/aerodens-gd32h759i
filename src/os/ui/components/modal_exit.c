/*!
    \file    src/os/ui/modal_exit.c
    \brief   Exit & Save Telemetry Confirmation Modal Dialog Implementation
*/

#include "modal_exit.h"
#include "ui_manager.h"
#include "ui_theme.h"
#include "flight_store.h"

static lv_obj_t *s_modal_overlay = NULL;

static void btn_save_exit_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        flight_record_t *curr = ui_manager_get_active_flight();
        if (curr != NULL) {
            curr->is_completed = true;
            flight_store_add(curr);
        }
        modal_exit_hide();
        ui_manager_navigate_to(SCREEN_HOME, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    }
}

static void btn_exit_no_save_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        modal_exit_hide();
        ui_manager_navigate_to(SCREEN_HOME, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    }
}

static void btn_cancel_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        modal_exit_hide();
    }
}

void modal_exit_show(lv_obj_t *parent_scr)
{
    if (s_modal_overlay != NULL) {
        return;
    }

    s_modal_overlay = lv_obj_create(parent_scr);
    lv_obj_set_size(s_modal_overlay, 480, 272);
    lv_obj_set_pos(s_modal_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_modal_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_modal_overlay, LV_OPA_60, 0);
    lv_obj_set_style_border_width(s_modal_overlay, 0, 0);
    lv_obj_set_style_radius(s_modal_overlay, 0, 0);
    lv_obj_clear_flag(s_modal_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *dialog = lv_obj_create(s_modal_overlay);
    lv_obj_add_style(dialog, ui_style_card(), 0);
    lv_obj_set_size(dialog, 320, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(dialog, 20, 0);
    lv_obj_set_style_pad_top(dialog, 16, 0);
    lv_obj_set_style_pad_bottom(dialog, 16, 0);
    lv_obj_set_style_pad_left(dialog, 24, 0);
    lv_obj_set_style_pad_right(dialog, 24, 0);
    lv_obj_set_style_pad_row(dialog, 8, 0);
    lv_obj_center(dialog);
    lv_obj_set_flex_flow(dialog, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(dialog, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(dialog);
    lv_label_set_text(title, "Are you sure to finish?");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *desc = lv_label_create(dialog);
    lv_label_set_text(desc, "This will halt telemetry and stop all\ncurrent flight calculations.");
    lv_obj_set_style_text_font(desc, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(desc, lv_color_hex(UI_COLOR_HEX_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_align(desc, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_bottom(desc, 4, 0);

    lv_obj_t *btn_save = lv_btn_create(dialog);
    lv_obj_add_style(btn_save, ui_style_btn_primary(), 0);
    lv_obj_set_size(btn_save, 272, 32);
    lv_obj_add_event_cb(btn_save, btn_save_exit_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "Save & Exit");
    lv_obj_set_style_text_font(lbl_save, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_save);

    lv_obj_t *btn_no_save = lv_btn_create(dialog);
    lv_obj_add_style(btn_no_save, ui_style_btn_outline(), 0);
    lv_obj_set_size(btn_no_save, 272, 32);
    lv_obj_add_event_cb(btn_no_save, btn_exit_no_save_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_no_save = lv_label_create(btn_no_save);
    lv_label_set_text(lbl_no_save, "Exit Without Saving");
    lv_obj_set_style_text_font(lbl_no_save, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_no_save);

    lv_obj_t *btn_cancel = lv_btn_create(dialog);
    lv_obj_add_style(btn_cancel, ui_style_btn_outline(), 0);
    lv_obj_set_size(btn_cancel, 272, 32);
    lv_obj_add_event_cb(btn_cancel, btn_cancel_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, "Cancel");
    lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_cancel);
}

void modal_exit_hide(void)
{
    if (s_modal_overlay != NULL) {
        lv_obj_del_async(s_modal_overlay);
        s_modal_overlay = NULL;
    }
}

