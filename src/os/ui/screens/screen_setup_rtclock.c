/*!
    \file    src/os/ui/screen_setup_rtclock.c
    \brief   AeroDENS RTC Clock & Calendar Setup Wizard
             Target: GD32H759I-EVAL (480x272)
*/

#include "screen_setup_rtclock.h"
#include "ui_manager.h"
#include "ui_theme.h"
#include "rtc_manager.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *label;
    const char *region;
    int16_t offset_min;
} tz_entry_t;

static const tz_entry_t s_tz_list[] = {
    {"UTC-10:00 (Honolulu / HST)", "Hawaii Standard Time", -600},
    {"UTC-08:00 (Los Angeles / PST)", "Pacific Standard Time", -480},
    {"UTC-05:00 (New York / EST)", "Eastern Standard Time", -300},
    {"UTC+00:00 (London / GMT)", "Greenwich Mean Time", 0},
    {"UTC+01:00 (Paris, Berlin / CET)", "Central European Time", 60},
    {"UTC+02:00 (Cairo, Athens / EET)", "Eastern European Time", 120},
    {"UTC+03:00 (Riyadh, Moscow / MSK)", "Moscow Standard Time", 180},
    {"UTC+04:00 (Dubai / GST)", "Gulf Standard Time", 240},
    {"UTC+05:30 (New Delhi / IST)", "Indian Standard Time", 330},
    {"UTC+07:00 (Bangkok, Jakarta / ICT)", "Indochina Time", 420},
    {"UTC+08:00 (Manila, Singapore / PHT)", "Philippine Standard Time", 480},
    {"UTC+09:00 (Tokyo, Seoul / JST)", "Japan Standard Time", 540},
    {"UTC+09:30 (Adelaide / ACST)", "Australian Central Time", 570},
    {"UTC+10:00 (Sydney, Brisbane / AEST)", "Australian Eastern Time", 600},
    {"UTC+12:00 (Auckland, Fiji / NZST)", "New Zealand Time", 720}
};

#define TZ_COUNT         ((uint16_t)(sizeof(s_tz_list) / sizeof(s_tz_list[0])))
#define TZ_DEFAULT_INDEX 10U

static uint8_t  s_step = 0U;

static uint16_t s_year = 2026U;
static uint8_t  s_month = 11U;
static uint8_t  s_day = 1U;

static uint8_t  s_hour_12 = 12U;
static uint8_t  s_minute = 0U;
static bool     s_is_pm = false;

static uint16_t s_tz_idx = TZ_DEFAULT_INDEX;

static lv_obj_t *s_root_scr = NULL;
static lv_obj_t *s_subtext_lbl = NULL;
static lv_obj_t *s_step_container = NULL;
static lv_obj_t *s_btn_next = NULL;
static lv_obj_t *s_btn_next_lbl = NULL;
static lv_obj_t *s_btn_back = NULL;

static lv_obj_t *s_date_val_lbl = NULL;
static lv_obj_t *s_date_dow_lbl = NULL;

static lv_obj_t *s_time_val_lbl = NULL;
static lv_obj_t *s_time_fmt_lbl = NULL;
static lv_obj_t *s_clock_hour_lbl = NULL;
static lv_obj_t *s_clock_min_lbl = NULL;
static lv_obj_t *s_btn_am = NULL;
static lv_obj_t *s_btn_pm = NULL;

static lv_obj_t *s_tz_preview_lbl = NULL;

static void render_current_step(void);
static void update_clock_display(void);
static void update_date_display(void);
static void update_tz_display(void);

static const char *get_weekday_str(uint16_t y, uint8_t m, uint8_t d)
{
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    static const char *days[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                                 "Thursday", "Friday", "Saturday"};
    if (m < 3) {
        y -= 1;
    }
    int dow = (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
    return days[dow];
}

static void calendar_event_cb(lv_event_t *e)
{
    lv_obj_t *cal = lv_event_get_current_target(e);
    lv_calendar_date_t d;
    if (lv_calendar_get_pressed_date(cal, &d) == LV_RES_OK) {
        if (d.year >= 2000U && d.year <= 2099U &&
            d.month >= 1 && d.month <= 12 &&
            d.day >= 1 && d.day <= 31) {
            s_year = (uint16_t)d.year;
            s_month = (uint8_t)d.month;
            s_day = (uint8_t)d.day;
            update_date_display();
        }
    }
}

static void btn_hour_up_cb(lv_event_t *e)
{
    (void)e;
    s_hour_12 = (s_hour_12 % 12U) + 1U;
    update_clock_display();
}

static void btn_hour_down_cb(lv_event_t *e)
{
    (void)e;
    s_hour_12 = (s_hour_12 == 1U) ? 12U : (s_hour_12 - 1U);
    update_clock_display();
}

static void btn_min_up_cb(lv_event_t *e)
{
    (void)e;
    s_minute = (s_minute + 1U) % 60U;
    update_clock_display();
}

static void btn_min_down_cb(lv_event_t *e)
{
    (void)e;
    s_minute = (s_minute == 0U) ? 59U : (s_minute - 1U);
    update_clock_display();
}

static void btn_am_cb(lv_event_t *e)
{
    (void)e;
    s_is_pm = false;
    update_clock_display();
}

static void btn_pm_cb(lv_event_t *e)
{
    (void)e;
    s_is_pm = true;
    update_clock_display();
}

static void tz_roller_event_cb(lv_event_t *e)
{
    lv_obj_t *roller = lv_event_get_target(e);
    uint16_t sel = lv_roller_get_selected(roller);
    if (sel < TZ_COUNT) {
        s_tz_idx = sel;
        update_tz_display();
    }
}

static void btn_next_cb(lv_event_t *e)
{
    (void)e;
    if (s_step < 2U) {
        s_step++;
        render_current_step();
    } else {
        rtc_datetime_t dt;
        dt.year = s_year;
        dt.month = s_month;
        dt.day = s_day;

        uint8_t h24 = s_hour_12 % 12U;
        if (s_is_pm) {
            h24 += 12U;
        }
        dt.hour = h24;
        dt.minute = s_minute;
        dt.second = 0U;
        dt.tz_offset_min = s_tz_list[s_tz_idx].offset_min;

        rtc_manager_set_datetime(&dt);
        ui_manager_navigate_to(SCREEN_LAUNCHER, LV_SCR_LOAD_ANIM_FADE_IN);
    }
}

static void btn_back_cb(lv_event_t *e)
{
    (void)e;
    if (s_step > 0U) {
        s_step--;
        render_current_step();
    }
}

static void update_date_display(void)
{
    if (s_date_val_lbl != NULL) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%04u-%02u-%02u", s_year, s_month, s_day);
        lv_label_set_text(s_date_val_lbl, buf);
    }
    if (s_date_dow_lbl != NULL) {
        lv_label_set_text(s_date_dow_lbl, get_weekday_str(s_year, s_month, s_day));
    }
}

static void update_clock_display(void)
{
    if (s_clock_hour_lbl != NULL) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%02u", (unsigned int)s_hour_12);
        lv_label_set_text(s_clock_hour_lbl, buf);
    }
    if (s_clock_min_lbl != NULL) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%02u", (unsigned int)s_minute);
        lv_label_set_text(s_clock_min_lbl, buf);
    }
    if (s_time_val_lbl != NULL) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%02u:%02u %s", (unsigned int)s_hour_12, (unsigned int)s_minute, s_is_pm ? "PM" : "AM");
        lv_label_set_text(s_time_val_lbl, buf);
    }
    if (s_time_fmt_lbl != NULL) {
        lv_label_set_text(s_time_fmt_lbl, "12-Hour Format");
    }
    if (s_btn_am != NULL) {
        if (!s_is_pm) {
            lv_obj_set_style_bg_color(s_btn_am, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
            lv_obj_set_style_text_color(s_btn_am, lv_color_hex(UI_COLOR_HEX_ON_PRIMARY), 0);
        } else {
            lv_obj_set_style_bg_color(s_btn_am, lv_color_hex(UI_COLOR_HEX_SURFACE_CONTAINER), 0);
            lv_obj_set_style_text_color(s_btn_am, lv_color_hex(UI_COLOR_HEX_TEXT_MUTED), 0);
        }
    }
    if (s_btn_pm != NULL) {
        if (s_is_pm) {
            lv_obj_set_style_bg_color(s_btn_pm, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
            lv_obj_set_style_text_color(s_btn_pm, lv_color_hex(UI_COLOR_HEX_ON_PRIMARY), 0);
        } else {
            lv_obj_set_style_bg_color(s_btn_pm, lv_color_hex(UI_COLOR_HEX_SURFACE_CONTAINER), 0);
            lv_obj_set_style_text_color(s_btn_pm, lv_color_hex(UI_COLOR_HEX_TEXT_MUTED), 0);
        }
    }
}

static void update_tz_display(void)
{
    if (s_tz_preview_lbl != NULL) {
        lv_label_set_text(s_tz_preview_lbl, s_tz_list[s_tz_idx].label);
    }
}

static void render_current_step(void)
{
    if (s_step == 0U) {
        lv_label_set_text(s_subtext_lbl, "Select a date");
        lv_obj_add_flag(s_btn_back, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_btn_next_lbl, "Next " LV_SYMBOL_RIGHT);
    } else if (s_step == 1U) {
        lv_label_set_text(s_subtext_lbl, "Select a time");
        lv_obj_clear_flag(s_btn_back, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_btn_next_lbl, "Next " LV_SYMBOL_RIGHT);
    } else {
        lv_label_set_text(s_subtext_lbl, "Select a timezone");
        lv_obj_clear_flag(s_btn_back, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_btn_next_lbl, "Save " LV_SYMBOL_OK);
    }

    lv_obj_clean(s_step_container);

    s_date_val_lbl = NULL;
    s_date_dow_lbl = NULL;
    s_time_val_lbl = NULL;
    s_time_fmt_lbl = NULL;
    s_clock_hour_lbl = NULL;
    s_clock_min_lbl = NULL;
    s_btn_am = NULL;
    s_btn_pm = NULL;
    s_tz_preview_lbl = NULL;

    if (s_step == 0U) {
        lv_obj_t *date_card = lv_obj_create(s_step_container);
        lv_obj_set_size(date_card, 154, 145);
        lv_obj_set_pos(date_card, 14, 60);
        lv_obj_set_style_bg_color(date_card, lv_color_hex(UI_COLOR_HEX_SURFACE_CONTAINER), 0);
        lv_obj_set_style_border_color(date_card, lv_color_hex(UI_COLOR_HEX_OUTLINE), 0);
        lv_obj_set_style_border_width(date_card, 1, 0);
        lv_obj_set_style_radius(date_card, 16, 0);
        lv_obj_set_style_pad_all(date_card, 10, 0);
        lv_obj_clear_flag(date_card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *hdr_lbl = lv_label_create(date_card);
        lv_label_set_text(hdr_lbl, "SELECTED DATE");
        lv_obj_set_style_text_font(hdr_lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(hdr_lbl, lv_color_hex(UI_COLOR_HEX_TEXT_MUTED), 0);
        lv_obj_set_pos(hdr_lbl, 4, 8);

        s_date_val_lbl = lv_label_create(date_card);
        lv_obj_set_style_text_font(s_date_val_lbl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(s_date_val_lbl, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
        lv_obj_set_pos(s_date_val_lbl, 4, 38);

        s_date_dow_lbl = lv_label_create(date_card);
        lv_obj_set_style_text_font(s_date_dow_lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_date_dow_lbl, lv_color_hex(UI_COLOR_HEX_TEXT_SECONDARY), 0);
        lv_obj_set_pos(s_date_dow_lbl, 4, 66);

        update_date_display();

        lv_obj_t *calendar = lv_calendar_create(s_step_container);
        lv_obj_set_size(calendar, 280, 206);
        lv_obj_set_pos(calendar, 184, 8);
        lv_obj_set_style_bg_color(calendar, lv_color_hex(UI_COLOR_HEX_SURFACE_CONTAINER), 0);
        lv_obj_set_style_border_color(calendar, lv_color_hex(UI_COLOR_HEX_OUTLINE), 0);
        lv_obj_set_style_border_width(calendar, 1, 0);
        lv_obj_set_style_radius(calendar, 14, 0);
        lv_obj_set_style_pad_all(calendar, 4, 0);

        lv_obj_t *header = lv_calendar_header_arrow_create(calendar);
        lv_obj_t *mo_prev = lv_obj_get_child(header, 0);
        lv_obj_t *header_lbl = lv_obj_get_child(header, 1);
        lv_obj_t *mo_next = lv_obj_get_child(header, 2);

        if (mo_prev != NULL) {
            lv_obj_set_style_bg_color(mo_prev, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
            lv_obj_set_style_bg_color(mo_prev, lv_color_hex(UI_COLOR_HEX_PRIMARY_CONTAINER), LV_STATE_PRESSED);
            lv_obj_set_style_bg_img_recolor(mo_prev, lv_color_hex(UI_COLOR_HEX_ON_PRIMARY), 0);
            lv_obj_set_style_bg_img_recolor_opa(mo_prev, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(mo_prev, 8, 0);
            lv_obj_set_style_shadow_width(mo_prev, 0, 0);
        }
        if (header_lbl != NULL) {
            lv_obj_set_style_text_color(header_lbl, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
            lv_obj_set_style_text_font(header_lbl, &lv_font_montserrat_14, 0);
        }
        if (mo_next != NULL) {
            lv_obj_set_style_bg_color(mo_next, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
            lv_obj_set_style_bg_color(mo_next, lv_color_hex(UI_COLOR_HEX_PRIMARY_CONTAINER), LV_STATE_PRESSED);
            lv_obj_set_style_bg_img_recolor(mo_next, lv_color_hex(UI_COLOR_HEX_ON_PRIMARY), 0);
            lv_obj_set_style_bg_img_recolor_opa(mo_next, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(mo_next, 8, 0);
            lv_obj_set_style_shadow_width(mo_next, 0, 0);
        }

        lv_calendar_set_today_date(calendar, s_year, s_month, s_day);
        lv_calendar_set_showed_date(calendar, s_year, s_month);

        lv_obj_t *btnm = lv_calendar_get_btnmatrix(calendar);
        lv_obj_set_style_text_font(btnm, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(btnm, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);

        lv_obj_add_event_cb(calendar, calendar_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    } else if (s_step == 1U) {
        lv_obj_t *time_card = lv_obj_create(s_step_container);
        lv_obj_set_size(time_card, 154, 145);
        lv_obj_set_pos(time_card, 14, 60);
        lv_obj_set_style_bg_color(time_card, lv_color_hex(UI_COLOR_HEX_SURFACE_CONTAINER), 0);
        lv_obj_set_style_border_color(time_card, lv_color_hex(UI_COLOR_HEX_OUTLINE), 0);
        lv_obj_set_style_border_width(time_card, 1, 0);
        lv_obj_set_style_radius(time_card, 16, 0);
        lv_obj_set_style_pad_all(time_card, 10, 0);
        lv_obj_clear_flag(time_card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *hdr_lbl = lv_label_create(time_card);
        lv_label_set_text(hdr_lbl, "SELECTED TIME");
        lv_obj_set_style_text_font(hdr_lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(hdr_lbl, lv_color_hex(UI_COLOR_HEX_TEXT_MUTED), 0);
        lv_obj_set_pos(hdr_lbl, 4, 8);

        s_time_val_lbl = lv_label_create(time_card);
        lv_obj_set_style_text_font(s_time_val_lbl, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(s_time_val_lbl, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
        lv_obj_set_pos(s_time_val_lbl, 4, 38);

        s_time_fmt_lbl = lv_label_create(time_card);
        lv_obj_set_style_text_font(s_time_fmt_lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_time_fmt_lbl, lv_color_hex(UI_COLOR_HEX_TEXT_SECONDARY), 0);
        lv_obj_set_pos(s_time_fmt_lbl, 4, 66);

        const lv_coord_t start_x = 200;
        const lv_coord_t top_y = 38;

        lv_obj_t *btn_h_up = lv_btn_create(s_step_container);
        lv_obj_set_size(btn_h_up, 78, 32);
        lv_obj_set_pos(btn_h_up, start_x, top_y);
        lv_obj_set_style_bg_color(btn_h_up, lv_color_hex(UI_COLOR_HEX_SURFACE_HIGH), 0);
        lv_obj_set_style_bg_color(btn_h_up, lv_color_hex(UI_COLOR_HEX_PRIMARY), LV_STATE_PRESSED);
        lv_obj_set_style_radius(btn_h_up, 10, 0);
        lv_obj_set_style_shadow_width(btn_h_up, 0, 0);
        lv_obj_add_event_cb(btn_h_up, btn_hour_up_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *lbl_h_up = lv_label_create(btn_h_up);
        lv_label_set_text(lbl_h_up, LV_SYMBOL_UP);
        lv_obj_set_style_text_color(lbl_h_up, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
        lv_obj_center(lbl_h_up);

        lv_obj_t *card_hour = lv_obj_create(s_step_container);
        lv_obj_set_size(card_hour, 78, 64);
        lv_obj_set_pos(card_hour, start_x, top_y + 38);
        lv_obj_set_style_bg_color(card_hour, lv_color_hex(UI_COLOR_HEX_SURFACE_CONTAINER), 0);
        lv_obj_set_style_border_color(card_hour, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
        lv_obj_set_style_border_width(card_hour, 2, 0);
        lv_obj_set_style_radius(card_hour, 16, 0);
        lv_obj_set_style_pad_all(card_hour, 0, 0);
        lv_obj_clear_flag(card_hour, LV_OBJ_FLAG_SCROLLABLE);

        s_clock_hour_lbl = lv_label_create(card_hour);
        lv_obj_set_style_text_font(s_clock_hour_lbl, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(s_clock_hour_lbl, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
        lv_obj_center(s_clock_hour_lbl);

        lv_obj_t *btn_h_down = lv_btn_create(s_step_container);
        lv_obj_set_size(btn_h_down, 78, 32);
        lv_obj_set_pos(btn_h_down, start_x, top_y + 108);
        lv_obj_set_style_bg_color(btn_h_down, lv_color_hex(UI_COLOR_HEX_SURFACE_HIGH), 0);
        lv_obj_set_style_bg_color(btn_h_down, lv_color_hex(UI_COLOR_HEX_PRIMARY), LV_STATE_PRESSED);
        lv_obj_set_style_radius(btn_h_down, 10, 0);
        lv_obj_set_style_shadow_width(btn_h_down, 0, 0);
        lv_obj_add_event_cb(btn_h_down, btn_hour_down_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *lbl_h_down = lv_label_create(btn_h_down);
        lv_label_set_text(lbl_h_down, LV_SYMBOL_DOWN);
        lv_obj_set_style_text_color(lbl_h_down, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
        lv_obj_center(lbl_h_down);

        lv_obj_t *colon_lbl = lv_label_create(s_step_container);
        lv_label_set_text(colon_lbl, ":");
        lv_obj_set_style_text_font(colon_lbl, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(colon_lbl, lv_color_hex(UI_COLOR_HEX_TEXT_SECONDARY), 0);
        lv_obj_set_pos(colon_lbl, start_x + 84, top_y + 54);

        const lv_coord_t min_x = start_x + 96;

        lv_obj_t *btn_m_up = lv_btn_create(s_step_container);
        lv_obj_set_size(btn_m_up, 78, 32);
        lv_obj_set_pos(btn_m_up, min_x, top_y);
        lv_obj_set_style_bg_color(btn_m_up, lv_color_hex(UI_COLOR_HEX_SURFACE_HIGH), 0);
        lv_obj_set_style_bg_color(btn_m_up, lv_color_hex(UI_COLOR_HEX_PRIMARY), LV_STATE_PRESSED);
        lv_obj_set_style_radius(btn_m_up, 10, 0);
        lv_obj_set_style_shadow_width(btn_m_up, 0, 0);
        lv_obj_add_event_cb(btn_m_up, btn_min_up_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *lbl_m_up = lv_label_create(btn_m_up);
        lv_label_set_text(lbl_m_up, LV_SYMBOL_UP);
        lv_obj_set_style_text_color(lbl_m_up, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
        lv_obj_center(lbl_m_up);

        lv_obj_t *card_min = lv_obj_create(s_step_container);
        lv_obj_set_size(card_min, 78, 64);
        lv_obj_set_pos(card_min, min_x, top_y + 38);
        lv_obj_set_style_bg_color(card_min, lv_color_hex(UI_COLOR_HEX_SURFACE_CONTAINER), 0);
        lv_obj_set_style_border_color(card_min, lv_color_hex(UI_COLOR_HEX_OUTLINE), 0);
        lv_obj_set_style_border_width(card_min, 1, 0);
        lv_obj_set_style_radius(card_min, 16, 0);
        lv_obj_set_style_pad_all(card_min, 0, 0);
        lv_obj_clear_flag(card_min, LV_OBJ_FLAG_SCROLLABLE);

        s_clock_min_lbl = lv_label_create(card_min);
        lv_obj_set_style_text_font(s_clock_min_lbl, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(s_clock_min_lbl, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
        lv_obj_center(s_clock_min_lbl);

        lv_obj_t *btn_m_down = lv_btn_create(s_step_container);
        lv_obj_set_size(btn_m_down, 78, 32);
        lv_obj_set_pos(btn_m_down, min_x, top_y + 108);
        lv_obj_set_style_bg_color(btn_m_down, lv_color_hex(UI_COLOR_HEX_SURFACE_HIGH), 0);
        lv_obj_set_style_bg_color(btn_m_down, lv_color_hex(UI_COLOR_HEX_PRIMARY), LV_STATE_PRESSED);
        lv_obj_set_style_radius(btn_m_down, 10, 0);
        lv_obj_set_style_shadow_width(btn_m_down, 0, 0);
        lv_obj_add_event_cb(btn_m_down, btn_min_down_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *lbl_m_down = lv_label_create(btn_m_down);
        lv_label_set_text(lbl_m_down, LV_SYMBOL_DOWN);
        lv_obj_set_style_text_color(lbl_m_down, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
        lv_obj_center(lbl_m_down);

        const lv_coord_t ampm_x = min_x + 88;
        lv_obj_t *ampm_card = lv_obj_create(s_step_container);
        lv_obj_set_size(ampm_card, 54, 64);
        lv_obj_set_pos(ampm_card, ampm_x, top_y + 38);
        lv_obj_set_style_bg_color(ampm_card, lv_color_hex(UI_COLOR_HEX_SURFACE_CONTAINER), 0);
        lv_obj_set_style_border_color(ampm_card, lv_color_hex(UI_COLOR_HEX_OUTLINE), 0);
        lv_obj_set_style_border_width(ampm_card, 1, 0);
        lv_obj_set_style_radius(ampm_card, 14, 0);
        lv_obj_set_style_pad_all(ampm_card, 0, 0);
        lv_obj_clear_flag(ampm_card, LV_OBJ_FLAG_SCROLLABLE);

        s_btn_am = lv_btn_create(ampm_card);
        lv_obj_set_size(s_btn_am, 52, 31);
        lv_obj_set_pos(s_btn_am, 0, 0);
        lv_obj_set_style_radius(s_btn_am, 12, 0);
        lv_obj_set_style_pad_all(s_btn_am, 0, 0);
        lv_obj_set_style_shadow_width(s_btn_am, 0, 0);
        lv_obj_add_event_cb(s_btn_am, btn_am_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *lbl_am = lv_label_create(s_btn_am);
        lv_label_set_text(lbl_am, "AM");
        lv_obj_set_style_text_font(lbl_am, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl_am);

        s_btn_pm = lv_btn_create(ampm_card);
        lv_obj_set_size(s_btn_pm, 52, 31);
        lv_obj_set_pos(s_btn_pm, 0, 31);
        lv_obj_set_style_radius(s_btn_pm, 12, 0);
        lv_obj_set_style_pad_all(s_btn_pm, 0, 0);
        lv_obj_set_style_shadow_width(s_btn_pm, 0, 0);
        lv_obj_add_event_cb(s_btn_pm, btn_pm_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *lbl_pm = lv_label_create(s_btn_pm);
        lv_label_set_text(lbl_pm, "PM");
        lv_obj_set_style_text_font(lbl_pm, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl_pm);

        update_clock_display();

    } else {
        static char opts_str[1024];
        opts_str[0] = '\0';
        for (uint16_t i = 0; i < TZ_COUNT; i++) {
            strcat(opts_str, s_tz_list[i].label);
            if (i < TZ_COUNT - 1U) {
                strcat(opts_str, "\n");
            }
        }

        lv_obj_t *tz_roller = lv_roller_create(s_step_container);
        lv_obj_set_size(tz_roller, 380, 115);
        lv_obj_set_pos(tz_roller, 50, 52);
        lv_roller_set_options(tz_roller, opts_str, LV_ROLLER_MODE_NORMAL);
        lv_roller_set_visible_row_count(tz_roller, 3);
        lv_roller_set_selected(tz_roller, s_tz_idx, LV_ANIM_OFF);

        lv_obj_set_style_bg_color(tz_roller, lv_color_hex(UI_COLOR_HEX_SURFACE_CONTAINER), 0);
        lv_obj_set_style_border_color(tz_roller, lv_color_hex(UI_COLOR_HEX_OUTLINE), 0);
        lv_obj_set_style_border_width(tz_roller, 1, 0);
        lv_obj_set_style_radius(tz_roller, 14, 0);
        lv_obj_set_style_text_font(tz_roller, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(tz_roller, lv_color_hex(UI_COLOR_HEX_TEXT_SECONDARY), 0);

        lv_obj_set_style_bg_color(tz_roller, lv_color_hex(UI_COLOR_HEX_PRIMARY_CONTAINER), LV_PART_SELECTED);
        lv_obj_set_style_text_color(tz_roller, lv_color_hex(UI_COLOR_HEX_ON_PRIMARY_CONT), LV_PART_SELECTED);
        lv_obj_set_style_text_font(tz_roller, &lv_font_montserrat_12, LV_PART_SELECTED);

        lv_obj_add_event_cb(tz_roller, tz_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

        lv_obj_t *tz_preview_card = lv_obj_create(s_step_container);
        lv_obj_set_size(tz_preview_card, 380, 44);
        lv_obj_set_pos(tz_preview_card, 50, 172);
        lv_obj_set_style_bg_color(tz_preview_card, lv_color_hex(UI_COLOR_HEX_SURFACE_CONTAINER), 0);
        lv_obj_set_style_border_color(tz_preview_card, lv_color_hex(UI_COLOR_HEX_OUTLINE), 0);
        lv_obj_set_style_border_width(tz_preview_card, 1, 0);
        lv_obj_set_style_radius(tz_preview_card, 12, 0);
        lv_obj_set_style_pad_all(tz_preview_card, 4, 0);
        lv_obj_clear_flag(tz_preview_card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *sel_hdr = lv_label_create(tz_preview_card);
        lv_label_set_text(sel_hdr, "Selected:");
        lv_obj_set_style_text_font(sel_hdr, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(sel_hdr, lv_color_hex(UI_COLOR_HEX_TEXT_MUTED), 0);
        lv_obj_align(sel_hdr, LV_ALIGN_TOP_MID, 0, 1);

        s_tz_preview_lbl = lv_label_create(tz_preview_card);
        lv_obj_set_style_text_font(s_tz_preview_lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(s_tz_preview_lbl, lv_color_hex(UI_COLOR_HEX_PRIMARY), 0);
        lv_obj_align(s_tz_preview_lbl, LV_ALIGN_BOTTOM_MID, 0, -1);

        update_tz_display();
    }
}

lv_obj_t *screen_setup_rtclock_create(void)
{
    s_step = 0U;

    rtc_datetime_t dt;
    rtc_manager_get_datetime(&dt);
    s_year = dt.year;
    s_month = dt.month;
    s_day = dt.day;
    s_hour_12 = dt.hour % 12U;
    if (s_hour_12 == 0U) s_hour_12 = 12U;
    s_is_pm = (dt.hour >= 12U);
    s_minute = dt.minute;
    s_tz_idx = TZ_DEFAULT_INDEX;

    s_root_scr = lv_obj_create(NULL);
    lv_obj_set_size(s_root_scr, 480, 272);
    lv_obj_set_style_bg_color(s_root_scr, lv_color_hex(UI_COLOR_HEX_SURFACE), 0);
    lv_obj_set_style_bg_opa(s_root_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_root_scr, 0, 0);
    lv_obj_clear_flag(s_root_scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *headline_lbl = lv_label_create(s_root_scr);
    lv_label_set_text(headline_lbl, "Let's Get Started");
    lv_obj_set_style_text_font(headline_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(headline_lbl, lv_color_hex(UI_COLOR_HEX_TEXT_PRIMARY), 0);
    lv_obj_set_pos(headline_lbl, 14, 10);

    s_subtext_lbl = lv_label_create(s_root_scr);
    lv_label_set_text(s_subtext_lbl, "Select a date");
    lv_obj_set_style_text_font(s_subtext_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_subtext_lbl, lv_color_hex(UI_COLOR_HEX_TEXT_SECONDARY), 0);
    lv_obj_set_pos(s_subtext_lbl, 14, 32);

    s_step_container = lv_obj_create(s_root_scr);
    lv_obj_set_size(s_step_container, 480, 218);
    lv_obj_set_pos(s_step_container, 0, 0);
    lv_obj_set_style_bg_opa(s_step_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_step_container, 0, 0);
    lv_obj_set_style_pad_all(s_step_container, 0, 0);
    lv_obj_clear_flag(s_step_container, LV_OBJ_FLAG_SCROLLABLE);

    s_btn_back = lv_btn_create(s_root_scr);
    lv_obj_add_style(s_btn_back, ui_style_btn_outline(), 0);
    lv_obj_set_size(s_btn_back, 78, 36);
    lv_obj_set_pos(s_btn_back, 14, 224);
    lv_obj_add_event_cb(s_btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_lbl = lv_label_create(s_btn_back);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(back_lbl);

    s_btn_next = lv_btn_create(s_root_scr);
    lv_obj_add_style(s_btn_next, ui_style_btn_primary(), 0);
    lv_obj_set_size(s_btn_next, 94, 36);
    lv_obj_set_pos(s_btn_next, 372, 224);
    lv_obj_add_event_cb(s_btn_next, btn_next_cb, LV_EVENT_CLICKED, NULL);

    s_btn_next_lbl = lv_label_create(s_btn_next);
    lv_label_set_text(s_btn_next_lbl, "Next " LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(s_btn_next_lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(s_btn_next_lbl);

    render_current_step();

    return s_root_scr;
}
