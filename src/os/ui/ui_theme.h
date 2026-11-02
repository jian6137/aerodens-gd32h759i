/*!
    \file    src/os/ui/ui_theme.h
    \brief   AeroDENS Material 3 Theme Palette & Shared Widget Styles
             Seed Color: #7D001F | Target: GD32H759I-EVAL (480x272)
*/

#ifndef UI_THEME_H
#define UI_THEME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

#define UI_COLOR_HEX_SURFACE            0x1C1B1EUL
#define UI_COLOR_HEX_SURFACE_CONTAINER  0x2B292CUL
#define UI_COLOR_HEX_SURFACE_HIGH       0x363438UL
#define UI_COLOR_HEX_PRIMARY            0xFFB4C8UL
#define UI_COLOR_HEX_ON_PRIMARY         0x560013UL
#define UI_COLOR_HEX_PRIMARY_CONTAINER  0x920025UL
#define UI_COLOR_HEX_ON_PRIMARY_CONT    0xFFDADCUL
#define UI_COLOR_HEX_WARNING            0xFFB400UL
#define UI_COLOR_HEX_ON_WARNING         0x402D00UL
#define UI_COLOR_HEX_WARNING_CONTAINER  0x5C4300UL
#define UI_COLOR_HEX_ON_WARNING_CONT    0xFFDF99UL
#define UI_COLOR_HEX_OUTLINE            0x49454FUL
#define UI_COLOR_HEX_TEXT_PRIMARY       0xE6E1E5UL
#define UI_COLOR_HEX_TEXT_SECONDARY     0xCAC4D0UL
#define UI_COLOR_HEX_TEXT_MUTED         0x938F99UL

void ui_theme_init(void);

lv_style_t *ui_style_card(void);
lv_style_t *ui_style_card_alert(void);
lv_style_t *ui_style_btn_primary(void);
lv_style_t *ui_style_btn_tonal(void);
lv_style_t *ui_style_btn_outline(void);
lv_style_t *ui_style_fab(void);
lv_style_t *ui_style_bottom_sheet(void);
lv_style_t *ui_style_chip(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_THEME_H */

