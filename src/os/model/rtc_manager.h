/*!
    \file    src/os/model/rtc_manager.h
    \brief   AeroDENS RTC Hardware & Timezone Management Driver
             Target: GD32H759I-EVAL (Internal RTC with Backup Domain Persistence)
*/

#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define RTC_MAGIC_VALID         0x5A5A4837U
#define DEFAULT_TZ_OFFSET_MIN   480 /* GMT+8:00 (Asia/Manila, PHT) */

typedef struct {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    int16_t  tz_offset_min;
} rtc_datetime_t;

void rtc_manager_init(void);
bool rtc_manager_is_valid(void);
void rtc_manager_set_datetime(const rtc_datetime_t *dt);
void rtc_manager_get_datetime(rtc_datetime_t *dt);
void rtc_manager_get_time_str(char *buf, size_t max_len);
void rtc_manager_second_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* RTC_MANAGER_H */

