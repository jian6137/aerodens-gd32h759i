/*!
    \file    src/os/ui/ui_theme.c
    \brief   AeroDENS Definitions & Initialization
*/

#include "ui_theme.h"

static lv_style_t s_style_card;
static lv_style_t s_style_card_alert;
static lv_style_t s_style_btn_primary;
static lv_style_t s_style_btn_tonal;
static lv_style_t s_style_btn_outline;
static lv_style_t s_style_fab;
static lv_style_t s_style_bottom_sheet;
static lv_style_t s_style_chip;
static bool s_theme_initialized = false;

void ui_theme_init(void)
{
    if (s_theme_initialized) {
        return;
    }

    lv_style_init(&s_style_card);
    lv_style_set_bg_color(&s_style_card, lv_color_hex(UI_COLOR_HEX_SURFACE_CONTAINER));
    lv_style_set_bg_opa(&s_style_card, LV_OPA_COVER);
    lv_style_set_radius(&s_style_card, 10);
    lv_style_set_border_width(&s_style_card, 1);
    lv_style_set_border_color(&s_style_card, lv_color_hex(UI_COLOR_HEX_OUTLINE));
    lv_style_set_pad_all(&s_style_card, 10);
    lv_style_set_text_color(&s_style_card, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY));

    lv_style_init(&s_style_card_alert);
    lv_style_set_bg_color(&s_style_card_alert, lv_color_hex(UI_COLOR_HEX_WARNING_CONTAINER));
    lv_style_set_bg_opa(&s_style_card_alert, LV_OPA_COVER);
    lv_style_set_radius(&s_style_card_alert, 8);
    lv_style_set_border_width(&s_style_card_alert, 1);
    lv_style_set_border_color(&s_style_card_alert, lv_color_hex(UI_COLOR_HEX_WARNING));
    lv_style_set_pad_all(&s_style_card_alert, 8);
    lv_style_set_text_color(&s_style_card_alert, lv_color_hex(UI_COLOR_HEX_ON_WARNING_CONT));

    lv_style_init(&s_style_btn_primary);
    lv_style_set_bg_color(&s_style_btn_primary, lv_color_hex(UI_COLOR_HEX_PRIMARY));
    lv_style_set_bg_opa(&s_style_btn_primary, LV_OPA_COVER);
    lv_style_set_radius(&s_style_btn_primary, LV_RADIUS_CIRCLE);
    lv_style_set_border_width(&s_style_btn_primary, 0);
    lv_style_set_text_color(&s_style_btn_primary, lv_color_hex(UI_COLOR_HEX_ON_PRIMARY));
    lv_style_set_pad_ver(&s_style_btn_primary, 8);
    lv_style_set_pad_hor(&s_style_btn_primary, 18);
    lv_style_set_shadow_width(&s_style_btn_primary, 0);

    lv_style_init(&s_style_btn_tonal);
    lv_style_set_bg_color(&s_style_btn_tonal, lv_color_hex(UI_COLOR_HEX_PRIMARY_CONTAINER));
    lv_style_set_bg_opa(&s_style_btn_tonal, LV_OPA_COVER);
    lv_style_set_radius(&s_style_btn_tonal, LV_RADIUS_CIRCLE);
    lv_style_set_border_width(&s_style_btn_tonal, 0);
    lv_style_set_text_color(&s_style_btn_tonal, lv_color_hex(UI_COLOR_HEX_ON_PRIMARY_CONT));
    lv_style_set_pad_ver(&s_style_btn_tonal, 8);
    lv_style_set_pad_hor(&s_style_btn_tonal, 16);
    lv_style_set_shadow_width(&s_style_btn_tonal, 0);

    lv_style_init(&s_style_btn_outline);
    lv_style_set_bg_color(&s_style_btn_outline, lv_color_hex(UI_COLOR_HEX_SURFACE_CONTAINER));
    lv_style_set_bg_opa(&s_style_btn_outline, LV_OPA_TRANSP);
    lv_style_set_radius(&s_style_btn_outline, LV_RADIUS_CIRCLE);
    lv_style_set_border_width(&s_style_btn_outline, 1);
    lv_style_set_border_color(&s_style_btn_outline, lv_color_hex(UI_COLOR_HEX_PRIMARY));
    lv_style_set_text_color(&s_style_btn_outline, lv_color_hex(UI_COLOR_HEX_PRIMARY));
    lv_style_set_pad_ver(&s_style_btn_outline, 8);
    lv_style_set_pad_hor(&s_style_btn_outline, 16);
    lv_style_set_shadow_width(&s_style_btn_outline, 0);

    lv_style_init(&s_style_fab);
    lv_style_set_bg_color(&s_style_fab, lv_color_hex(UI_COLOR_HEX_PRIMARY));
    lv_style_set_bg_opa(&s_style_fab, LV_OPA_COVER);
    lv_style_set_radius(&s_style_fab, 16);
    lv_style_set_text_color(&s_style_fab, lv_color_hex(UI_COLOR_HEX_ON_PRIMARY));
    lv_style_set_shadow_width(&s_style_fab, 8);
    lv_style_set_shadow_color(&s_style_fab, lv_color_black());
    lv_style_set_shadow_opa(&s_style_fab, LV_OPA_40);
    lv_style_set_shadow_ofs_y(&s_style_fab, 4);

    lv_style_init(&s_style_bottom_sheet);
    lv_style_set_bg_color(&s_style_bottom_sheet, lv_color_hex(UI_COLOR_HEX_SURFACE_CONTAINER));
    lv_style_set_bg_opa(&s_style_bottom_sheet, LV_OPA_COVER);
    lv_style_set_radius(&s_style_bottom_sheet, 20);
    lv_style_set_border_width(&s_style_bottom_sheet, 1);
    lv_style_set_border_color(&s_style_bottom_sheet, lv_color_hex(UI_COLOR_HEX_OUTLINE));
    lv_style_set_border_side(&s_style_bottom_sheet, LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT);
    lv_style_set_shadow_width(&s_style_bottom_sheet, 16);
    lv_style_set_shadow_color(&s_style_bottom_sheet, lv_color_black());
    lv_style_set_shadow_opa(&s_style_bottom_sheet, LV_OPA_50);
    lv_style_set_shadow_ofs_y(&s_style_bottom_sheet, -4);

    lv_style_init(&s_style_chip);
    lv_style_set_bg_color(&s_style_chip, lv_color_hex(UI_COLOR_HEX_PRIMARY_CONTAINER));
    lv_style_set_bg_opa(&s_style_chip, LV_OPA_COVER);
    lv_style_set_border_width(&s_style_chip, 0);
    lv_style_set_radius(&s_style_chip, 8);
    lv_style_set_pad_hor(&s_style_chip, 8);
    lv_style_set_pad_ver(&s_style_chip, 3);
    lv_style_set_text_color(&s_style_chip, lv_color_hex(UI_COLOR_HEX_ON_PRIMARY_CONT));

    s_theme_initialized = true;
}

lv_style_t *ui_style_card(void)          { return &s_style_card; }
lv_style_t *ui_style_card_alert(void)    { return &s_style_card_alert; }
lv_style_t *ui_style_btn_primary(void)   { return &s_style_btn_primary; }
lv_style_t *ui_style_btn_tonal(void)     { return &s_style_btn_tonal; }
lv_style_t *ui_style_btn_outline(void)   { return &s_style_btn_outline; }
lv_style_t *ui_style_fab(void)           { return &s_style_fab; }
lv_style_t *ui_style_bottom_sheet(void)  { return &s_style_bottom_sheet; }
lv_style_t *ui_style_chip(void)          { return &s_style_chip; }

