/*!
    \file    src/os/model/rtc_manager.c
    \brief   AeroDENS RTC Hardware & Timezone Management Driver Implementation
             Target: GD32H759I-EVAL (Internal RTC with Backup Domain Persistence)
*/

#include "rtc_manager.h"
#include "gd32h7xx.h"
#include <stdio.h>
#include <string.h>

static rtc_datetime_t s_current_dt = {
    .year = 2026,
    .month = 11,
    .day = 1,
    .hour = 8,
    .minute = 45,
    .second = 0,
    .tz_offset_min = DEFAULT_TZ_OFFSET_MIN
};

static inline uint8_t dec_to_bcd(uint8_t dec)
{
    return (uint8_t)(((dec / 10U) << 4) | (dec % 10U));
}

static inline uint8_t bcd_to_dec(uint8_t bcd)
{
    return (uint8_t)(((bcd >> 4) * 10U) + (bcd & 0x0FU));
}

static const char *get_weekday_short(uint16_t y, uint8_t m, uint8_t d)
{
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    static const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    if (m < 1U || m > 12U) {
        return "Mon";
    }
    if (m < 3U) {
        y -= 1U;
    }
    int dow = (y + y / 4U - y / 100U + y / 400U + t[m - 1U] + d) % 7;
    return days[dow];
}

static const char *get_month_short(uint8_t m)
{
    static const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    if (m < 1U || m > 12U) {
        return "Jan";
    }
    return months[m - 1U];
}

static uint8_t days_in_month(uint16_t y, uint8_t m)
{
    static const uint8_t dim[] = {31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};
    if (m < 1U || m > 12U) {
        return 30U;
    }
    if (m == 2U && ((y % 4U == 0U && y % 100U != 0U) || (y % 400U == 0U))) {
        return 29U;
    }
    return dim[m - 1U];
}

void rtc_manager_init(void)
{
    rcu_periph_clock_enable(RCU_PMU);
    pmu_backup_write_enable();

    rcu_osci_on(RCU_IRC32K);
    uint32_t timeout = 0x80000U;
    while ((rcu_flag_get(RCU_FLAG_IRC32KSTB) == RESET) && (--timeout > 0U)) {
    }

    rcu_rtc_clock_config(RCU_RTCSRC_IRC32K);
    rcu_periph_clock_enable(RCU_RTC);
    rtc_register_sync_wait();

    if (rtc_manager_is_valid()) {
        rtc_manager_get_datetime(&s_current_dt);
    }
}

bool rtc_manager_is_valid(void)
{
    return (RTC_BKP0 == RTC_MAGIC_VALID);
}

void rtc_manager_set_datetime(const rtc_datetime_t *dt)
{
    if (dt == NULL) {
        return;
    }

    rcu_periph_clock_enable(RCU_PMU);
    pmu_backup_write_enable();

    rtc_parameter_struct rtc_initpara;
    /* 32 kHz / ((127 + 1) * (249 + 1)) = 1 Hz */
    rtc_initpara.factor_asyn = 0x7FU;
    rtc_initpara.factor_syn = 0xF9U;
    rtc_initpara.year = dec_to_bcd((uint8_t)(dt->year >= 2000 ? (dt->year - 2000) : dt->year));
    rtc_initpara.month = dec_to_bcd(dt->month);
    rtc_initpara.date = dec_to_bcd(dt->day);
    rtc_initpara.day_of_week = RTC_MONDAY;
    rtc_initpara.hour = dec_to_bcd(dt->hour);
    rtc_initpara.minute = dec_to_bcd(dt->minute);
    rtc_initpara.second = dec_to_bcd(dt->second);
    rtc_initpara.display_format = RTC_24HOUR;
    rtc_initpara.am_pm = (dt->hour >= 12) ? RTC_PM : RTC_AM;

    rtc_init(&rtc_initpara);

    RTC_WPK = RTC_UNLOCK_KEY1;
    RTC_WPK = RTC_UNLOCK_KEY2;
    RTC_BKP0 = RTC_MAGIC_VALID;
    RTC_BKP1 = (uint32_t)(int32_t)dt->tz_offset_min;
    RTC_BKP2 = (uint32_t)dt->year;
    RTC_WPK = RTC_LOCK_KEY;

    memcpy(&s_current_dt, dt, sizeof(rtc_datetime_t));
}

void rtc_manager_get_datetime(rtc_datetime_t *dt)
{
    if (dt == NULL) {
        return;
    }

    if (rtc_manager_is_valid()) {
        rtc_parameter_struct rtc_para;
        rtc_current_time_get(&rtc_para);

        uint16_t bkp_year = (uint16_t)RTC_BKP2;
        if (bkp_year >= 2000U && bkp_year <= 2099U) {
            dt->year = bkp_year;
        } else {
            dt->year = 2000U + bcd_to_dec(rtc_para.year);
        }
        dt->month = bcd_to_dec(rtc_para.month);
        dt->day = bcd_to_dec(rtc_para.date);
        dt->hour = bcd_to_dec(rtc_para.hour);
        dt->minute = bcd_to_dec(rtc_para.minute);
        dt->second = bcd_to_dec(rtc_para.second);
        dt->tz_offset_min = (int16_t)(int32_t)RTC_BKP1;
        memcpy(&s_current_dt, dt, sizeof(rtc_datetime_t));
    } else {
        memcpy(dt, &s_current_dt, sizeof(rtc_datetime_t));
    }
}

void rtc_manager_second_tick(void)
{
    if (rtc_manager_is_valid()) {
        rtc_manager_get_datetime(&s_current_dt);
    } else {
        s_current_dt.second++;
        if (s_current_dt.second >= 60U) {
            s_current_dt.second = 0U;
            s_current_dt.minute++;
            if (s_current_dt.minute >= 60U) {
                s_current_dt.minute = 0U;
                s_current_dt.hour++;
                if (s_current_dt.hour >= 24U) {
                    s_current_dt.hour = 0U;
                    s_current_dt.day++;
                    if (s_current_dt.day > days_in_month(s_current_dt.year, s_current_dt.month)) {
                        s_current_dt.day = 1U;
                        s_current_dt.month++;
                        if (s_current_dt.month > 12U) {
                            s_current_dt.month = 1U;
                            s_current_dt.year++;
                        }
                    }
                }
            }
        }
    }
}

void rtc_manager_get_time_str(char *buf, size_t max_len)
{
    if (buf == NULL || max_len < 24U) {
        return;
    }

    uint8_t h12 = s_current_dt.hour % 12U;
    if (h12 == 0U) {
        h12 = 12U;
    }
    const char *ampm = (s_current_dt.hour >= 12U) ? "PM" : "AM";
    const char *dow_str = get_weekday_short(s_current_dt.year, s_current_dt.month, s_current_dt.day);
    const char *mon_str = get_month_short(s_current_dt.month);

    snprintf(buf, max_len, "%02u:%02u:%02u %s %s, %s %02u",
             (unsigned int)h12,
             (unsigned int)s_current_dt.minute,
             (unsigned int)s_current_dt.second,
             ampm,
             dow_str,
             mon_str,
             (unsigned int)s_current_dt.day);
}

